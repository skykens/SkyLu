//
// Created by jimlu on 2020/7/22.
//

#include <iostream>
#include <yaml-cpp/yaml.h>
#include <skylu/base/daemon.h>
#include "dirserver.h"
using namespace  skylu;
int main(int argc,char ** argv)
{
  YAML::Node config;
  if(argc < 2){
    std::cout<<"please enter config.yaml"<<std::endl;
    return -1;
  }
  try{
    config = YAML::LoadFile(argv[1]);
  }catch(YAML::BadFile &e){
    std::cout<<"read yaml error!"<<std::endl;
    return -1;
  }
  G_LOGGER->setLevel(LogLevel::FromString(config["LogLevel"].as<std::string>()));
  if(config["GlobalLog"]){
    G_LOGGER->clearAppenders();
    G_LOGGER->addAppender(LogAppender::ptr(new FileLogAppender(config["GlobalLog"].as<std::string>())));
  }

    if(config["Daemon"].as<bool>()){
    if(config["GlobalLog"]){
      G_LOGGER->clearAppenders();
      G_LOGGER->addAppender(LogAppender::ptr(new FileLogAppender(config["GlobalLog"].as<std::string>())));
      Daemon::start();
    }else{
      std::cout<<"please specify a global log path if you want daemon start."<<std::endl;
      return -1;
    }
  }



  EventLoop g_loop;
  Address::ptr addr = IPv4Address::Create(config["LocalAddress"].as<std::string>().c_str(),
                   config["Port"].as<int>());
  DirServer server(&g_loop,addr,config["Name"].as<std::string>());
  server.run();
  return 0;
}

