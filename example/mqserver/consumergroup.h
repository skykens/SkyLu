//
// Created by jimlu on 2020/7/25.
//

#ifndef MQSERVER_CONSUMERGROUP_H
#define MQSERVER_CONSUMERGROUP_H

#include <unordered_set>
#include <unordered_map>
#include <skylu/base/nocopyable.h>
#include <skylu/net/tcpconnection.h>
#include <skylu/net/buffer.h>
using namespace skylu;
class ConsumerGroup  : Nocopyable{
public:
  ConsumerGroup() = default;

  void addConsumer(const std::string & topic,const TcpConnection::ptr &conne);

private:
  int32_t m_id; ///消费者组id
  std::unordered_set<TcpConnection::ptr> m_consumers;
  std::unordered_map<std::string,std::list<Buffer>::iterator> m_topic_lastMsgIndex;  /// 记录每个topic 消费到的位置


};

#endif // MQSERVER_CONSUMERGROUP_H
