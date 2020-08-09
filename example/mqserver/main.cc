//
// Created by jimlu on 2020/7/23.
//

#include <fstream>
#include <vector>
#include <skylu/net/eventloop.h>
#include "mqserver.h"
#include "partition.h"
#include "commitpartition.h"

using namespace  skylu;
int main(int argc,char **argv)
{
 // G_LOGGER->setLevel(LogLevel::INFO);
  if(argc < 3){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"your argc = %d,usgae host port id",argc);
    return -1;
  }
  G_LOGGER->setLevel(LogLevel::ERROR);
  EventLoop loop;
  Address::ptr addr = IPv4Address::Create(argv[1],atoi(argv[2]));
  std::vector<Address::ptr> peer_addrs(2);
  peer_addrs[0] = IPv4Address::Create("127.0.0.1",6001);
  peer_addrs[1] = IPv4Address::Create("127.0.0.1",6002);
  MqServer server(&loop,addr,peer_addrs,"MqServer " + std::to_string(addr->getPort()));
  server.run();


  /*

  std::string str = "hello";
  {
    Partition test(str, 0);
    Buffer buff;
    for (; i < 50; ++i) {
      std::string message = "world id =" + std::to_string(i);
      MqPacket msg{};
      msg.command = MQ_COMMAND_DELIVERY;
      msg.messageId = createMessageId();
      msg.msgBytes = message.size();
      msg.topicBytes = str.size();
      serializationToBuffer(&msg, str, message, buff);
    }
    test.addToLog(&buff);
  }

  {
    Partition test(str, 0);
    Buffer buff;
    for (; i < 100; ++i) {
      std::string message = "world id =" + std::to_string(i);
      MqPacket msg{};
      msg.command = MQ_COMMAND_DELIVERY;
      msg.messageId = createMessageId();
      msg.msgBytes = message.size();
      msg.topicBytes = str.size();
      serializationToBuffer(&msg, str, message, buff);
    }
    test.addToLog(&buff);
  }
  {
    Partition test(str, 0);
    Buffer buff;
    for (; i < 150; ++i) {
      std::string message = "world id =" + std::to_string(i);
      MqPacket msg{};
      msg.command = MQ_COMMAND_DELIVERY;
      msg.messageId = createMessageId();
      msg.msgBytes = message.size();
      msg.topicBytes = str.size();
      serializationToBuffer(&msg, str, message, buff);
    }
    test.addToLog(&buff);
  }
  {
    Partition test(str, 0);
    Buffer buff;
    for (; i < 200; ++i) {
      std::string message = "world id =" + std::to_string(i);
      MqPacket msg{};
      msg.command = MQ_COMMAND_DELIVERY;
      msg.messageId = createMessageId();
      msg.msgBytes = message.size();
      msg.topicBytes = str.size();
      serializationToBuffer(&msg, str, message, buff);
    }
    test.addToLog(&buff);
  }
  Partition test(str, 0);
  MqPacket info{};
  info.command = MQ_COMMAND_PULL;
  info.offset = test.getSize() - 10;
  info.maxEnableBytes =  -1;
  test.sendToConsumer(&info, nullptr);

  std::cout<<"test's size = "<<test.getSize();
   */

  return 0;

}