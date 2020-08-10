//
// Created by jimlu on 2020/7/23.
//

#include <fstream>
#include <vector>
#include <skylu/net/eventloop.h>
#include "mqserver.h"
#include "partition.h"
#include "commitpartition.h"
#include "yaml-cpp/yaml.h"

using namespace  skylu;
int main(int argc,char **argv)
{
  YAML::Node config;
  if(argc < 2){
    std::cout<<"please enter config.yaml"<<std::endl;
  }
  try{
    config = YAML::LoadFile(argv[1]);
  }catch(YAML::BadFile &e){
    std::cout<<"read error!"<<std::endl;
    return -1;
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
  std::string name = config["Name"].as<std::string>();
  MqServer server(&loop,addr,peer_addrs,name,
                  config["PersistentCommitMs"].as<int>(),
                  config["PersistentLogMs"].as<int>(),
                  config["SingleFileMax"].as<uint64_t>(),
                  config["MsgBlockMax"].as<int>(),
                  config["IndexMinInterval"].as<int>());

  server.run();


  return 0;

}