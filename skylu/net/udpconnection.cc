//
// Created by jimlu on 2020/7/10.
//

#include "udpconnection.h"

#include <utility>
namespace skylu{
UdpConnection::UdpConnection(EventLoop *loop, const Socket::ptr& socket,std::string  name)
    :m_state(kConnected)
    ,m_loop(loop)
    ,m_socket(socket)
    ,m_channel(new Channel(m_loop,m_socket->getSocket()))
    ,m_name(std::move(name)),
     m_highMark(64*1024*1024){  //高水位的位置应该是64K
  assert(!socket->isTcpSocket());
  m_channel->setReadCallback(std::bind(&UdpConnection::handleRead,this));
  m_channel->setErrorCallback(std::bind(&UdpConnection::handleError,this));
  m_channel->setCloseCallback(std::bind(&UdpConnection::handleClose,this));
  m_channel->setWriteCallback(std::bind(&UdpConnection::handleWrite,this));
  assert(m_loop->isInLoopThread());
  assert(m_state == kConnected);
  m_channel->enableReading();
}

void UdpConnection::handleWrite() {

  assert(m_loop->isInLoopThread());
  if(m_channel->isWriting()){

    ssize_t n = ::write(m_channel->getFd(),m_output_buffer.curRead(),m_output_buffer.readableBytes());
    if(n > 0){
      m_output_buffer.updatePos(n);
      if(m_output_buffer.readableBytes() == 0){
      }else{
        SKYLU_LOG_INFO(G_LOGGER)<<"write("<<m_channel->getFd()<<") more data";
      }
    }else{
      SKYLU_LOG_ERROR(G_LOGGER)<<"errno ="<<errno<<"  strerror="<<strerror(errno);
    }
  }else{
    SKYLU_LOG_INFO(G_LOGGER)<<"Connection is down ,no more writing.";
  }

}
void UdpConnection::handleRead() {

  assert(m_loop->isInLoopThread());
  int savedError;
  Address::ptr addr(new IPv4Address());
  ssize_t  n = m_input_buffer.readFd(m_channel->getFd(),&savedError,addr);
  if(n > 0){
    if(m_message_cb){
      m_message_cb(shared_from_this(),addr,&m_input_buffer);
    }

  }else if(n == 0){
    handleClose();
  }else{
    errno = savedError;
    handleError();
  }
}

void UdpConnection::handleError() {
  SKYLU_LOG_ERROR(G_LOGGER)<<"UdpConnection("<<m_channel->getFd()<<") error:"<<errno<<"   strerror:"<<strerror(errno);
}
void UdpConnection::handleClose() {
  assert(m_loop->isInLoopThread());
  assert(m_state == kConnected);
  setState(kdisConnected);
  m_channel->disableAll();
  UdpConnection::ptr conne(shared_from_this());
  m_close_cb(shared_from_this());
}

void UdpConnection::connectDestroyed() {
  assert(m_loop->isInLoopThread());

  if(m_state == kConnected){
    setState(kdisConnected);
    m_channel->disableAll();
  }
  m_channel->remove();
}



void UdpConnection::send(const std::string & message,const Address::ptr& addr){
  send(message.data(),message.size(),addr);
}
void UdpConnection::send(const void *message, size_t len,const Address::ptr& addr) {
  if(m_state == kConnected){
    if(m_loop->isInLoopThread()){
      sendInLoop(message,len,addr);
    } else{
      m_loop->runInLoop(std::bind(&UdpConnection::sendInLoop,this,message,len,addr));
    }
  }

}

void UdpConnection::send(Buffer *buf,const Address::ptr& addr) {
  if(m_state == kConnected){
    if(m_loop->isInLoopThread()){
      sendInLoop(buf->curRead(),buf->readableBytes(),addr);
      buf->resetAll();
    }else{
      m_loop->runInLoop(std::bind(&UdpConnection::sendInLoop,
                                  this,buf->curRead(),buf->readableBytes(),addr));
      buf->resetAll();
    }
  }
}

void UdpConnection::sendInLoop(const void *data, size_t len,const Address::ptr& addr) {
  assert(m_loop->isInLoopThread());
  ssize_t nwrite = 0;
  size_t remaning = len;
  bool Error=false;
  if(m_state == kdisConnected){
    SKYLU_LOG_WARN(G_LOGGER)<<"disconnected ,give up writing";
    return ;
  }
  if(!m_channel->isWriting() && m_output_buffer.readableBytes() == 0){
    socklen_t  socklen = addr->getAddrLen();
    nwrite = ::sendto(m_socket->getSocket(),data,len,0,addr->getAddr(),socklen);

    if(nwrite >= 0){
      //注册handleWrite事件
      remaning = len - nwrite;
      if(remaning == 0 && m_writecomplete_cb){
        m_loop->queueInLoop(std::bind(m_writecomplete_cb,shared_from_this(),addr));

      }

    }else{
      nwrite = 0;
      if(errno != EWOULDBLOCK){
        SKYLU_LOG_ERROR(G_LOGGER)<<"UdpConnection::sendInLoop errno="<<errno
                                 << "  strerrno="<<strerror(errno);
        Error = true;

      }
    }
  }

  assert(nwrite>=0);
  if(!Error && remaning > 0){
    size_t oldlen = m_output_buffer.readableBytes();
    if(oldlen + remaning >= m_highMark
       && oldlen < m_highMark
       && m_highwater_cb){
      m_loop->queueInLoop(std::bind(m_highwater_cb,shared_from_this(),oldlen+remaning,addr));
    }
    m_output_buffer.append(static_cast<const char*>(data)+nwrite,remaning);
    if(!m_channel->isWriting()){
      m_channel->enableWriting();
      SKYLU_LOG_INFO(G_LOGGER)<<"write too large";
    }
  }

}
}