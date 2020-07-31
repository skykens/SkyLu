//
// Created by jimlu on 2020/7/26.
//

#ifndef MQBUSD_CONSUMER_H
#define MQBUSD_CONSUMER_H
#include <string>
#include <deque>
#include <skylu/proto/mq_proto.h>
#include "mqbusd.h"
/// 取消订阅、 拉取 、提交都是向全部有效的连接发送的。 但是如果在订阅成功的时候保存了连接，当TcpClient检测到连接关闭要移除的时候，consumer这里保存了一份副本会导致原来废除的连接没办法删除。
///
class Consumer :public MqBusd
{
public:
  typedef std::pair<int,std::string> IdAndMessagePair ;
  typedef std::unordered_map<std::string,std::deque<IdAndMessagePair>> TopicMap;  ///topic - id+message
  typedef std::function<void(std::string)> SubscribeCallback;
  Consumer(EventLoop * loop,const std::vector<Address::ptr> & dir_addrs,const std::string &name,int id  = -1);
  ~Consumer() override = default;
  void subscribe(const std::string &topic);
  bool cancelSubscribe(const std::string& topic);
  void poll(int timeoutMs);
  void commit();
  void commit(int messageId);

  TopicMap getMessage(){return m_recv_messages;}
  void setSubscribeCallback(const SubscribeCallback &cb){m_subscribe_cb = cb;}
  void setCancelSubscribeCallback(const SubscribeCallback &cb){m_cancel_subscribe_cb = cb;}


private:
  void sendKeepAliveWithMs();
  void onConnectionToMqServer(const TcpConnection::ptr &conne) override;
  void onMessageFromMqServer(const TcpConnection::ptr &conne, Buffer *buff) override;
  void onCloseConnection(const TcpConnection::ptr & conne);
  void subscribe(const TcpConnection::ptr &conne);
  void pull(int timeoutMs);
  void handleSubscribe(const MqPacket * msg,const TcpConnection::ptr &conne);
  void handleCommit(const MqPacket *msg,const TcpConnection::ptr &conne);
  void handleCancelSubscribe(const MqPacket *msg,const TcpConnection::ptr &conne);
  void handlePull(const MqPacket *msg,const TcpConnection::ptr &conne);

private:
  std::unordered_set<std::string> m_topic; ////订阅组
  std::unordered_map<std::string, TcpConnection::ptr> m_vaild_topic_conne;
  int m_groupId;
  std::unordered_map<int,std::string> m_recv_messageId;  /// 主要用来判重的。
  TopicMap  m_recv_messages; /// 接受到的消息 ，当commit 的时候会移除对应的就message
  int m_maxEnableBytes;
  int m_minEnableBytes;
  SubscribeCallback m_subscribe_cb;
  SubscribeCallback m_cancel_subscribe_cb;

};

#endif // MQBUSD_CONSUMER_H
