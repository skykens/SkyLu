//
// Created by jimlu on 2020/7/24.
//

#include "dirserver_proto.h"
void getRegisteConf(const TcpConnection::ptr &conne) {

  DirServerPacket msg{};
  msg.Msgbytes = 0;
  msg.command = DirProto::COMMAND_REQUEST;
  conne->send(reinterpret_cast<char *>(&msg),sizeof(DirServerPacket));
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sendToDirServer[conne|%s]",conne->getName().c_str());


}
void parseRegisteConf(Buffer *buff,HostAndPortSet &set) {

  std::string tmp(buff->curRead(),buff->readableBytes());
  buff->updatePos(buff->readableBytes());

  for(size_t i = 0;i<tmp.size();){
    int  br = tmp.find('\n',i);
    std::string str  = tmp.substr(i,br-i);
    set.insert(str);  ///str = host:port
    i = br + 1;
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"parseReghisteConf| host : %s ",str.c_str());
  }

}
