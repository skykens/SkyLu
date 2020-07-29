//
// Created by jimlu on 2020/7/22.
//

#include "tcpclient.h"

namespace skylu{

TcpClient::TcpClient(EventLoop *loop, const Address::ptr &server,
                     const std::string &name)
:m_loop(loop)
    ,m_connector(new Connector(loop,server))
,m_name(name)
,m_connection_callback(nullptr)
,m_message_callback(nullptr)
,m_writeCompleteCallback(nullptr)
,m_isRetry(false)
      ,m_enableConnect(false)
,m_ConnectId(1)
{

  m_connector->setNewConnectionCallback(
                  std::bind(&TcpClient::newConnection,this,std::placeholders::_1)
                  );

}
TcpClient::~TcpClient() {
  TcpConnection::ptr conne = nullptr;
  if(m_loop->isStop()){
    return;
  }
  bool unique = false;
  {
    Mutex::Lock lock(m_mutex);
    unique = m_connection.unique();  // 如果其他的函数还有保留这个副本就会失败
    conne = m_connection;
    //写时复制
  }
  if(conne){
    assert(m_loop == conne->getLoop());
    TcpConnection::CloseCallback  cb = [this](const TcpConnection::ptr &conne){

      m_loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conne));

    };
    m_loop->runInLoop(std::bind(&TcpConnection::setCloseCallback,conne,cb));
    if(unique){
      conne->shutdown();
    }


  }


}
void TcpClient::connect() {
  m_enableConnect = true;
  m_connector->start();

}
void TcpClient::disconnect() {
  m_enableConnect = false;

  {
    Mutex::Lock lock(m_mutex);
    if(m_connection){
      m_connection->shutdown();
    }
  }

}
void TcpClient::stop() {
  m_enableConnect = false;
  m_connector->stop();

}
void TcpClient::newConnection(const Socket::ptr & sock) {
  m_loop->assertInLoopThread();
  char buf[32];
  sprintf(buf,":%s#%d",sock->getRemoteAddress()->toString().c_str(),m_ConnectId);
  ++m_ConnectId;
  TcpConnection::ptr conne(new TcpConnection(m_loop
                                             ,sock
                                             ,m_name+buf));

  conne->setConnectionCallback(m_connection_callback);
  conne->setMessageCallback(m_message_callback);
  conne->setWriteCompleteCallback(m_writeCompleteCallback);
  conne->setCloseCallback(std::bind(&TcpClient::removeConnection,this,std::placeholders::_1));
  {
    Mutex::Lock lock(m_mutex);
    m_connection = conne;
  }
  conne->connectEstablished();

}
void TcpClient::removeConnection(const TcpConnection::ptr &conne) {
  m_loop->assertInLoopThread();
  assert(m_loop == conne->getLoop());
  {
    Mutex::Lock lock(m_mutex);
    assert(m_connection == conne);
    m_connection.reset();
  }

  m_loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conne));
  if(m_isRetry && m_enableConnect){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"TcpClient::removeConnection[%s]|restart to host %s"
                        ,m_name.c_str(),m_connector->getServerAddress()->toString().c_str());
    m_connector->restart();
  }
}
}