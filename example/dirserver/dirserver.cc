//
// Created by jimlu on 2020/7/22.
//

#include "dirserver.h"

DirServer::DirServer(EventLoop *owner
                     ,const Address::ptr& addr
                     ,const std::string &name
                     ,int heartBeatMs)
      :m_owner(owner)
      ,m_server(owner,addr,name)
      ,m_ClientHeartBeatMs(heartBeatMs){

  init();

}
void DirServer::run() {
  m_owner->runEvery(m_ClientHeartBeatMs * Timestamp::kSecondToMilliSeconds,std::bind(&DirServer::listenClientWithMs,this));
  m_server.start();
  m_owner->loop();
}
void DirServer::init() {

  m_server.setMessageCallback(std::bind(&DirServer::onMessage,this
      ,std::placeholders::_1
      ,std::placeholders::_2));
  m_server.setConnectionCallback([](const TcpConnection::ptr & conne){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"new connection %s",conne->getName().c_str());

  });

  m_server.setCloseCallback([this](const  TcpConnection::ptr & conne){
    ///如果这里不做的话会延长clients连接的生命期
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"close connection %s",conne->getName().c_str());
    if(m_link_info_to_clients.find(conne->getSocketFd()) != m_link_info_to_clients.end()) {
      m_clients_info.erase(m_link_info_to_clients[conne->getSocketFd()]);
      m_link_info_to_clients.erase(conne->getSocketFd());
      m_clients.erase(conne->getSocketFd());
      m_clients_heartBeat.erase(conne->getSocketFd());
    }

  });


}
void DirServer::onMessage(const TcpConnection::ptr &conne, Buffer *buff) {

  auto msg = reinterpret_cast<const DirServerPacket *>(buff->curRead());
  switch (msg->command) {
  case COMMAND_KEEPALIVE:
    assert(msg->Msgbytes == 0);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_keepalive from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleKeepAlive(conne,buff);
    buff->updatePos(sizeof(DirServerPacket) + msg->Msgbytes);
    break;
  case COMMAND_REGISTER:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_register from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleRegister(conne,buff);
    break;
  case COMMAND_REQUEST:
    assert(msg->Msgbytes == 0);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_request from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleRequest(conne);
    buff->updatePos(sizeof(DirServerPacket) + msg->Msgbytes);
    break;
  default:
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild command %d",msg->command);
  }


}

void DirServer::listenClientWithMs() {
  Timestamp now = Timestamp::now();
  assert(m_clients.size() == m_clients_heartBeat.size());
 // assert(m_clients_info.size() == m_clients_heartBeat.size());
  for(auto it = m_clients_heartBeat.begin(); it != m_clients_heartBeat.end();){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"now timestamp :% d, interval : %d"
                        ,now.getMicroSeconds(), now - it->second.getMicroSeconds());
    if((now - it->second) > m_ClientHeartBeatMs * 1000){  // 因为是微妙
      assert(m_clients.find(it->first) != m_clients.end()); //it->first : fd
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%d|%s] has timeout"
                          ,it->first,m_clients[it->first]->getName().c_str());
      auto & conne = m_clients[it->first];
      m_clients.erase(it->first);
      m_clients_info.erase(m_link_info_to_clients[conne->getSocketFd()]);
      m_link_info_to_clients.erase(conne->getSocketFd());
      m_clients_heartBeat.erase(it++);

      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"now clients_info's num = %d, clients' num = %d"
      ,m_clients_info.size(),m_clients.size());

    }else{
      ++it;
    }
  }



}
void DirServer::handleRegister(const TcpConnection::ptr &conne,Buffer * buff) {

  int fd = conne->getSocketFd();
  m_clients[fd] = conne;
  m_clients_heartBeat[fd] = Timestamp::now();
  HostAndTopics  info;
  serializationFromBuffer(buff,info);
  m_clients_info[info.first] = info.second;
  m_link_info_to_clients[conne->getSocketFd()] = info.first;
  SKYLU_LOG_FMT_INFO(G_LOGGER,"recv register host[%s] topic.size = %d"
                               ",clients_info num=%d, clients' num = %d"
                     ,info.first.c_str(),info.second.size(),m_clients_info.size(),m_clients.size());
}
void DirServer::handleRequest(const TcpConnection::ptr &conne) {

  if(m_clients_info.empty()){
    sendAck(conne);

  }else {
    if(!m_clients_heartBeat.empty()){
      Buffer buff;
      serializationToBuffer(&buff,m_clients_info);
      conne->send(&buff);
    }

  }

}
void DirServer::handleKeepAlive(const TcpConnection::ptr &conne,Buffer * buff) {
  Address::ptr addr = conne->getSocket()->getLocalAddress();
  int fd = conne->getSocketFd();
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"recv heartbeat from conne[%d|%s]"
                        ,conne->getSocketFd(),conne->getName().c_str());

  if(m_clients.find(fd) == m_clients.end()){
    /// FIXME 有时候MqServer 刚连接上就发心跳了
    conne->shutdown();
    return;
  }
    // 更新心跳
    m_clients_heartBeat[fd] = Timestamp::now();

  //  serializationFromBuffer(buff,m_clients_info);


}
void DirServer::sendAck(const TcpConnection::ptr &conne) {
  DirServerPacket msg{};
  msg.command = COMMAND_ACK;
  conne->send(&msg,sizeof(DirServerPacket));
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send ack to conne:%s",conne->getName().c_str());

}
