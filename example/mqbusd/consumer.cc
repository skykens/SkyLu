//
// Created by jimlu on 2020/7/26.
//

#include "consumer.h"
Consumer::Consumer(EventLoop * loop,const std::vector<Address::ptr> &dir_addrs,
                   const std::string &name,int id)
    :MqBusd(loop,dir_addrs,name)
    ,m_groupId(id),m_maxEnableBytes(0),m_minEnableBytes(-1){

}
void Consumer::onMessageFromMqServer(const TcpConnection::ptr &conne,
                                     Buffer *buff) {
  bool hasPull = false;
  while(buff->readableBytes()>sizeof(MqPacket)) {
    const MqPacket *msg = serializationToMqPacket(buff);
    switch (msg->command) {
    case MQ_COMMAND_COMMIT: ///确认COMMIT
      handleCommit(msg,conne);
        break;
    case MQ_COMMAND_SUBSCRIBE:
      handleSubscribe(msg,conne);
      break;
    case MQ_COMMAND_CANCEL_SUBSCRIBE:
      handleCancelSubscribe(msg,conne);
      break;
    case MQ_COMMAND_PULL:
      handlePull(msg,conne);
      hasPull = true;
      break;
    default:
      SKYLU_LOG_FMT_ERROR(G_LOGGER, "invaild command[%d] form conne[%s] ",
                          msg->command, conne->getName().c_str());
      break;
    }
  }
  if(hasPull && m_pull_cb){
    m_pull_cb(m_recv_messages);
  }

}

void Consumer::subscribe(const std::string &topic) {

  m_topic.insert(topic);

}
void Consumer::poll(int timeoutMs) {

  MqBusd::connect();
  m_loop->runEvery(kSendKeepAliveToMqMs,std::bind(&Consumer::sendKeepAliveWithMs,this));
  m_loop->loop();

}
void Consumer::subscribe(const TcpConnection::ptr& conne) {
  Buffer buff;
  std::vector<MqPacket> vec(m_topic.size());
  int i = 0;
  for(const auto & it : m_topic){
    vec[i].messageId = createMessageId();
    vec[i].msgCreateTime = Timestamp::now().getMicroSeconds();
    vec[i].retCode = ErrCode::SUCCESSFUL;
    vec[i].command = MQ_COMMAND_SUBSCRIBE;
    vec[i].topicBytes = it.size();
    vec[i].consumerGroupId = m_groupId;
    ++i;
  }
  serializationToBuffer(vec,m_topic,buff);
  conne->send(&buff);

}
bool Consumer::cancelSubscribe(const std::string &topic) {
  if(m_vaild_topic_conne.find(topic) == m_vaild_topic_conne.end() || !m_vaild_topic_conne[topic]){
    SKYLU_LOG_ERROR(G_LOGGER)<<"topic subscribe faild.";
    return false;
  }
  MqPacket msg{};
  Buffer buff;
  msg.consumerGroupId = m_groupId;
  msg.command = MQ_COMMAND_CANCEL_SUBSCRIBE;
  msg.topicBytes = topic.size();
  msg.msgBytes = 0;
  msg.msgCreateTime = Timestamp::now().getMicroSeconds();
  serializationToBuffer(&msg,buff);
  m_vaild_topic_conne[topic]->send(&buff);
  m_vaild_topic_conne.erase(topic);

  return true;


}
void Consumer::commit(int messageId) {
  assert(m_recv_messageId.find(messageId) != m_recv_messageId.end()); ///提交的id必须是保存在里面的messageId
  assert(m_recv_messages.find(m_recv_messageId[messageId]) != m_recv_messages.end());
  MqPacket msg{};
  Buffer buff;
  msg.messageId = messageId;
  msg.msgCreateTime = Timestamp::now().getMicroSeconds();
  msg.consumerGroupId = m_groupId;
  msg.command = MQ_COMMAND_COMMIT;
  serializationToBuffer(&msg,buff);
  m_vaild_topic_conne[m_recv_messageId[messageId]]->send(&buff);

  m_recv_messages.erase(m_recv_messageId[msg.messageId]);

}
void Consumer::commit() {
    std::unordered_map<std::string,MqPacket> vec;
    for(const auto & it : m_recv_messageId){

      vec[it.second].messageId = it.first;
      vec[it.second].msgCreateTime = Timestamp::now().getMicroSeconds();
      vec[it.second].consumerGroupId =m_groupId;
      vec[it.second].command = MQ_COMMAND_COMMIT;

    }
    for(auto &it : vec){
      Buffer buff;
      serializationToBuffer(&it.second,buff);
      if(m_vaild_topic_conne[it.first]){
        m_vaild_topic_conne[it.first]->send(&buff);
      }
    }
  m_recv_messages.clear();


}
void Consumer::sendKeepAliveWithMs() {

  Buffer buff;
  MqPacket msg{};
  msg.command = MQ_COMMAND_HEARTBEAT;
  msg.consumerGroupId = m_groupId;
  serializationToBuffer(&msg,buff);
  for(const auto & i : m_mqserver_clients){

    const auto & conne = i.second->getConnection();
    if(conne){
      conne->send(&buff);
    }

  }

}
void Consumer::pull(int timeoutMs) {
  MqPacket msg{};
  Buffer buff;
  msg.command = MQ_COMMAND_PULL;
  msg.consumerGroupId = m_groupId;
  msg.maxEnableBytes = m_maxEnableBytes;
  msg.minEnableBytes = m_minEnableBytes;
  msg.msgCreateTime = Timestamp::now().getMicroSeconds();
  msg.pullInteval = timeoutMs;
  msg.consumerGroupId = m_groupId;
  serializationToBuffer(&msg,buff);

  for(const auto & i : m_mqserver_clients){

    const auto & conne = i.second->getConnection();
    if(conne){
      conne->send(&buff);
    }

  }


}
void Consumer::onCloseConnection(const TcpConnection::ptr &conne) {
  for(const auto it = m_vaild_topic_conne.begin();it!=m_vaild_topic_conne.end();){
    ////可能出现一个conne 有多个topic情况
    if(it->second == conne){
      it->second.reset(); /// 避免内存泄露
    }
  }
}
void Consumer::onConnectionToMqServer(const TcpConnection::ptr &conne) {
  MqBusd::onConnectionToMqServer(conne);
  subscribe(conne);
}
void Consumer::handleSubscribe(const MqPacket *msg,const TcpConnection::ptr &conne) {

  /// 注冊成功
  if(msg->retCode == ErrCode::SUCCESSFUL){
    std::string topic;
    getTopic(msg,topic);
    if(msg->command == MQ_COMMAND_SUBSCRIBE){
      SKYLU_LOG_FMT_INFO(G_LOGGER,"success subscribe topic %s",topic.c_str());
      if(m_subscribe_cb){
        m_subscribe_cb(topic);
      }
      m_vaild_topic_conne.insert({topic,conne});
      m_loop->runInLoop(std::bind(&Consumer::pull,this,10));
    }else{
      /// 服务端对取消订阅的响应
      SKYLU_LOG_FMT_INFO(G_LOGGER,"success cancel subscribe topic %s",topic.c_str());
      if(m_cancel_subscribe_cb){
        m_cancel_subscribe_cb(topic);
      }
      m_vaild_topic_conne.erase(topic);
      m_topic.erase(topic);
    }
  }else{
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild reCode =%d",msg->retCode);
  }
}
void Consumer::handlePull(const MqPacket *msg,const TcpConnection::ptr &conne) {

  if(msg->retCode == ErrCode::UNKNOW_CONSUMER){
    ///没有维持心跳 超时了
    subscribe(conne);
  }
  if(msg->retCode == ErrCode::SUCCESSFUL){

    std::string topic,message;
    getTopicAndMessage(msg,topic,message);
    if(m_recv_messageId.find(msg->messageId) == m_recv_messageId.end()) {
      m_recv_messageId[msg->messageId] = topic;
      m_recv_messages[topic].push_back({msg->messageId, message});
      SKYLU_LOG_FMT_DEBUG(G_LOGGER, "pull a message[%d|%s] from topic[%s]",
                          msg->messageId, topic.c_str(), message.c_str());
    }else{
      ///收到重复消息 可能是因为consumer还在处理消息
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"recv a repeat message[%d|%s] from topic[%s]",
                          msg->messageId,topic.c_str(),message.c_str());

    }

  }else{
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild reCode =%d",msg->retCode);
  }
}
void Consumer::handleCancelSubscribe(const MqPacket *msg,const TcpConnection::ptr &conne) {

}
void Consumer::handleCommit(const MqPacket *msg,const TcpConnection::ptr &conne) {

  assert(m_recv_messageId.find(msg->messageId) != m_recv_messageId.end());
  m_recv_messageId.erase(msg->messageId);  ///直到这个时候才要删除message 之前都还在处理。
}
void Consumer::connectToMqServer() {

  static int i = 0;
  for(auto topic : m_topic) {

    for (auto it : m_mqserver_info) {
      /// 添加最新的Client FIXME  这里会有性能的问题
      if (it.second.find(topic) != it.second.end()  ///找到有相关主题的broker 进行连接
          &&m_mqserver_clients.find(it.first) == m_mqserver_clients.end()) {
        int flag = it.first.find(':');
        std::string host(it.first.substr(0, flag++));
        int port = atoi(it.first.substr(flag, it.first.size() - flag).c_str());
        Address::ptr addr = IPv4Address::Create(host.c_str(), port);
        m_mqserver_clients[it.first].reset(new TcpClient(
            m_loop, addr, "mqServerClient Id" + std::to_string(++i)));

        SKYLU_LOG_FMT_INFO(G_LOGGER,
                           "initMqServerClients| client host : %s port : %d",
                           host.c_str(), port);

        m_mqserver_clients[it.first]->connect();
        m_mqserver_clients[it.first]->setConnectionCallback(std::bind(
            &Consumer::onConnectionToMqServer, this, std::placeholders::_1));
        m_mqserver_clients[it.first]->setMessageCallback(
            std::bind(&Consumer::onMessageFromMqServer, this,
                      std::placeholders::_1, std::placeholders::_2));
      }
    }
  }
}
