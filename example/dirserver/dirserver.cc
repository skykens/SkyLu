//
// Created by jimlu on 2020/7/22.
//

#include "dirserver.h"

const char * DirServer::kRegisterFileName = "register.conf";
const char* DirServer::kRegisterBackup = "register.conf.bak";
DirServer::DirServer(EventLoop *owner,const Address::ptr& addr,const std::string &name)
      :m_owner(owner)
      ,m_server(owner,addr,name)
      ,m_register_conf(nullptr){

  init();

}
void DirServer::run() {
  m_owner->runEvery(5,std::bind(&DirServer::listenClientWithMs,this));
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

  m_server.setCloseCallback([](const  TcpConnection::ptr & conne){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"close connection %s",conne->getName().c_str());

  });

  int fd = ::open(kRegisterFileName,O_RDWR|O_CREAT|O_CLOEXEC);
  m_register_conf.reset(new File(fd));


}
void DirServer::onMessage(const TcpConnection::ptr &conne, Buffer *buff) {

  auto msg = reinterpret_cast<const DirServerPacket *>(buff->curRead());
  buff->updatePos(sizeof(DirServerPacket));
  switch (msg->command) {
  case COMMAND_KEEPALIVE:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_keepalive from %s  host:%s port:%d"
    ,conne->getName().c_str()
    ,conne->getSocket()->getLocalAddress()->toString().c_str()
    ,conne->getSocket()->getLocalAddress()->getPort());
    handleKeepAlive(conne);
    break;
  case COMMAND_REGISTER:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_register from %s  host:%s port:%d"
    ,conne->getName().c_str()
    ,conne->getSocket()->getLocalAddress()->toString().c_str()
    ,conne->getSocket()->getLocalAddress()->getPort());
    handleRegister(conne);
    break;
  case COMMAND_REQUEST:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"command_request from %s  host:%s port:%d"
    ,conne->getName().c_str()
    ,conne->getSocket()->getLocalAddress()->toString().c_str()
    ,conne->getSocket()->getLocalAddress()->getPort());
    handleRequest(conne);
    break;
  }

}
void DirServer::loadFromRegisterFile() {
  std::vector<std::string> ves;
  m_register_conf->readAllLine(ves);
  for(auto it:ves){
    int port;
    char host[64];
    sscanf(it.c_str(),"%s:%d",host,&port);
    m_conf.insert({host,port});
  }

}

void DirServer::listenClientWithMs() {



}
void DirServer::handleRegister(const TcpConnection::ptr &conne) {

  int fd = conne->getSocketFd();
  Address::ptr addr = conne->getSocket()->getLocalAddress();
  assert(m_clients.find(fd) == m_clients.end());
  m_clients.insert({fd,conne});

  m_conf.insert({addr->toString(),addr->getPort()});
  updateRegisterFile();
  sendAck(conne);

}
void DirServer::handleRequest(const TcpConnection::ptr &conne) {

  conne->sendFile(kRegisterFileName);
}
void DirServer::handleKeepAlive(const TcpConnection::ptr &conne) {
  Address::ptr addr = conne->getSocket()->getLocalAddress();
  if(m_clients.find(conne->getSocketFd()) == m_clients.end() && m_conf.find({addr->toString(),addr->getPort()}) != m_conf.end()){
    // 处理断线重连的情况
    m_clients.insert({conne->getSocketFd(),conne});
  }

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
  int fd = open(kRegisterBackup,O_CREAT|O_RDWR);
  File file(fd);
  for(const auto& it : m_conf){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"register new host : %s port :%d",
                        it.first.c_str(),it.second);
    file.writeNewLine(it.first + ":" + std::to_string(it.second));
  }

  File::rename(kRegisterBackup,kRegisterFileName);

  File::remove(kRegisterBackup);

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"updateRegisterFile successful! now clients' num = %d",
                      m_clients.size());

}
