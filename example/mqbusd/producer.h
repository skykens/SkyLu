//
// Created by jimlu on 2020/7/26.
//

#ifndef MQBUSD_PRODUCER_H
#define MQBUSD_PRODUCER_H

#include <memory>
#include <queue>
#include <skylu/base/consistenthash.hpp>
#include <skylu/base/snowflake.h>
#include <skylu/base/thread.h>
#include <skylu/net/channel.h>
#include <skylu/net/eventloop.h>
#include <string>

#include "mqbusd.h"
using namespace skylu;
class ProducerWrap;

/**
 * 这个类是不能被直接创建的，如果直接创建的话需要每次调用stop之后才能安全的释放，否则线程中的eventloop无法停止。放在析构函数里面也不行,析构要回收线程
 */
class Producer :protected MqBusd,public std::enable_shared_from_this<Producer>{
  const int kVirtualNodeNum = 50;

  friend class ProducerWrap;
public:
  typedef std::pair<uint64_t ,TopicAndMessage > IdAndTopicMessagePair;
  typedef std::deque<IdAndTopicMessagePair> ProduceQueue;
  typedef std::function<void(bool,const ProduceQueue &)> SendCallback;
  typedef std::function<void(const IdAndTopicMessagePair &)> SendOkCallbck;
  typedef std::shared_ptr<Producer> ptr;
  ~Producer() override;
  /**
   * 将本地队列的消息发送出去
   * @param isRetry 发送失败是否重试
   * @return
   */
  bool send(bool isRetry = false);
  /**
   * 将对应的主题消息放到发送队列里
   * @param topic
   * @param message
   */
  void put(const std::string& topic,const std::string& message);
  /**
   * 发送之后的回调
   */
  void setSendCallback(const SendCallback &cb){m_send_cb = cb;}

  /**
   * 消息成功投递后的回调
   * @param cb
   */
  void setSendOkCallback(const SendOkCallbck &cb){m_send_ok_cb = cb;}
  /**
   * 线程启动。
   */
  void start();
  /**
   * 阻塞的等待消息发送完成
   */
  void wait(){
    {
      Mutex::Lock lock(m_mutex);
      while(!m_send_queue.empty() || !m_resend_set.empty())
        m_empty_queueAndsResend.wait();
    }

  }

  /**
   * 开始发送消息的时间
   * @return
   */
  int64_t  startSendTime(){return m_begin_send_time;}

private:
  static Producer::ptr createProducer(const std::vector<Address::ptr> & dir_addrs,const std::string &name);
  Producer(const std::vector<Address::ptr> & dir_addrs,const std::string &name);
  void run();
  void stop();


  void sendInLoop(bool isRetry);

  void onMessageFromMqBroker(const TcpConnection::ptr &conne,Buffer * buff)override ;
  void checkReSendSetTimer();
  void handleDelivery(const MqPacket *msg);
  void onConnectionToMqBroker(const TcpConnection::ptr & conne)override;
  void connectToMqBroker()override ;
  void removeInvaildConnection(const TcpConnection::ptr & conne);





private:
  const int kReSendTimerWithMs = 2000;
  Mutex m_mutex;
  std::unique_ptr<Thread> m_thread;
  ProduceQueue m_send_queue;  ///跨线程的 需要保护
  std::map<int,TopicAndMessage> m_resend_set;
  SendCallback  m_send_cb;
  SendOkCallbck  m_send_ok_cb;
  const std::vector<Address::ptr>  m_dir_addrs;
  bool m_isInit;  ///初始化的时候置位
  Condition m_empty_queueAndsResend;
  int count = 0;
  consistent_hash_map<> m_hashHost;
  std::unordered_map<std::string,TcpConnection::ptr > m_vaild_conne;
  int64_t  m_begin_send_time = 0;
};

/**
 * 这个类用来包裹Producer，当他析构的时候手动会调用produce->stop来停止eventloop，之后线程就能安全的释放
 */
class ProducerWrap{
public:
  ProducerWrap(const std::vector<Address::ptr> & dir_addrs,const std::string &name){
    produce = Producer::createProducer(dir_addrs,name);
  }
  ~ProducerWrap(){
    produce->stop();
  }
  bool send(bool isRetry = false){return produce->send(isRetry);}
  uint64_t put(const std::string& topic,const std::string& message){produce->put(topic,message);return message.size() + topic.size() + sizeof(MqPacket);}
  void setSendCallback(const Producer::SendCallback &cb){produce->setSendCallback(cb);}
  void setSendOkCallback(const Producer::SendOkCallbck &cb){produce->setSendOkCallback(cb);}
  void start(){produce->start();}
  void wait(){produce->wait();}
  std::string getName()const {return produce->getName();}
  int64_t  startSendTime(){return produce->startSendTime();}




private:
  Producer::ptr produce;

};
#endif // MQBUSD_PRODUCER_H
