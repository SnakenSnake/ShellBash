#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <sys/wait.h>
#include <filesystem>
#include <fcntl.h>
#include <termios.h>
#include <set>
#include <map>
#include <iomanip>
#include <fstream>    
#ifdef _WIN32 // Identifies if os is Windows
    #include <io.h>
#else // If not Windows then os is Linux
    #include <unistd.h>
#endif
  int nextJob=1; 
  int historyIndex=0;
  size_t lastsaved=0;
  std::vector<std::string> history;
  std::map<std::string,std::string> completeC;
  std::map<std::string,std::string> declarevar;
  std::string history_file;
  std::vector<std::string> builtin={"echo","exit","pwd","cd","type","complete","jobs","history","declare"};
  namespace fs=std::filesystem;
struct Redirection{
  int fd;
  std::string filename;
  bool append;
};
struct Job{
  int job_no;
  pid_t PID;
  std::string cmd;
  std::string status;
};
std::vector<Job> jobs;
bool isvalididentifier(std::string s)
{
  if(s.empty())
  {
    return false;
  }
  if(s[0]!='_'&&!(std::isalpha(s[0])))
  return false;
  for (char c:s)
  {
    if(!std::isalnum(c)&&c!='_')
    {
      return false;
    }
  }
  return true;
}
void declare(std::vector<std::string> &args)
{
  if(args.size()==3&&args[1]=="-p")
  {
    if(declarevar.find(args[2])==declarevar.end())
    {
    std::cerr<<"declare: "<<args[2]<<": not found\n";
    return;
    }
    else
    {
      std::cout<<"declare -- "<<args[2]<<"="<<"\""<<declarevar[args[2]]<<"\""<<'\n';
    }
  }
  if(args.size()==2)
  {
    size_t pos=args[1].find('=');
    if(pos!=std::string::npos)
    {
      std::string name=args[1].substr(0,pos);
      std::string val=args[1].substr(pos+1);
      if(isvalididentifier(name))
      declarevar[name]=val;
      else
      std::cout<<"declare: `"<<args[1]<<"\': not a valid identifier"<<'\n';
    }
  }
}
void history_boot()
{
  const char *histfile=getenv("HISTFILE");
    if(histfile!=nullptr)
    {
      history_file=histfile;
      std::ifstream fin(histfile);
      std::string line;
      while(std::getline(fin,line))
        {
            if (!line.empty())
              history.push_back(line);
        }
        lastsaved = history.size();
    }
}
void history_read(std::string &filename)
{
  std::ifstream fin(filename);
  if(!fin.is_open())
  {
    std::cerr<<"history: "<<filename<<": No such file or directory\n";
    return;
  }
  std::string line;
  while(std::getline(fin,line))
  {
    if(!line.empty())
    {
      history.push_back(line);
    }
  }
  fin.close();
}
void history_write(std::string &filename)
{
  if(filename.empty())
  {
    return;
  }
  std::ofstream fout(filename);
  if(!fout.is_open())
  {
    std::cerr<<"history: "<<filename<<": Cannot open file\n";
    return;
  }
  for(auto s:history)
  {
    fout<<s<<'\n';
  }
  fout.close();
}
void history_append(std::string filename)
{
  if(filename.empty())
  {
    return;
  }
  std::ofstream fout(filename,std::ios::app);
  if(!fout.is_open())
  {
     std::cerr<<"history: "<<filename<<": Cannot open file\n";
    return;
  }
  for(int i=lastsaved;i<history.size();i++)
  {
    fout<<history[i]<<'\n';
  }
  lastsaved=history.size();
  fout.close();
}
int getjobno()
{
  if(jobs.empty())
  {
    return 1;
  }
  int maxi=0;
 
    for(auto &job:jobs)
    {
      maxi=std::max(maxi,job.job_no);
    }
  return maxi+1;
}
void jobs_func(std::vector<std::string> &args)
{
  std::vector<Job> remJobs;
  for(int i=0;i<jobs.size();i++)
  {
    int status;
    pid_t result=waitpid(jobs[i].PID,&status,WNOHANG);
    if(result==0)
    {
      jobs[i].status="Running";
    }
    else if(result==jobs[i].PID)
    {
      jobs[i].status="Done";
    }
    else
    {
      perror("waitpid");
    }
    std::cout<<"["<<jobs[i].job_no<<"]";
    if(i==jobs.size()-1)
    {
      std::cout<<"+";
    }
    else if(jobs.size()>=2&&i==jobs.size()-2)
    {
      std::cout<<"-";
    }
    else{
      std::cout<<" ";
    }
    std::cout<<"  ";
    std::cout<<std::left<<std::setw(24)<<jobs[i].status;
    std::cout<<jobs[i].cmd<<'\n';
    if(jobs[i].status=="Running")
    {
      remJobs.push_back(jobs[i]);
    }
  }
  jobs=remJobs;
}
void reapJobs()
{
  std::vector<Job> rem;
  for(int i=0;i<jobs.size();i++)
  {
  int status;
  pid_t pid=waitpid(jobs[i].PID,&status,WNOHANG);
  if(pid==0)
  {
    rem.push_back(jobs[i]);
  }
  else if(pid==jobs[i].PID)
  {
    char marker = ' ';
    if (i==jobs.size()-1)
        marker='+';
    else if(i==jobs.size()-2)
        marker='-';
            std::cout<<"["<<jobs[i].job_no<<"]"<<marker<<"  "<<std::left<<std::setw(20)<<"Done"
                      <<jobs[i].cmd<<'\n';
  }
  }
  jobs=rem;
}
void complete(std::vector<std::string> &args)
{
  if(args.size()==4&&args[1]=="-C")
  {
    completeC[args[3]]=args[2];
  }
  if(args.size()==3&&args[1]=="-p")
  {
    auto it=completeC.find(args[2]);
    if(it==completeC.end())
    {
      std::cerr<<"complete: "<<args[2]<<": no completion specification"<<'\n';
    }
    else
    {
      std::cout<<"complete -C '"<<it->second<<"' "<<args[2]<<'\n';
    }
  }
  if(args.size()==3&&args[1]=="-r")
  {
    completeC.erase(args[2]);
  }
}
std::vector<int> apply_redirections(std::vector<Redirection> & reds)
{
  std::vector<int> saved;
  for(auto &r:reds)
  {
    saved.push_back(dup(r.fd));
    int flags= O_WRONLY|O_CREAT;
    if(r.append)
    {
      flags|=O_APPEND;
    }
    else
    {
      flags|=O_TRUNC;
    }
    int fd=open(r.filename.c_str(),flags,0644);
    if(fd==-1)
    {
      perror("open");
      exit(1);
    }
    dup2(fd,r.fd);
    close(fd);
  }
  return saved;
}
void restore_redirections(const std::vector<Redirection>& reds,const std::vector<int>& saved)
{
    for(int i=0;i<reds.size();i++)
    {
        dup2(saved[i],reds[i].fd);
        close(saved[i]);
    }
}
std::vector<std::string> runCompleter(const std::string &script,std::string &command,
  std::string &currentword,std::string &previousWord,std::string &user_input)
{
  std::string cmd ="'" + script + "' '" +command + "' '" +currentword + "' '" +previousWord + "'";
    std::vector<std::string> matches;
    setenv("COMP_LINE",user_input.c_str(),1);
    std::string point=std::to_string(user_input.size());
    setenv("COMP_POINT",point.c_str(),1);
    FILE *pipe=popen(cmd.c_str(),"r");
    if (!pipe)
    {
      return matches;
    }
    char buffer[1024];
    while (fgets(buffer,sizeof(buffer),pipe))
    {
        std::string line(buffer);
        while (!line.empty()&&(line.back()=='\n'||line.back()=='\r'))
        {
            line.pop_back();
        }
        if (!line.empty())
            matches.push_back(line);
    }
    pclose(pipe);
    return matches;
}
char getch()
{
  char c;
  termios oldt,newt;
  tcgetattr(STDIN_FILENO,&oldt);
  newt=oldt;
  newt.c_lflag&=~(ICANON|ECHO);
  tcsetattr(STDIN_FILENO,TCSANOW,&newt);
  read(STDIN_FILENO,&c,1);
  tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
  return c;
}
std::string LCP(std::vector<std::string> &matches)
{
  if(matches.empty())
  {
    return "";
  }
  std::string prefix=matches[0];
  for(int i=1;i<matches.size();i++)
  {
    while(matches[i].find(prefix)!=0)
    {
      prefix.pop_back();
      if(prefix.empty())
      {
        return "";
      }
    }
  }
  return prefix;
}
std::vector<std::string> autocomplete(std::string curr)
{
  std::vector<std::string> matches;
  for(auto &cmd:builtin)
  {
    if(cmd.find(curr)==0)
    {
      matches.push_back(cmd);
    }
  }
  char *path=getenv("PATH");
  if(path)
  {
    std::istringstream ss(path);
    std::string dir;
    while(getline(ss,dir,':'))
    {
      if(!std::filesystem::exists(dir))
      {
        continue;
      }
      for(auto &entry :std::filesystem::directory_iterator(dir))
      {
        std::string file=entry.path().filename().string();
        if(file.find(curr)==0)
        {
          matches.push_back(file);
        }
      }
    }
  }
  sort(matches.begin(), matches.end());

    matches.erase(unique(matches.begin(), matches.end()),matches.end());
    return matches;
}
std::vector<std::string> getFileMatches(std::string &input)
{
  std::vector<std::string> matches;
  std::string dir=".";
  std::string prefix=input;
  size_t pos=input.find_last_of("/\\");
  std::string base="";
  if(pos!=std::string::npos)
  {
    base=input.substr(0,pos+1);
    dir=input.substr(0,pos);
    prefix=input.substr(pos+1);
    if(dir.empty())
    {
      dir="/";
    }
  }
  try
  {
    for(auto &entry:fs::directory_iterator(dir))
    {
      std::string name=entry.path().filename().string();
      if(name.rfind(prefix,0)==0)
      {
        if(entry.is_directory())
        {
          matches.push_back(base+name+"/");
        }
        else
        {
          matches.push_back(base+name);
        }
      }
    }
  }
  catch(...)
  {
    // catch all exceptions
  }
  sort(matches.begin(),matches.end());
  return matches;
}
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
    else if(c=='&'&&!singlequote&&!doublequote)
    {
      if(!curr.empty())
      {
        args.push_back(curr);
        curr.clear();
      }
      args.push_back("&");
    }
    else if(!singlequote&&!doublequote&&(c=='>'||(c=='1'&&i+1<input.size()&&input[i+1]=='>')||(c=='2'&&i+1<input.size()&&input[i+1]=='>')))
    {
      if(!curr.empty())
      {
        args.push_back(curr);
        curr.clear();
      }
      std::string op;
      if(c=='1'||c=='2')
      {
        op+=c;
        i++;
      }
      op+='>';
      if(i+1<input.size()&&input[i+1]=='>')
      {
        
        op+='>';
        i++;
      }
      args.push_back(op);
      continue;
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
int execute_file(std::vector<std::string> args,std::vector<Redirection> &reds)
{
  
  if(args.empty())
  {
    return 0;
  }
  bool background=false;
  if(args.back()=="&")
  {
    background=true;
    args.pop_back();
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
     apply_redirections(reds);
     execv(path.c_str(), argv.data());
        perror("execv");
        _exit(1);
  }
  if(background)
  {
    std::string cmd;
    for(int i=0;i<args.size();i++)
    {
      if(i>0)
      {
        cmd+=" ";
      }
      cmd+=args[i];
    }
    int job_no=getjobno();
    jobs.push_back({
      job_no,
      pid,
      cmd,
      "Running"
    });
    std::cout<<"["<<job_no<<"] "<<pid<<'\n';
    nextJob++;
    return 0;
  }
  else
  {
    int status;
    waitpid(pid,&status,0);
    return WEXITSTATUS(status);
  }
}
void trim(std::string &s)
{
    int start=0;
    while(start<s.size()&&s[start]==' ')
        start++;
    if(start==s.size()) 
    {
        s="";
        return;
    }

    int end=s.size()-1;
    while (end>=start&&s[end]==' ')
        end--;

    s=s.substr(start,end-start+1);
}
bool execute_builtin(std::vector<std::string> &args)
{
  if(args[0]=="exit")
  {
    if(!history.empty())
    {
      history_append(history_file);
    }
    exit(0);
  }
  if(args[0]=="history")
  {
    int start=0;
    if(args.size()==2&&args[1][0]!='-')
    {
      int n=std::stoi(args[1]);
      if(n<(int)history.size())
      {
        start=history.size()-n;
      }
    }
    else if(args.size()==3&&args[1]=="-r")
    {
      history_read(args[2]);
      return true;
    }
    else if(args.size()==3&&args[1]=="-w")
    {
      history_write(args[2]);
      return true;
    }
    else if(args.size()==3&&args[1]=="-a")
    {
      history_append(args[2]);
      return true;
    }
     for(int i=start;i<history.size();i++)
    {
      std::cout<<i+1<<" "<<history[i]<<'\n';
    }
    return true;
  }
  if(args[0]=="declare")
  {
    declare(args);
  }
  if(args[0]=="echo")
  {
    echo(args);
    return 1;
  }
  if(args[0]=="pwd")
  {
    pwd();
    return 1;
  }
  if(args[0]=="type")
  {
    int saved_stdout = -1;
    int fd = -1;
    if(args.size()<2)
    {
      return true;
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
  return true;
  }
  if(args[0]=="cd")
  {
    if(args.size()>1)
    {
      cd(args[1]);
    }
    return true;
  }
  if(args[0]=="jobs")
  {
    jobs_func(args);
    return true;
  }
  if(args[0]=="complete")
  {
    complete(args);
    return true;
  }
  return false;
}
void execute_pipe(std::string &input)
{
  std::vector<std::string> commands;
  std::stringstream ss(input);
  std::string cmd;
  while(getline(ss,cmd,'|'))
  {
    trim(cmd);
    commands.push_back(cmd);
  }
  if(commands.empty())
  {
    return;
  }
  int prev_read=-1;
  std::vector<pid_t> pids;
  for(int i=0;i<commands.size();i++)
  {
    std::vector<std::string> args=tokenize(commands[i]);
    if(args.empty())
    {
      continue;
    }
    std::vector<char *> argv;
    for(auto &s:args)
    {
      argv.push_back(s.data());
    }
    argv.push_back(nullptr);
    int fd[2];
    if(i!=commands.size()-1)
    {
      if(pipe(fd)==-1)
      {
        perror("pipe");
        return;
      }
    }
    pid_t pid=fork();
    if(pid==-1)
    {
      perror("fork");
      return;
    }
    if(pid==0)
    {
      if(prev_read!=-1)
      {
        dup2(prev_read,STDIN_FILENO);
      }
      if(i!=commands.size()-1)
      {
        dup2(fd[1],STDOUT_FILENO);
      }
      if(prev_read!=-1)
      {
        close(prev_read);
      }
      if(i!=commands.size()-1)
      {
        close(fd[0]);
        close(fd[1]);
      }
      if(execute_builtin(args))
      {
        _exit(0);
      }
      std::string path=get_command_path(args[0]);
      if(path.empty())
      {
        std::cerr<<args[0]<<": command not found\n";
        _exit(1);
      }
      execv(path.c_str(),argv.data());
      perror("execv");
      _exit(1);
    }
    pids.push_back(pid);
    if(prev_read!=-1)
    {
      close(prev_read);
    }
    if(i!=commands.size()-1)
    {
      close(fd[1]);
      prev_read=fd[0];
    }

  }
  if(prev_read!=-1)
  {
    close(prev_read);
  }
  for(pid_t p:pids)
  {
    waitpid(p,nullptr,0);
  }
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  history_boot();
  do
  {
    reapJobs();
  std::cout << "$ ";
  std::string user_input;
  bool lastTab=false;
  while(true)
  {
    char c=getch();
  
  if(c==27)
  {
    char c1=getch();
    if(c1=='[')
    {
      char c2=getch();
      if(c2=='A')
      {
        if(!history.empty()&&historyIndex>0)
        {
          historyIndex--;
          std::cout<<"\r\33[2K";
          std::cout<<"$ ";
          user_input=history[historyIndex];
          std::cout<<user_input;
          std::cout.flush();
        }
        continue;
      }
      if(c2=='B')
      {
        if(historyIndex<history.size())
        {
          historyIndex++;
          std::cout << "\r\33[2K";
          std::cout << "$ ";
          if(historyIndex==history.size())
          {
            user_input.clear();
          }
          else
          {
            user_input=history[historyIndex];
            std::cout<<user_input;
          }
          std::cout.flush();
        }
        continue;
      }
    }
  }
    if(c=='\n')
    {
      lastTab=false;
      std::cout<<'\n';
      break;
    }
    if(c==127)
    {
      lastTab=false;
      if(!user_input.empty())
      {
        user_input.pop_back();
        std::cout<<"\b \b";
        std::cout.flush();
      }
      continue;
    }
    if(c=='\t')
    {
      size_t pos=user_input.find_last_of(" ");
      bool endswithSpace=!user_input.empty()&&user_input.back()==' ';
      std::string word;
      if(endswithSpace)
      {
        word="";
      }
      else if(pos==std::string::npos)
      {
        word=user_input;
      }
      else
      {
        word=user_input.substr(pos+1);
      }
      std::istringstream ss(user_input);
      std::string command;
      ss>>command;
      std::vector<std::string> tokens=tokenize(user_input);
      std::string prevword="";
      if(endswithSpace)
      {
        if(!tokens.empty())
        {
          prevword=tokens.back();
        }
      }
      else
      if(tokens.size()>=2)
      {
        prevword=tokens[tokens.size()-2];
      }
      std::vector<std::string> matches;
      if(pos==std::string::npos)
      {
        matches=autocomplete(word);
      }
      else
      {
        auto it=completeC.find(command);
        if(it!=completeC.end())
        {
          matches=runCompleter(it->second,command,word,prevword,user_input);
        }
        else if(pos==std::string::npos)
        {
          matches=autocomplete(word);
        }
        else
        matches=getFileMatches(word);
      }
      std::string prefix=LCP(matches);
      if(matches.empty())
      {
        lastTab=false;
        std::cout<<'\a';
        std::cout.flush();
        continue;
      }
      if(matches.size()==1)
      {
        lastTab=false;
        user_input.erase(user_input.size()-word.size());
        user_input+=matches[0];
        if(matches[0].back()!='/')
            user_input+=" ";
        std::cout<<"\r\33[2K";
        std::cout<<"$ "<<user_input;
        std::cout.flush();
        continue;
      }
      if(prefix.size()>word.size())
      {
          lastTab=false;
          user_input.erase(user_input.size()-word.size());
          user_input+=prefix;
        std::cout << "\r\33[2K";
        std::cout << "$ " << user_input;
        std::cout.flush();
         continue;
      }

      if(!lastTab)
      {
        std::cout<<'\a';
        std::cout.flush();
        lastTab=!lastTab;
      }
      else
      {
        std::cout<<"\n";
        for(auto s:matches)
        {
          std::cout<<s<<" ";
        }
        std::cout<<'\n';
        std::cout<<"$ "<<user_input;
        std::cout.flush();
        lastTab=false;
      }
    continue;   
    }
    lastTab=false;
    user_input+=c;
    std::cout<<c;
    std::cout.flush();
    }
    if(!user_input.empty())
    {
      history.push_back(user_input);
      historyIndex=history.size();
    }
    
  size_t pipePos=user_input.find('|');
  if(pipePos!=std::string::npos)
  {
    execute_pipe(user_input);
    continue;
  }
  std::vector<std::string> args=tokenize(user_input);
  std::vector<Redirection> redirections;
  std::vector<std::string> realArgs;
  for(int i=0;i<args.size();i++)
  {
    if(args[i]==">"||args[i]=="1>")
    {
        redirections.push_back({STDOUT_FILENO,args[i+1],false});
        i++;
    } 
    else if(args[i]==">>"||args[i]=="1>>")
    {
      redirections.push_back({STDOUT_FILENO,args[i+1],true});
      i++;
    }
    else if(args[i]=="2>")
    {
      redirections.push_back({STDERR_FILENO,args[i+1],false});
      i++;
    }
    else if(args[i]=="2>>")
    {
      redirections.push_back({STDERR_FILENO,args[i+1],true});
      i++;
    }
    else
    {
      realArgs.push_back(args[i]);
    }
  }

  args=realArgs;
  if(args.empty())
  {
    continue;
  }

  if(std::find(builtin.begin(),builtin.end(),args[0])!=builtin.end())
{
    auto saved=apply_redirections(redirections);
    execute_builtin(args);
    restore_redirections(redirections, saved);
    continue;
}
  else
  {
    execute_file(args,redirections);
  }
}
  while(true);
}
