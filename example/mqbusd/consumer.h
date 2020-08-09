//
// Created by jimlu on 2020/7/26.
//

#ifndef MQBUSD_CONSUMER_H
#define MQBUSD_CONSUMER_H
#include <string>
#include <deque>
#include <skylu/base/snowflake.h>
#include <skylu/proto/mq_proto.h>
#include <skylu/base/consistenthash.hpp>
#include "mqbusd.h"
struct CommitInfo{
  CommitInfo()=default;
  ~CommitInfo()=default;
  uint64_t  getOffset(const std::string &conneName)const {
    auto it = info.find(conneName);
    if(it == info.end()){
     return  0;
    }
    return it->second;
  }

  void incOffset(const std::string & conneName){
    info[conneName]++;
  }

  void setOffset(const std::string & conneName,uint64_t offset){
    if(info.find(conneName) != info.end()){
      if(info[conneName] >= offset){
        SKYLU_LOG_FMT_WARN(G_LOGGER,"set offset[%d] is small then local commit[%d]",offset,info[conneName]);

      }
    }
    info[conneName] = offset;
  }
  std::unordered_map<std::string,uint64_t> info;


};

struct MessageInfo{
  MessageInfo(uint64_t messageID_t,uint64_t offset_t,const std::string & msg)
  :messageId(messageID_t),offset(offset_t),content(msg)
  {}
  uint64_t  messageId;
  uint64_t  offset;
  std::string content;
};
class Consumer :public MqBusd
{
public:
  typedef std::unordered_map<std::string,std::deque<MessageInfo>> TopicMap;  ///topic - id+message
  typedef std::function<void(std::string)> SubscribeCallback;
  typedef std::function<void(const TopicMap & msg)> PullCallback;

  Consumer(EventLoop * loop,const std::vector<Address::ptr> & dir_addrs,const std::string &name
           ,int pullIntervalMs,int id  = -1);
  ~Consumer() override = default;
  /**
   *  订阅(发送动作在subscribeInLoop)
   * @param topic
   */
  void subscribe(const std::string &topic);
  /**
   * 取消订阅(直接发送),如果连接失效的话就什么都不做
   * @param topic
   * @return
   */
  void cancelSubscribe(const std::string& topic);
  /**
   * 需要调用这个才能连接上broker
   */
  void poll();
  /**
   * 提交全部
   */
  void commit();
  /**
   * 设置具体连接拉取的offset
   * @param topic
   * @param conneName
   * @param offset
   */
  void pull(const std::string& topic,const std::string &conneName,uint64_t offset);
  /**
   * 获得接收到的message
   * @return
   */
  TopicMap getMessage(){return m_recv_messages;}
  /**
   * 订阅成功后的回调函数
   * @param cb
   */
  void setSubscribeCallback(const SubscribeCallback &cb){m_subscribe_cb = cb;}
  /**
   *  取消订阅成功后的回调函数
   * @param cb
   */
  void setCancelSubscribeCallback(const SubscribeCallback &cb){m_cancel_subscribe_cb = cb;}
  /**
   *  主要的函数。 用于拉取到消息之后的回调
   * @param cb
   */
  void setPullCallback(const PullCallback & cb){ m_pull_cb = cb;}
  void setMaxEnableBytes(int64_t byte){m_maxEnableBytes = byte;}

private:
  /**
   * 消费者需要重载这个函数，成功连接之后马上订阅
   * @param conne
   */
  void onConnectionToMqServer(const TcpConnection::ptr &conne) override;
  void onMessageFromMqServer(const TcpConnection::ptr &conne, Buffer *buff) override;
  void subscribeInLoop(const TcpConnection::ptr &conne);
  /**
   * 当获取到了Broker的Info根据本地的订阅信息来决定连接到哪台broker上
   */
  void connectToMqServer()override ;
  /**
   *  如果本地保存的m_commit_offset 为空的话就根据远端的提交记录来消费。
   *
   */
  void pullInLoop();

  /**
   * 同时处理订阅成功和取消订阅的包
   * @param msg
   * @param conne
   */
  void handleSubscribe(const MqPacket * msg,const TcpConnection::ptr &conne);
  void handlePull(const MqPacket *msg,const TcpConnection::ptr &conne);
  void removeInvaildConnection(const TcpConnection::ptr &conne);

private:
  std::unordered_map<std::string,std::string> m_topic; ////订阅 - 对应的host:port
  std::unordered_map<std::string, TcpConnection::ptr> m_vaild_topic_conne;
  int32_t m_groupId;  ///远端唯一标识consumer offset

  std::unordered_map<std::string,CommitInfo> m_commit_offset; ///本地保留一份
  std::unordered_map<int,std::string> m_recv_messageId;  /// 主要用来判重的。
  TopicMap  m_recv_messages; /// 接受到的消息 ，当commit 的时候会移除对应的就message
  int64_t m_maxEnableBytes;
  SubscribeCallback m_subscribe_cb;
  SubscribeCallback m_cancel_subscribe_cb;
  PullCallback  m_pull_cb;
  int m_pullIntervalMs; /// 拉取间隔



};

#endif // MQBUSD_CONSUMER_H
