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
      ,m_register_conf(nullptr)
      ,kRegisterFileName(name + ".conf")
      ,kRegisterBackup(name + ".conf.bak")
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
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"close connection %s",conne->getName().c_str());
    m_conf.erase(conne->getName());
    m_clients.erase(conne->getSocketFd());
    m_clients_heartBeat.erase(conne->getSocketFd());

  });

  if(File::isExits(kRegisterFileName)){
    File::remove(kRegisterFileName);
  }

  int fd = ::open(kRegisterFileName.c_str(),O_RDWR|O_CREAT|O_CLOEXEC);
  m_register_conf.reset(new File(fd));


}
void DirServer::onMessage(const TcpConnection::ptr &conne, Buffer *buff) {

  auto msg = reinterpret_cast<const DirServerPacket *>(buff->curRead());
  buff->updatePos(sizeof(DirServerPacket) + msg->Msgbytes);
  switch (msg->command) {
  case COMMAND_KEEPALIVE:
    assert(msg->Msgbytes == 0);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_keepalive from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleKeepAlive(conne);
    break;
  case COMMAND_REGISTER:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_register from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleRegister(conne,msg);
    break;
  case COMMAND_REQUEST:
    assert(msg->Msgbytes == 0);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_request from %s  host:%s"
    ,conne->getName().c_str()
    ,conne->getSocket()->getRemoteAddress()->toString().c_str());
    handleRequest(conne);
    break;
  default:
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild command %d",msg->command);
  }


}

void DirServer::listenClientWithMs() {
  Timestamp now = Timestamp::now();
  assert(m_clients.size() == m_clients_heartBeat.size());
  assert(m_conf.size() == m_clients_heartBeat.size());
  for(auto it = m_clients_heartBeat.begin(); it != m_clients_heartBeat.end();){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"now timestamp :% d, interval : %d"
                        ,now.getMicroSeconds(), now - it->second.getMicroSeconds());
    if((now - it->second) > m_ClientHeartBeatMs * 1000){  // 因为是微妙
      assert(m_clients.find(it->first) != m_clients.end()); //it->first : fd
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%d|%s] has timeout"
                          ,it->first,m_clients[it->first]->getName().c_str());
      auto & conne = m_clients[it->first];
      m_clients.erase(it->first);
      m_conf.erase(conne->getName());
      m_clients_heartBeat.erase(it++);


    }else{
      ++it;
    }
  }
  updateRegisterFile();



}
void DirServer::handleRegister(const TcpConnection::ptr &conne,const DirServerPacket * msg) {

  int fd = conne->getSocketFd();
  assert(m_clients.find(fd) == m_clients.end());
  std::string addr(&msg->hostAndPort,msg->Msgbytes);
  m_clients.insert({fd,conne});
  m_conf.insert({conne->getName(),addr});
  m_clients_heartBeat.insert({fd,Timestamp::now()});
  updateRegisterFile();
  sendAck(conne);

}
void DirServer::handleRequest(const TcpConnection::ptr &conne) {

  if(!File::getFilesize(kRegisterFileName)){
    ///注册文件为空的时候仍然还是要发ACK
    sendAck(conne);

  }else {
    if(!m_clients_heartBeat.empty())
      conne->sendFile(kRegisterFileName.c_str());
  }

}
void DirServer::handleKeepAlive(const TcpConnection::ptr &conne) {
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




}
void DirServer::sendAck(const TcpConnection::ptr &conne) {
  DirServerPacket msg{};
  msg.command = COMMAND_ACK;
  conne->send(&msg,sizeof(DirServerPacket));
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send ack to conne:%s",conne->getName().c_str());

}
void DirServer::updateRegisterFile() {
  if(File::isExits(kRegisterBackup)){
    File::remove(kRegisterBackup);
  }
  File::ptr file = Fdmanager::FdMagr::GetInstance()->open(kRegisterBackup,O_CREAT|O_RDWR);
  for(const auto& it : m_conf){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"register new host : %s from conne[%s]",
                        it.second.c_str(),it.first.c_str());
    file->writeNewLine(it.second.c_str());
  }

  File::rename(kRegisterBackup,kRegisterFileName);


  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"updateRegisterFile successful! now clients' num = %d",
                      m_clients.size());

}
