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
 if(argc < 2){
   SKYLU_LOG_INFO(G_LOGGER)<<"please enter type : produecer or consumer";
   exit(1);
 }
 G_LOGGER->setLevel(LogLevel::ERROR);
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



  const std::vector<std::string> topics{
      "hello0","hello1","hello2","hello3","hello4","hello5","hello6","hello7","hello8","hello9"
  };
    /////測試消息發送
  if(strcmp(argv[1],"producer")==0){

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


    SKYLU_LOG_FMT_WARN(G_LOGGER, "start to send,now: %d",
                       Timestamp::now().getMicroSeconds());
    /*
     *
     * 功能性测试
    for (int i = 0; i >=0; ++i) {

          int j = 0;
          for (const auto &it : vec) {
            it->put(topics[j++],
                    it->getName() + "  msgId = " + std::to_string(i));
            it->send(true);
          }
          sleep(1);
        }

     */

    int64_t  now = Timestamp::now().getMicroSeconds();
    uint64_t length = 0;
    while(Timestamp::now().getMicroSeconds() - now < Timestamp::kMicroSecondsPerSecond) {
        for (const auto &it : vec) {
         length += it->put("pretest_6", it->getName() + "  msgId =");
          it->send(true);
        }
    }
    for (const auto &it : vec) {
        it->wait();
      }
      SKYLU_LOG_FMT_WARN(G_LOGGER, "over to send,now: %d,interval = %d,length = %d",
                         Timestamp::now().getMicroSeconds(),now - Timestamp::now().getMicroSeconds(),length);
    ///消息发送完成后生产者线程退出去


  }else{


    if(argc < 3){

      SKYLU_LOG_INFO(G_LOGGER)<<"please enter id ";
      exit(1);
    }
    G_LOGGER->setLevel(LogLevel::INFO);
    EventLoop loop;
    Consumer consumer(&loop,peer_addrs,"consumer",500,atoi(argv[2]));
    consumer.subscribe(topics[2]);
    consumer.subscribe(topics[3]);
    consumer.subscribe(topics[1]);
    consumer.subscribe(topics[9]);
    consumer.subscribe(topics[4]);

    consumer.setPullCallback([&consumer](const Consumer::TopicMap & msg){
      for(const auto & it : msg){
        SKYLU_LOG_FMT_INFO(G_LOGGER,"recv topic: %s",it.first.c_str());
        std::vector<uint64_t> vec;
        for(const auto & i: it.second){
          if(vec.empty()){
            vec.push_back(i.offset);
          }else{
            if(vec.back()+1 != i.offset){
              SKYLU_LOG_FMT_ERROR(G_LOGGER,"########offset = %d msg = %s msgID = %d",i.offset,i.content.c_str(),i.messageId);
            }
            vec.push_back(i.offset);
            continue;
          }
          SKYLU_LOG_FMT_INFO(G_LOGGER,"////////offset = %d msg = %s msgID = %d",i.offset,i.content.c_str(),i.messageId);

        }
      }
      consumer.commit();

    });
    consumer.poll();

  }




  return 0;
}
