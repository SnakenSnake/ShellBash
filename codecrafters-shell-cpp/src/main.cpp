#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <sys/wait.h>
#include <filesystem>
#ifdef _WIN32 // Identifies if os is Windows
    #include <io.h>
#else // If not Windows then os is Linux
    #include <unistd.h>
#endif
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
  std::istringstream ss(user_input);
  std::vector<std::string> args;
  std::string token;
  while(ss>>token)
  {
    args.push_back(token);
  }
  if(args.empty())
  {
    return 0;
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
  pid_t pid=fork();
  if(pid==-1)
  {
    perror("fork");
    return 1;
  }
  if(pid==0)
  {
     execv(path.c_str(), argv.data());
        perror("execv");
        _exit(1);
  }
   int status;
    waitpid(pid, &status, 0);
    return WEXITSTATUS(status);
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
  if(user_input=="exit")
  {
    break;
  }
  else if(user_input=="pwd")
  {
    pwd();
  }
  else if(user_input.substr(0,2)=="cd")
  {
    cd(user_input.substr(3));
  }
  else if(user_input.substr(0,5)=="echo ")
  {
    std::cout<<user_input.substr(5)<<'\n';
  }
  else if(user_input.substr(0,5)=="type ")
  {
    std::string command=user_input.substr(5);
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
  }
  else
  {
    execute_file(user_input);
  }
}
  while(true);
}
