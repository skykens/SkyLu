//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_MQSERVER_H
#define DIRSERVER_MQSERVER_H
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <skylu/net/address.h>
#include <skylu/net/tcpclient.h>
#include <skylu/net/tcpserver.h>
#include <skylu/net/eventloop.h>
#include <skylu/net/tcpconnection.h>
#include <skylu/base/nocopyable.h>
#include <skylu/proto/mq_proto.h>
#include <skylu/proto/dirserver_proto.h>
#include "partition.h"



using namespace skylu;
class MqServer :Nocopyable{
  const int kPersistentTimeMs = 5000;
  typedef std::unique_ptr<TcpClient> TcpClientPtr;
public:
  MqServer(EventLoop * loop
           ,const Address::ptr& local_addr
           ,std::vector<Address::ptr>& peerdirs_addr
           ,const std::string & name);
  void run();

  ~MqServer() = default;


private:

  void init();
  void persistentPartitionWithMs();
  void sendRegisterWithMs(const TcpConnection::ptr & conne);
  void sendKeepAliveWithMs();
  void simpleSendReply(uint64_t messageId,char command,const TcpConnection::ptr & conne);

  void onMessageFromDirPeer(const TcpConnection::ptr &conne,Buffer *buff);

  void onMessageFromMqBusd(const TcpConnection::ptr &conne,Buffer *buff);

  /**
   * 处理生产者的投递
   * @param msg
   */
  void handleDeliver(const MqPacket * msg);

  /**
   * 订阅相关
   * @param msg
   */
  void handleSubscribe(const TcpConnection::ptr& conne,const MqPacket * msg);
  void handleCancelSubscribe(const MqPacket * msg);

  /**
   * 处理消费者发来的心跳
   * @param msg
   */
  void handleHeartBeat(const MqPacket *msg);


  /**
   * 处理消费者的pull请求
   * @param msg
   */
  void handlePull(const TcpConnection::ptr& conne,const MqPacket* msg);

  /**
   * 处理消费者的commit请求
   * @param msg
   */
  void handleCommit(const MqPacket *msg);





  const std::string m_name;
  EventLoop * m_loop;
  TcpServer m_server;
  std::vector<TcpClientPtr> m_dirpeer_clients;
  Address::ptr m_local_addr;
  std::unordered_map<TcpConnection::ptr,std::vector<std::string> > m_consumer; ///消费者订阅的组
  std::unordered_map<std::string,std::unique_ptr<Partition>> m_partition; /// topic -  message
  std::unordered_set<uint64_t> m_messageId_set;
  std::unordered_map<int64_t ,std::unordered_map<std::string,uint64_t>> m_consumer_commit;




};

#endif // DIRSERVER_MQSERVER_H
