#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <sys/wait.h>
#include <filesystem>
#include <fcntl.h>
#ifdef _WIN32 // Identifies if os is Windows
    #include <io.h>
#else // If not Windows then os is Linux
    #include <unistd.h>
#endif
std::vector<std::string> tokenize(std::string &input)
{
  std::vector<std::string> args;
  std::string curr;
  bool singlequote=false;
  bool doublequote=false;
  for(int i=0;i<input.size();i++)
  {
    char c=input[i];
    if(!singlequote&&c=='\\'&&i+1<input.size())
    {
      curr=curr+input[++i];
      continue;
    }
    else if(c=='\''&&!doublequote)
    {
      singlequote=!singlequote;
    }
    else if(c=='\"'&&!singlequote)
    {
      doublequote=!doublequote;
    }
    else if(c==' '&&!singlequote&&!doublequote)
    {
      if(!curr.empty())
      {
        args.push_back(curr);
        curr.clear();
      }
    }
    else
    {
      curr+=c;
    }
  }
  if(!curr.empty())
  {
    args.push_back(curr);
  }
  return args;
}
void cd(std::string path)
{
  if(path=="~")
  {
    #ifdef _WIN32
    const char *home=getenv("USERPROFILE");
    #else
    const char *home=getenv("HOME");
    #endif
    if(home)
    {
      path=home;
    }
  }
  try{
    std::filesystem::current_path(path);
  }
  catch(std::filesystem::filesystem_error&){
    std::cout<<"cd: "<<path<<": No such file or directory\n";
  }
}
void echo(std::vector<std::string> &args)
{

  for(int i=1;i<args.size();i++)
  {
    if(i>1)
    {
      std::cout<<' ';
    }
    std::cout<<args[i];
  }
  std::cout<<'\n';
}
void pwd()
{
  // Works for both Linux and Windows systems
  std::cout<<std::filesystem::current_path().string()<<"\n";
}
std::string get_command_path(const std::string &user_input)
{
  char* path=getenv("PATH");
  if(!path)
  {
    return "";
  }
      std::istringstream ss(path);
      std::string dir;
      #ifdef _WIN32
      while(getline(ss,dir,';'))
      {
        std::string full_path=dir+"/"+user_input;
        if(!_access(full_path.c_str(),0))
        {
          return full_path;
        }
      }
      #else
      while(getline(ss,dir,':'))
      {
        std::string full_path=dir+"/"+user_input;
        if(!access(full_path.c_str(),X_OK))
        {
          return full_path;
        }
      }
      #endif
      return "";
    }
int execute_file(std::string user_input)
{
  std::vector<std::string> args= tokenize(user_input);
  std::string outfile;
  bool redirect=false;
  if(args.empty())
  {
    return 0;
  }
  for(int i=0;i<args.size();i++)
  {
    if(args[i]==">"||args[i]=="1>")
    {
      redirect=true;
      outfile=args[i+1];
      args.resize(i);
      break;
    }
  }
  std::string path=get_command_path(args[0]);
  if(path.empty())
  {
    std::cout<<args[0]<<": command not found"<<'\n';
    return 1;
  }
  std::vector<char *> argv;
  for(auto &arg:args)
  {
    argv.push_back(arg.data());
  }
  argv.push_back(nullptr);
  int fd=-1;
  if(redirect)
  {
    fd=open(outfile.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd==-1)
  {
    perror("open");
    return 1;
  }
  }
  
  pid_t pid=fork();
  if(pid==-1)
  {
    perror("fork");
    return 1;
  }
  if(pid==0)
  {
     if (redirect)
    {
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
     execv(path.c_str(), argv.data());
        perror("execv");
        _exit(1);
  }
  if(redirect)
  {
    close(fd);
  }
   int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
}
void redirect_stdout_begin(int &saved_stdout, int &fd, const std::string &outfile)
{
    saved_stdout = dup(STDOUT_FILENO);

    fd = open(outfile.c_str(),
              O_WRONLY | O_CREAT | O_TRUNC,
              0644);

    dup2(fd, STDOUT_FILENO);
}

void redirect_stdout_end(int saved_stdout, int fd)
{
    fflush(stdout);

    dup2(saved_stdout, STDOUT_FILENO);

    close(saved_stdout);
    close(fd);
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::vector<std::string> builtin;
  builtin.push_back("exit");
  builtin.push_back("echo");
  builtin.push_back("type");
  builtin.push_back("pwd");
  do
  {
  std::cout << "$ ";
  std::string user_input;
  std::getline(std::cin,user_input);
  std::vector<std::string> args = tokenize(user_input);
  bool redirect = false;
  std::string outfile;
  for(int i=0;i<args.size();i++)
  {
    if(args[i]==">"||args[i]=="1>")
    {
        redirect=true;
        outfile=args[i+1];
        args.resize(i);
        break;
    } 
  }
  if(args.empty())
  {
    continue;
  }
  if(args[0]=="exit")
  {
    break;
  }
  else if(args[0]=="pwd")
  {
    int saved_stdout=-1;
    int fd=-1;
    if(redirect)
    redirect_stdout_begin(saved_stdout, fd, outfile);
    pwd();
    if(redirect)
    redirect_stdout_end(saved_stdout, fd);
  }
  else if(args[0]=="cd")
  {
    if(args.size()>1)
    cd(args[1]);
  }
  else if(args[0]=="echo")
  {
    int saved_stdout=-1;
    int fd=-1;

    if(redirect)
    {
      redirect_stdout_begin(saved_stdout, fd, outfile);
    }
    echo(args);

    if (redirect)
      redirect_stdout_end(saved_stdout, fd);
  }
  else if(args[0]=="type")
  {
    int saved_stdout = -1;
    int fd = -1;

    if (redirect)
    redirect_stdout_begin(saved_stdout, fd, outfile);
    if(args.size()<2)
    {
      continue;
    }
    std::string command=args[1];
    bool type_found=std::find(builtin.begin(),builtin.end(),command)!=builtin.end();
    if(type_found)
      std::cout<<command<<" is a shell builtin"<<"\n";
    else
    {
      std::string full_path=get_command_path(command);
      if(!full_path.empty())
      {
        std::cout<<command<<" is "<<full_path<<"\n";
        type_found=true;
      }
    }
    if(!type_found)
    {
      std::cout<<command<<": not found\n";
    }

  if (redirect)
    redirect_stdout_end(saved_stdout, fd);
  }
  else
  {
    execute_file(user_input);
  }
}
  while(true);
}
