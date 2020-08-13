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
#include <yaml-cpp/yaml.h>
using namespace  skylu;
std::string random_string( size_t length )
{
  srand(time(NULL));
  auto randchar = []() -> char
  {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[ rand() % max_index ];
  };
  std::string str(length,0);
  std::generate_n( str.begin(), length, randchar );
  return str;
}


int main(int argc,char **argv) {

  if (argc < 2) {
    std::cout << "please enter load yaml file . eg : consumer.yaml"<<std::endl;
    exit(1);
  }
  YAML::Node config;
  try {
    config = YAML::LoadFile(argv[1]);
  } catch (YAML::BadFile &e) {
    std::cout << "read yaml error!" << std::endl;
    return -1;
  }
  G_LOGGER->setLevel(
      LogLevel::FromString(config["LogLevel"].as<std::string>()));

  std::vector<Address::ptr> peer_addrs(config["DirServer"].size());
  for (size_t i = 0; i < peer_addrs.size(); ++i) {
    peer_addrs[i] = IPv4Address::Create(
        config["DirServer"][i]["addr"].as<std::string>().c_str(),
        config["DirServer"][i]["port"].as<uint32_t>());
  }

  if (config["Type"].as<std::string>() == "consumer") {

    std::vector<std::string> topics(config["Topic"].size());
    for (size_t i = 0; i < topics.size(); ++i) {
      topics[i] = config["Topic"][i].as<std::string>();
    }
    EventLoop loop;
    Consumer consumer(&loop, peer_addrs, config["Name"].as<std::string>(),
                      config["PullIntervalMs"].as<int>(),
                      config["GroupId"].as<int>());
    for(auto it : topics){
      consumer.subscribe(it);
    }
    if(config["MaxEnableBytes"]){
      consumer.setMaxEnableBytes(config["MaxEnableBytes"].as<int>()*1024);
    }
    bool isEcho = false;
    if(config["EchoMessage"] && config["EchoMessage"].as<bool>()){
      isEcho = true;

    }
    consumer.setPullCallback([&consumer,&isEcho](const Consumer::TopicMap &msg) {
      static int64_t now = Timestamp::now().getMicroSeconds();
      if(isEcho) {
        for (const auto &it : msg) {
          SKYLU_LOG_FMT_INFO(G_LOGGER, "recv topic: %s", it.first.c_str());
          std::vector<uint64_t> vec;
          for (const auto &i : it.second) {
            if (vec.empty()) {
              vec.push_back(i.offset);
            } else {
              if (vec.back() + 1 != i.offset) {
                printf("################offset = %lu msg = %s msgID = %lu,interval = %ld\n",
                       i.offset, i.content.c_str(), i.messageId,
                       Timestamp::now().getMicroSeconds() - now);
                continue;
              }
              vec.push_back(i.offset);
            }
            printf("////////offset = %lu msg = %s msgID = %lu,interval = %f\n",
                   i.offset, i.content.c_str(), i.messageId,
                   (Timestamp::now().getMicroSeconds() - now) *
                       Timestamp::kSecondToMicroSeconds);
          }
        }
      }
      printf("*******interval = %f\n",(Timestamp::now().getMicroSeconds() - now) *
                                      Timestamp::kSecondToMicroSeconds);
      consumer.commit();
    });
    consumer.poll();

  }
  else if (config["Type"].as<std::string>() == "producer") {

    long double  total_time = 0;
    {
      std::vector<std::unique_ptr<ProducerWrap>> vec(config["Producer"].size());

      for (size_t i = 0; i < vec.size(); ++i) {
        vec[i].reset(new ProducerWrap(
            peer_addrs, config["Producer"][i]["name"].as<std::string>()));
        vec[i]->start();
        uint64_t maxLength =
                     config["Producer"][i]["totalLength"].as<int>() * 1024;

        for (size_t j = 0; j < config["Producer"][i]["topic"].size(); ++j) {
          uint length = 0;
          while (length < maxLength) {
            length += config["Producer"][i]["msgLength"].as<int>();
            vec[i]->put(
                config["Producer"][i]["topic"][j].as<std::string>(),
                random_string(config["Producer"][i]["msgLength"].as<int>()));
          }
        }
        vec[i]->send(true);
      }

      std::cout<<"******please wait for seconds*******"<<std::endl;
      for (const auto &it : vec) {
        it->wait();
      }
      for (const auto &it : vec) {
        total_time +=it->endSendTime()-it->startSendTime();
      }
    std::cout<<"*********Result************"<<std::endl;
    int producerNum =  vec.size();
    std::cout<<"avg interval =" <<
             (total_time/producerNum ) * Timestamp::kSecondToMicroSeconds<<std::endl;
    for(size_t i = 0 ;i < config["Producer"].size(); ++i){
      std::cout<<"ProducerName:"<<config["Producer"][i]["name"].as<std::string>()<<"   send count: "<<vec[i]->getSendCount()
                <<" delivery successful : "<<vec[i]->getDeliverSuccessfulCount()<<std::endl;
    }
    std::cout<<"***************************"<<std::endl;
    }

    ///消息发送完成后生产者线程退出去
  }
  else if(config["Type"].as<std::string>() == "producer-console")
  {
    std::string topic,msg;
    std::unique_ptr<ProducerWrap> producer(new ProducerWrap(peer_addrs,"Producer Console"));
    producer->start();
    std::cout<<"********Producer-Console********"<<std::endl;
    std::cout<<"please enter topic : ";
    std::cin>>topic;
    while(1){
      std::cout<<"please enter msg : ";
      std::cin>>msg;
      producer->put(topic,msg);
      producer->send(true);
      msg.clear();

      }


    return 0;

  }
  else {

    std::cout << "error type" << std::endl;
    return -1;
  }
  return 0;
}
