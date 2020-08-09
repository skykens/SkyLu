//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_TCPCLIENT_H
#define DIRSERVER_TCPCLIENT_H

#include "eventloop.h"
#include "connector.h"
#include "tcpconnection.h"

namespace skylu {

class TcpClient : Nocopyable {
public:
  TcpClient(EventLoop* loop,
            const Address::ptr &server,
            const  std::string & name);
  ~TcpClient();

  void connect();
  void disconnect();
  void stop();
  inline const TcpConnection::ptr& getConnection() const {return m_connection;}

  bool isRetry()const {return m_isRetry;}
  bool enableConnect() const {return m_enableConnect;}
  void enableRetry(){m_isRetry = true;}

  const std::string & getName()const {return m_name;}

  void setConnectionCallback(const TcpConnection::ConnectionCallback& cb){m_connection_callback = cb;}
  void setMessageCallback(const TcpConnection::MessageCallback & cb){m_message_callback = cb;}
  void setWriteCompleteCallback(const TcpConnection::WriteCompleteCallback &cb){m_writeCompleteCallback = cb;}
  void setCloseCallback(const TcpConnection::CloseCallback & cb){m_close_callback = cb;}


private:
  void newConnection(const Socket::ptr & sock);
  void removeConnection(const TcpConnection::ptr & conne);



private:
  EventLoop * m_loop;
  Connector::ptr m_connector;
  const std::string m_name;
  TcpConnection::ConnectionCallback  m_connection_callback;
  TcpConnection::MessageCallback  m_message_callback;
  TcpConnection::WriteCompleteCallback  m_writeCompleteCallback;
  TcpConnection::CloseCallback  m_close_callback;
  Mutex m_mutex; // m_connection和m_connector 可能跨线程 需要加锁
  bool m_isRetry;
  bool m_enableConnect;
  int m_ConnectId;  // 连接 ID （用来标识连接次数 ）
  TcpConnection::ptr m_connection;


};

}
#endif // DIRSERVER_TCPCLIENT_H
