//
// Created by jimlu on 2020/7/23.
//

#include <fstream>
#include <vector>
#include <skylu/net/eventloop.h>
#include "mqserver.h"
#include "partition.h"

using namespace  skylu;
int main(int argc,char **argv)
{
  /*
  G_LOGGER->setLevel(LogLevel::INFO);
  if(argc < 3){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"your argc = %d,usgae host port",argc);
    return -1;
  }
  EventLoop loop;
  Address::ptr addr = IPv4Address::Create(argv[1],atoi(argv[2]));
  std::vector<Address::ptr> peer_addrs(2);
  peer_addrs[0] = IPv4Address::Create("127.0.0.1",6001);
  peer_addrs[1] = IPv4Address::Create("127.0.0.1",6002);
  MqServer server(&loop,addr,peer_addrs,"MqServer ");
  server.run();

   */

  std::string str = "hello";
  Buffer msg;
  Partition test(str,0);
  createCommandMqPacket(&msg,MQ_COMMAND_DELIVERY,createMessageId());
  //test.addToLog(&msg);
  std::cout<<test.getTopic();


  return 0;

}