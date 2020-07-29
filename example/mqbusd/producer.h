//
// Created by jimlu on 2020/7/26.
//

#ifndef MQBUSD_PRODUCER_H
#define MQBUSD_PRODUCER_H

#include <memory>
#include <string>
#include <queue>
#include <skylu/base/snowflake.h>
#include <skylu/base/thread.h>
#include <skylu/net/channel.h>
#include <skylu/net/eventloop.h>
#include <skylu/net/channel.h>

#include "mqbusd.h"
using namespace skylu;
class ProducerWrap;

class Producer :public MqBusd,public std::enable_shared_from_this<Producer>{

  friend class ProducerWrap;
public:
  typedef std::pair<uint64_t ,TopicAndMessage > IdAndTopicMessagePair;
  typedef std::deque<IdAndTopicMessagePair> ProduceQueue;
  typedef std::function<void(bool,const ProduceQueue &)> SendCallback;
  typedef std::function<void(const IdAndTopicMessagePair &)> SendOkCallbck;
  typedef std::shared_ptr<Producer> ptr;
  ~Producer() override;
  bool send(bool isRetry = false);
  void put(const std::string& topic,const std::string& message);
  void setSendCallback(const SendCallback &cb){m_send_cb = cb;}
  void setSendOkCallback(const SendOkCallbck &cb){m_send_ok_cb = cb;}
  void start();
  void wait(){
    {
      Mutex::Lock lock(m_mutex);
      while(!m_send_queue.empty() || !m_resend_set.empty())
        m_empty_queueAndsResend.wait();
    }

  }
  int sendCount() const{return count;}

private:
  static Producer::ptr createProducer(const std::vector<Address::ptr> & dir_addrs,const std::string &name);
  Producer(const std::vector<Address::ptr> & dir_addrs,const std::string &name);
  void run();
  void stop();


  void sendInLoop(bool isRetry);

  void onMessageFromMqServer(const TcpConnection::ptr &conne,Buffer * buff)override ;
  void checkReSendSetTimer();
  void handleDelivery(const MqPacket *msg);





private:
  const int kReSendTimerWithMs = 500;
  Mutex m_mutex;
  std::unique_ptr<Thread> m_thread;
  ProduceQueue m_send_queue;  ///跨线程的 需要保护
  std::unordered_map<int,TopicAndMessage> m_resend_set;
  SendCallback  m_send_cb;
  SendOkCallbck  m_send_ok_cb;
  const std::vector<Address::ptr>  m_dir_addrs;
  bool m_isInit;  ///初始化的时候置位
  Condition m_empty_queueAndsResend;
  int count = 0;
};

class ProducerWrap{
public:
  ProducerWrap(const std::vector<Address::ptr> & dir_addrs,const std::string &name){
    produce = Producer::createProducer(dir_addrs,name);
  }
  ~ProducerWrap(){
    produce->stop();
  }
  bool send(bool isRetry = false){return produce->send(isRetry);}
  void put(const std::string& topic,const std::string& message){produce->put(topic,message);}
  void setSendCallback(const Producer::SendCallback &cb){produce->setSendCallback(cb);}
  void setSendOkCallback(const Producer::SendOkCallbck &cb){produce->setSendOkCallback(cb);}
  void start(){produce->start();}
  void wait(){produce->wait();}
  int sendCount(){return produce->sendCount();}
  std::string getName()const {return produce->getName();}




private:
  Producer::ptr produce;

};
#endif // MQBUSD_PRODUCER_H
