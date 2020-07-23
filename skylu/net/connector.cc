//
// Created by jimlu on 2020/5/21.
//

#include "connector.h"
#include <assert.h>
namespace skylu {
Connector::Connector(EventLoop *loop,
                            const Address::ptr &addr)
  :m_loop(loop)
  ,m_server_addr(addr)
  ,enableConnect(false)
  ,m_state(kDisconnected)
  ,m_socket(nullptr)
  ,m_retryDelayMs(kInitRetryDelayMs)
 {

}
Connector::~Connector() {
  assert(!m_channel);
}
void Connector::start() {

  enableConnect = true;
  m_loop->runInLoop(std::bind(&Connector::startInLoop,this));
}
void Connector::startInLoop() {

  m_loop->assertInLoopThread();
  assert(m_state == kDisconnected);
  if(enableConnect){
    connect();
  }

}
void Connector::stopInLoop() {
  m_loop->assertInLoopThread();
  if(m_state == kConnecting){
    setState(kDisconnected);
    removeAndResetChannel();
    retry();
  }
}
void Connector::connect() {
  m_socket = Socket::CreateTCP(nullptr);
  if(!m_socket->connect(m_server_addr)){
    switch (errno)
    {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting();
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry();
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      SKYLU_LOG_ERROR(G_LOGGER) << "connect error in Connector::startInLoop " << errno;
      m_socket.reset();
      break;

    default:
      SKYLU_LOG_ERROR(G_LOGGER) << "Unexpected error in Connector::startInLoop " <<errno;
      m_socket.reset();
      break;
    }

  }


}
void Connector::connecting() {
  setState(kConnecting);
  assert(!m_channel);
  m_channel.reset(new Channel(m_loop,m_socket->getSocket()));
  m_channel->setWriteCallback(std::bind(&Connector::handleWrite,this));
  m_channel->setErrorCallback(std::bind(&Connector::handleError,this));
  m_channel->enableWriting();


}
void Connector::handleWrite() {

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"Connector::handleWrite   state = %d",m_state);

  if(m_state == kConnecting){

    removeAndResetChannel();
    int Error = m_socket->getError();
    if(Error){
      SKYLU_LOG_FMT_ERROR(G_LOGGER,"Connector::handleWrite|error =%d,strerror:%s",Error,strerror(Error));
      retry();
    }else{

      setState(kConnected);
      if(enableConnect){
        assert(m_socket);
        if(m_newconnection_cb)
          m_newconnection_cb(m_socket);
      }else{
        m_socket.reset();

      }
    }
  }else{
    assert(m_state == kDisconnected);
  }


}
void Connector::handleError() {

  SKYLU_LOG_FMT_ERROR(G_LOGGER,"Error errno =  %d  strerrno = %s",errno,strerror(errno));
  if(m_state == kConnecting){
    removeAndResetChannel();
    retry();
    SKYLU_LOG_DEBUG(G_LOGGER)<<"retry again ";
  }


}
void Connector::retry() {

  m_socket.reset();
  setState(kDisconnected);
  if(enableConnect){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"retry after %d",m_retryDelayMs / 1000.0);
    m_loop->runAfter(m_retryDelayMs / 1000.0 , std::bind(&Connector::startInLoop,shared_from_this()));
    m_retryDelayMs = std::min(m_retryDelayMs *2 ,kMaxRetryDelayMs);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"new retryDelay %d",m_retryDelayMs);

  }


}
void Connector::removeAndResetChannel() {
  m_channel->disableAll();
  m_channel->remove();
  m_loop->queueInLoop(std::bind(&Connector::resetChannel,this));
}
void Connector::resetChannel() {
  m_channel.reset();
}
void Connector::restart() {
  m_loop->assertInLoopThread();
  setState(kDisconnected);
  m_retryDelayMs = kInitRetryDelayMs;
  enableConnect = true;
  startInLoop();
}
void Connector::stop() {
  enableConnect = false;
  m_loop->queueInLoop(std::bind(&Connector::stopInLoop,this));

}
}
