//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_DIRSERVER_PROTO_H
#define DIRSERVER_DIRSERVER_PROTO_H
#include <string>
#include <vector>
#include <unordered_set>
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
  char Msgbytes;
  char command;
  char hostAndPort; // host:port

};

const int kSendKeepAliveMs = 3000;  //发送心跳的间隔
const int kCheckKeepAliveMs = 5000;  //检查心跳的间隔
typedef std::string HostAndPort;
typedef std::unordered_set<HostAndPort> HostAndPortSet;

void getRegisteConf(const TcpConnection::ptr & conne);
void parseRegisteConf(Buffer *buff,HostAndPortSet & set);

#endif // DIRSERVER_DIRSERVER_PROTO_H
