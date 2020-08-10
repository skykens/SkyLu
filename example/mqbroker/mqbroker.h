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
#include "commitpartition.h"



using namespace skylu;
class MqBroker :Nocopyable{
  typedef std::unique_ptr<TcpClient> TcpClientPtr;
public:
  MqBroker(EventLoop * loop
           ,const Address::ptr& local_addr
           ,std::vector<Address::ptr>& peerdirs_addr
           ,const std::string & name
           ,int PersistentCommitMs = 500
           ,int PersistentLogMs = 5000
           ,uint64_t singleFileMax = 64
           ,int MsgBlockMax = 4096
           ,int IndexMinInterval = 10
           ,const std::string & commitLogfilename = ""
           );
  void run();

  ~MqBroker() = default;


private:

  void init();
  void initHookSignal();
  /**
   * 处理SIGNAL 信号
   */
  void HandleSIGNAL();
  void persistentPartitionWithMs();
  void persistentCommitWithMs();
  void sendRegisterWithMs(const TcpConnection::ptr & conne);
  void sendKeepAliveWithMs();
  void removeInvaildConnection(const TcpConnection::ptr & conne);
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
  std::unordered_map<TcpConnection::ptr,std::unordered_set<std::string> > m_consumer; ///消费者订阅的组
  std::unordered_map<std::string,Partition::ptr> m_partition; /// topic -  message
  std::unordered_set<uint64_t> m_messageId_set;
  CommitPartition::ptr m_commit_partition;

  const int kPersistentTimeMs;
  const int kPersistentCommmitTimeMs;
  const uint64_t ksingleFileMaxSize; ///
  const int kMsgblockMaxSize;  ///设置单个消息超过多大的时候稀疏索引 +  1
  const int  kIndexMinInterval;
  const std::string kCommitLogFileName; ///提交日志存放的位置



};

#endif // DIRSERVER_MQSERVER_H
