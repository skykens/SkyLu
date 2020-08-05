//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_DIRSERVER_PROTO_H
#define DIRSERVER_DIRSERVER_PROTO_H
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "../net/tcpconnection.h"
#include "../net/buffer.h"
using  namespace skylu;
enum DirProto{
  COMMAND_REGISTER = 0,
  COMMAND_REQUEST,
  COMMAND_ACK,
  COMMAND_KEEPALIVE,


};

struct DirServerPacket{
  char command;
  int Msgbytes; ///指的是之后的消息体的大小
  // ******
  //REGISTER-Client : HOST:PORT\r\n   topic0\r\n topic1\r\n   Client 刚上线or本地主题数量更新了
  //REQUEST-Client : topic0\r\n topic\r\n
  //REQUEST-DirServer :   HOST:PORT\r\n   topic0\r\n topic1\r\n

  // KEEPALIVE-Client :无

  ///

};

const int kSendKeepAliveMs = 3000;  //发送心跳的间隔
const int kCheckKeepAliveMs = 5000;  //检查心跳的间隔
typedef std::string HostAndPort;
typedef std::pair<HostAndPort,std::unordered_map<std::string,int> > HostAndTopics; ///host:port  + map<topic,msgNum>
//typedef std::unordered_map<std::string ,HostAndTopics> ConneNameAndClientInfo; /// ConneName + HostAndTopics
typedef std::unordered_map<HostAndPort,std::unordered_map<std::string,int> > HostAndTopicsMap; /// map [host:port  + set<topic> ]

void getRegisteConf(const TcpConnection::ptr & conne);
void serializationFromBuffer(Buffer * buff,HostAndTopics &m);
void serializationFromBuffer(Buffer * buff,HostAndTopicsMap &m);
void serializationToBuffer(Buffer * buff,HostAndTopicsMap & m);

#endif // DIRSERVER_DIRSERVER_PROTO_H
