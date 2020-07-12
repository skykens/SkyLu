//
// Created by jimlu on 2020/7/10.
//

#ifndef SKYLU_NET_UDPCONNECTION_H
#define SKYLU_NET_UDPCONNECTION_H

#include <memory>
#include "eventloop.h"
#include "address.h"
#include "socket.h"
#include "../base/nocopyable.h"
#include "buffer.h"
#include "channel.h"
#include "timestamp.h"
#include <string>
namespace skylu{
class UdpConnection :Nocopyable,public std::enable_shared_from_this<UdpConnection>{
  enum State{
    kConnected,  //connectEstablished之后的
    kdisConnected //调用了handleClose之后
  };
public:
  typedef std::shared_ptr<UdpConnection> ptr;
  // TODO UDPConnection addr 的参数传递都是shared_ptr，后续可以看看能不能改一下优化性能
  typedef std::function<void(const UdpConnection::ptr&,const Address::ptr& remote,Buffer *)> MessageCallback;
  typedef std::function<void(const UdpConnection::ptr&)> CloseCallback;
  typedef std::function<void(const UdpConnection::ptr&,const Address::ptr& remote)> WriteCompleteCallback;
  typedef std::function<void(const UdpConnection::ptr& ,size_t,const Address::ptr& remote)> HighWaterMarkCallback;
  UdpConnection(EventLoop * loop,Socket::ptr socket,std::string name);
  ~UdpConnection() = default;

  /**
   * 设置各类回调函数
   * @param cb
   */
  void setMessageCallback(const MessageCallback &cb){m_message_cb = cb;}
  void setCloseCallback(const CloseCallback &cb){m_close_cb = cb;}
  void setWriteCompleteCallback(const WriteCompleteCallback &cb){m_writecomplete_cb = cb;}
  void setHighWaterCallback(const HighWaterMarkCallback &cb,size_t len){m_highwater_cb = cb;m_highMark = len;}


  /**
   * 获得输出输出缓冲区
   * @return
   */
  Buffer * getInputBuffer(){return &m_input_buffer;}
  Buffer* getOutputBuffer(){return &m_output_buffer;}



  std::string getName()const {return m_name;}
  EventLoop * getLoop(){return m_loop;}
  std::string getStateForString()const{
    switch (m_state) {
#define XX(state) \
            case State::state :\
                return #state; \
                break;

    XX(kdisConnected);
    XX(kConnected);
#undef XX
    default:
      return "UNKNOW STATUS";

    }
  }


  void connectDestroyed();

  void send(const std::string & message,const Address::ptr& addr);
  void send(const void *message,size_t len,const Address::ptr& addr);
  void send(Buffer *buf,const Address::ptr& addr);





private:

  /**
   *  各类IO事件触发后的回调函数 （绑定在channel）
   * @param receiveTime
   */
  void handleRead();
  void handleWrite();
  void handleClose();
  void handleError();

  void sendInLoop(const void *data,size_t len,const Address::ptr& addr);

  void setState(State s){m_state = s;}


private:
  State m_state;
  EventLoop *m_loop;
  Socket::ptr m_socket;
  Channel::ptr m_channel;
  MessageCallback m_message_cb;
  CloseCallback  m_close_cb;
  HighWaterMarkCallback m_highwater_cb; //只会在写入操作触发EAGIN时触发一次
  const std::string m_name;
  size_t m_highMark; //超出的部分
  WriteCompleteCallback m_writecomplete_cb;
  Buffer m_input_buffer;   //存放从socket读进来的内容
  Buffer m_output_buffer;  //存放向socket写入超出来的内容(即触发EAGAIN后面的数据)








};

}


#endif // SKYLU_NET_UDPCONNECTION_H
