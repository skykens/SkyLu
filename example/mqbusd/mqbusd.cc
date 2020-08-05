//
// Created by jimlu on 2020/7/22.
//

#include "mqbusd.h"
void fun(int a);

MqBusd::MqBusd(EventLoop *loop,const std::vector<Address::ptr> &dir_addrs,
               const std::string &name)
      :m_loop(loop)
      ,m_name(name)
      ,m_dir_clients(dir_addrs.size())
      ,m_updateMqServerConfTimer(nullptr,-1)
      ,m_isVaild(false){
  for(size_t i = 0; i<m_dir_clients.size() ; ++i){

    m_dir_clients[i].reset(new TcpClient(m_loop,dir_addrs[i]
                                         ,name + "#DirClient[" + std::to_string(i) + "]"));

    m_dir_clients[i]->enableRetry();
    m_dir_clients[i]->setConnectionCallback(std::bind(&MqBusd::onNewDirServerConnection,this,std::placeholders::_1));
    m_dir_clients[i]->setMessageCallback(std::bind(&MqBusd::onMessageFromDirServer
                                                   ,this,std::placeholders::_1,std::placeholders::_2));
  }

}
MqBusd::MqBusd(const std::string &name)
   :m_loop(nullptr)
      ,m_name(name)
      ,m_dir_clients()
      ,m_updateMqServerConfTimer(nullptr,-1)
      ,m_isVaild(false){

  }
MqBusd::~MqBusd() = default;



void MqBusd::onMessageFromDirServer(const TcpConnection::ptr &conne,
                                    Buffer *buff) {
  if(buff->readableBytes() != sizeof(DirServerPacket)){
    serializationFromBuffer(buff,m_mqserver_info_tmp);

  }else{
    buff->updatePos(sizeof(DirServerPacket));
  }
  if(++m_recv_dir_count*2  >=  m_dir_clients.size()){
    /// 做一个并集 大部分的主备都已经返回信息了就可以更新
    if(m_mqserver_info != m_mqserver_info_tmp) {
      updateMqServerClients();
    }
    m_mqserver_info_tmp.clear();
    m_recv_dir_count = 0;

  }



}

void MqBusd::onConnectionToMqServer(const TcpConnection::ptr &conne) {
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"connect to %s",conne->getName().c_str());
  m_isVaild = true;
  if(m_conne_cb){
    m_conne_cb(conne);
  }


}

void MqBusd::updateMqServerClients() {

  if(m_mqserver_info_tmp.empty()){
    /// 为空的时候说明服务端全部无效
    SKYLU_LOG_ERROR(G_LOGGER)<<"dirServer send empty addrs. m_mqserver_clients clear.";
    if(!m_mqserver_clients.empty()){
      m_mqserver_clients.clear();
    }
    m_mqserver_info.clear();
    assert(m_mqserver_clients.empty());
  }else {
    m_mqserver_info = m_mqserver_info_tmp;
    for (auto it = m_mqserver_clients.begin();
         it != m_mqserver_clients.end();) {
      ///删除失效的TCPClient
      if (m_mqserver_info_tmp.find(it->first) == m_mqserver_info_tmp.end()) {
        m_mqserver_clients.erase(it++); /// 这里删除client 随之connection也会被删除
      } else {
        ++it;
      }
    }
   connectToMqServer();
  }
  if(m_updateMqServerConfTimer.getSequence() < 0){
   m_updateMqServerConfTimer =  m_loop->runEvery(kSendRequesetToDirSecond,std::bind(&MqBusd::updateMqSeverConfWithMs,this));
   assert(m_updateMqServerConfTimer.getSequence() >=0);
  }

}
void MqBusd::updateMqSeverConfWithMs() {
  for(const auto & it : m_dir_clients){
    const auto & conne = it->getConnection();
    if(conne)
        getRegisteConf(conne);
    else
      SKYLU_LOG_FMT_WARN(G_LOGGER,"DirClient[%s] disconnect ",it->getName().c_str());
  }
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"updateMqServerConfWithMs| now m_dir_clients' num = %d"
                                ", m_mqserver_clients's num = %d"
                      ,m_dir_clients.size(),m_mqserver_clients.size());
}
void MqBusd::onNewDirServerConnection(const TcpConnection::ptr &conne) {

  getRegisteConf(conne);

}
void MqBusd::connect() {
  assert(m_loop);
  for(const auto& it: m_dir_clients){
    it->connect();
  }
}
void MqBusd::init(const std::vector<Address::ptr> &dir_addrs) {
  assert(m_loop);
  m_dir_clients.resize(dir_addrs.size());
  for(size_t i = 0; i<m_dir_clients.size() ; ++i){

    m_dir_clients[i].reset(new TcpClient(m_loop,dir_addrs[i]
        , "#DirClient[" + std::to_string(i) + "]"));

    m_dir_clients[i]->enableRetry();
    m_dir_clients[i]->setConnectionCallback(std::bind(&MqBusd::onNewDirServerConnection,this,std::placeholders::_1));
    m_dir_clients[i]->setMessageCallback(std::bind(&MqBusd::onMessageFromDirServer
        ,this,std::placeholders::_1,std::placeholders::_2));
  }
}
void MqBusd::connectToMqServer() {

  static int i = 0;
  for (auto it : m_mqserver_info) {
    /// 添加最新的Client FIXME  这里会有性能的问题
    if (m_mqserver_clients.find(it.first) == m_mqserver_clients.end()) {
      int flag = it.first.find(':');
      std::string host(it.first.substr(0, flag++));
      int port = atoi(it.first.substr(flag, it.first.size() - flag).c_str());
      Address::ptr addr = IPv4Address::Create(host.c_str(), port);
      m_mqserver_clients[it.first].reset(new TcpClient(
          m_loop, addr, "mqServerClient Id" + std::to_string(++i)));

      SKYLU_LOG_FMT_INFO(G_LOGGER,
                         "initMqServerClients| client host : %s port : %d",
                         host.c_str(), port);

      m_mqserver_clients[it.first]->connect();
      m_mqserver_clients[it.first]->setConnectionCallback(std::bind(
          &MqBusd::onConnectionToMqServer, this, std::placeholders::_1));
      m_mqserver_clients[it.first]->setMessageCallback(
          std::bind(&MqBusd::onMessageFromMqServer, this,
                    std::placeholders::_1, std::placeholders::_2));
    }
  }
}
