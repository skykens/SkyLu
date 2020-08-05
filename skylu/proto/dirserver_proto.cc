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
void serializationFromBuffer(Buffer * buff,HostAndTopics &m){
 const auto * msg = reinterpret_cast<const DirServerPacket * >(buff->curRead());
  buff->updatePos(sizeof(DirServerPacket));
  const  char * crlf = buff->findCRLF();
  const char * eof = buff->curRead() + msg->Msgbytes;
  assert(msg->Msgbytes);

 if(!crlf){
   SKYLU_LOG_ERROR(G_LOGGER)<<"serialization error from buffer . crlf = nullptr";
   return ;
 }
 HostAndPort  host(buff->curRead(), crlf - buff->curRead());
 std::unordered_map<std::string,int > topics;
 buff->updatePosUntil(crlf + 2);
 if(!buff->readableBytes()){
   assert(!host.empty());
     m = {host,topics};
     SKYLU_LOG_FMT_DEBUG(G_LOGGER,"update m_client_info[%s]",host.c_str());
   return ;
 }


 for(crlf = buff->findCRLF(); crlf && crlf < eof ; crlf = buff->findCRLF()){
   std::string  str(buff->curRead(),crlf - buff->curRead());
   int index = str.find(':');
   std::string topic = str.substr(0,index);
   int num = std::stoi(str.substr(index+1));
   SKYLU_LOG_FMT_DEBUG(G_LOGGER,"topic = %s, msgNum = %d",topic.c_str(),num);
   buff->updatePosUntil(crlf + 2);
   topics.insert({topic,num});
 }
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"update m_client_info[%s]",host.c_str());
  m= {host,topics};

}
void serializationFromBuffer(Buffer * buff,HostAndTopicsMap &m){

  while(buff->readableBytes()){
    HostAndTopics  info;
    serializationFromBuffer(buff,info);
    m.insert(info);
  }
  assert(!buff->readableBytes());





}
void serializationToBuffer(Buffer * buff,HostAndTopicsMap & m){

  for(const auto & it : m){
    Buffer tmp;
    DirServerPacket packet{};
    size_t bodyLength = 0;
    tmp.append(reinterpret_cast<char *>(&packet),sizeof(packet));
    tmp.append(it.first + "\r\n"); ///主机HOST : PORT
    bodyLength += it.first.size() + 2;
    for(const auto & i : it.second){
      std::string str = i.first + ":" + std::to_string(i.second) + "\r\n";
      tmp.append(str); /// topic:msgNum\r\n
      bodyLength += str.size() + 2;
    } ///添加主题
    auto * msg_const  = reinterpret_cast<const DirServerPacket * >(tmp.curRead());
    auto * msg = const_cast<DirServerPacket*>(msg_const);
    msg->command = COMMAND_REQUEST;
    msg->Msgbytes = bodyLength;
    buff->append(tmp.curRead(),tmp.readableBytes());

  }



}
