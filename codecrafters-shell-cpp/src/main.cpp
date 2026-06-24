#include <iostream>
#include <string>
#include <vector>
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  std::vector<std::string> builtin;
  builtin.push_back("exit");
  builtin.push_back("echo");
  builtin.push_back("type");
  do
  {
  std::cout << "$ ";
  std::string user_input;
  std::getline(std::cin,user_input);
  if(user_input=="exit")
  {
    break;
  }
  else if(user_input.substr(0,5)=="echo ")
  {
    std::cout<<user_input.substr(5)<<'\n';
  }
  else if(user_input.substr(0,5)=="type ")
  {
    if(std::find(builtin.begin(),builtin.end(),user_input.substr(5))!=builtin.end())
        std::cout<<user_input.substr(5)<<" "<<"is a shell builtin"<<'\n';
    else
        std::cout<<user_input.substr(5)<<": not found"<<"\n";
  }
  else
  std::cout<<user_input<<": command not found"<<"\n";
  }
  while(true);
}
