//
// Created by jimlu on 2020/7/23.
//

#include "commitpartition.h"
#include "mqbroker.h"
#include "partition.h"
#include "yaml-cpp/yaml.h"
#include <fstream>
#include <skylu/net/eventloop.h>
#include <skylu/base/daemon.h>
#include <vector>

using namespace  skylu;
int main(int argc,char **argv)
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
  std::string name = config["Name"].as<std::string>();
  std::string globalLog = "",commitlog = "";
  if(config["GlobalLog"]){
    globalLog = config["GlobalLog"].as<std::string>();
  }

  if(config["CommitLog"]){
    commitlog = config["CommitLog"].as<std::string>();
  }
  if(globalLog.empty()&&config["Daemon"].as<bool>()){
    std::cout<<"please specifies global log path if you need daemon with start.";
    return -1;
  }
  if(!globalLog.empty()){
    if(config["Daemon"].as<bool>()){
      Daemon::start();
    }
    G_LOGGER->clearAppenders();
    G_LOGGER->addAppender(LogAppender::ptr(new FileLogAppender(globalLog)));
  }



  G_LOGGER->setLevel(LogLevel::FromString(config["LogLevel"].as<std::string>()));
  EventLoop loop;
  std::vector<Address::ptr> peer_addrs(config["DirServer"].size());
  for(size_t i = 0;i<peer_addrs.size();++i){
    peer_addrs[i] = IPv4Address::Create(
        config["DirServer"][i]["addr"].as<std::string>().c_str(),
            config["DirServer"][i]["port"].as<uint32_t>());
  }
  Address::ptr addr = IPv4Address::Create(
      config["LocalAddress"].as<std::string>().c_str(),
      config["Port"].as<uint32_t>()
      );

  MqBroker server(&loop,addr,peer_addrs,name,
                  config["PersistentCommitMs"].as<int>(),
                  config["PersistentLogMs"].as<int>(),
                  config["SingleFileMax"].as<uint64_t>(),
                  config["MsgBlockMax"].as<int>(),
                  config["IndexMinInterval"].as<int>(),
                  commitlog);

  server.run();


  return 0;

}