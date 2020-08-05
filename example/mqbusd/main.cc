//
// Created by jimlu on 2020/7/24.
//
#include "consumer.h"
#include "producer.h"
#include <memory>
#include <skylu/base/consistenthash.hpp>
#include <skylu/base/log.h>
#include <skylu/net/address.h>
#include <skylu/net/eventloop.h>
using namespace  skylu;
int main(int argc,char **argv)
{

 // G_LOGGER->setLevel(LogLevel::INFO);
  std::vector<Address::ptr> peer_addrs(2);
  peer_addrs[0] = IPv4Address::Create("127.0.0.1",6001);
  peer_addrs[1] = IPv4Address::Create("127.0.0.1",6002);


  /* 测试Producer的安全释放
  {
    ProducerWrap producer(peer_addrs,"Producer1");
    producer.start();
    producer.put("hello","first demo");
    producer.put("hello","ssss");
    producer.setSendCallback([](bool result,const Producer::ProduceQueue&queue){
      if(result){
        SKYLU_LOG_FMT_INFO(G_LOGGER,"all connection is vaild,queue's size = %d",queue.size());
      }
            });
    producer.setSendOkCallback([](const Producer::IdAndTopicMessagePair &msg){
      SKYLU_LOG_FMT_INFO(G_LOGGER,"msg[%u] topic:msg=%s:%s has successful.",msg.first,msg.second.first.c_str(),msg.second.second.c_str());

    });
    producer.send(false);

  }
   */


  /////測試消息發送

  {
    std::vector<std::unique_ptr<ProducerWrap>> vec(1);

    for (size_t i = 0; i < vec.size(); ++i) {
      vec[i].reset(
          new ProducerWrap(peer_addrs, "Producer[" + std::to_string(i) + "]"));
      vec[i]->setSendCallback(
          [](bool result, const Producer::ProduceQueue &queue) {
            if (!result) {
              SKYLU_LOG_FMT_INFO(G_LOGGER,
                                 "all connection is invaild,queue's size = %d",
                                 queue.size());
            }
          });
      vec[i]->setSendOkCallback(
          [&](const Producer::IdAndTopicMessagePair &msg) {
            // SKYLU_LOG_FMT_WARN(G_LOGGER,"send ok .now: %d",Timestamp::now());
          });
      vec[i]->start();
    }


    for (int i = 0; i < 10; ++i) {

      for (const auto &it : vec) {
        it->put("hello", it->getName() +"  msgId = "+ std::to_string(i));
      }
    }

    SKYLU_LOG_FMT_WARN(G_LOGGER, "start to send,now: %d",
                       Timestamp::now().getMicroSeconds());
    for (const auto &it : vec) {
      it->send(true);
    }

    for (const auto &it : vec) {
      it->wait();
    }
    SKYLU_LOG_FMT_WARN(G_LOGGER, "over to send,now: %d",
                       Timestamp::now().getMicroSeconds());
  }///消息发送完成后生产者线程退出去



  G_LOGGER->setLevel(LogLevel::DEBUG);
  EventLoop loop;
  Consumer consumer(&loop,peer_addrs,"consumer");
  consumer.subscribe("hello");
  consumer.setPullCallback([](const Consumer::TopicMap & msg){
    for(const auto & it : msg){
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"recv topic : %s",it.first.c_str());
      for(const auto & i: it.second){
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"//////// msg = %s",i.second.c_str());
      }
    }


          });
  consumer.poll(100);

  return 0;
}
