//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_MQBUSD_H
#define DIRSERVER_MQBUSD_H

#include <vector>
#include <string>
#include <memory>
#include <cstring>

#include <unordered_set>
#include <unordered_map>
#include <skylu/base/nocopyable.h>
#include <skylu/base/mutex.h>
#include <skylu/base/thread.h>
#include <skylu/net/eventloop.h>
#include <skylu/net/tcpclient.h>
#include <skylu/net/tcpconnection.h>
#include <skylu/net/channel.h>
#include <skylu/proto/dirserver_proto.h>
#include <skylu/proto/mq_proto.h>

using namespace  skylu;
class MqBusd : Nocopyable{
  typedef std::unique_ptr<TcpClient> TcpClientPtr;
  static const int kSendRequesetToDirSecond = 5;  /// 获取Dir注册表的间隔时间（单位：s）
public:
  typedef std::function<void(MqPacket *)> MqBusdMessageCallback;
  typedef TcpConnection::ConnectionCallback MqBusdConnectionCallback;
  typedef TcpConnection::CloseCallback  MqBusdCloseCallback;

  MqBusd(EventLoop *loop,const std::vector<Address::ptr> & dir_addrs
         ,const std::string &name);
  explicit MqBusd(const std::string & name);
  virtual ~MqBusd();
  bool isValid() const{return m_isVaild;}

  std::string getName()const {return m_name;}

  void setConnectionCallback(const MqBusdConnectionCallback & cb){m_conne_cb = cb;}




protected:
  void onMessageFromDirServer(const TcpConnection::ptr & conne,Buffer * buff);
  void onNewDirServerConnection(const TcpConnection::ptr & conne);

  virtual void onConnectionToMqServer(const TcpConnection::ptr & conne);
  virtual void onMessageFromMqServer(const TcpConnection::ptr &conne,Buffer * buff) = 0;
  virtual void connectToMqServer();

  void updateMqSeverConfWithMs();
  void updateMqServerClients();
  void connect();
  void init(const std::vector<Address::ptr> &dir_addrs);

protected:

  EventLoop * m_loop;

  const std::string m_name;

  std::vector<TcpClientPtr> m_dir_clients;  ///连接到dir上的clients 使用这个来发送前需要判断connection != nullptr

  std::unordered_map<std::string,TcpClientPtr> m_mqserver_clients; /// 连接到mqserver上的clients

  Timerid m_updateMqServerConfTimer;   ///获取Dir注册表的定时器
  bool m_isVaild;
  MqBusdConnectionCallback  m_conne_cb;

  HostAndTopicsMap  m_mqserver_info_tmp; ///这个用来作冗余存在的
  HostAndTopicsMap  m_mqserver_info; ///实际的
  size_t m_recv_dir_count = 0;


};

#endif // DIRSERVER_MQBUSD_H
