//
// Created by jimlu on 2020/7/26.
//

#include "consumer.h"
Consumer::Consumer(EventLoop * loop,const std::vector<Address::ptr> &dir_addrs,
                   const std::string &name,int pullIntervalMs,int id)
    :MqBusd(loop,dir_addrs,name)
    ,m_groupId(id),m_maxEnableBytes(-1),m_pullIntervalMs(pullIntervalMs){
  if(id == -1){
    MurMurHash hash;
    m_groupId = hash(name);
  }

}
void Consumer::onMessageFromMqServer(const TcpConnection::ptr &conne,
                                     Buffer *buff) {
  bool hasPull = false;
  while(buff->readableBytes()>sizeof(MqPacket)) {
    const MqPacket *msg = serializationToMqPacket(buff);
    if(msg == nullptr){
      //// FIXME 解析为空 也许对面发了空包过来。sendfile有关
      break;
    }
    switch (msg->command) {
    case MQ_COMMAND_CANCEL_SUBSCRIBE:
    case MQ_COMMAND_SUBSCRIBE:
      handleSubscribe(msg,conne);
      break;
    case MQ_COMMAND_PULL:
      handlePull(msg,conne);
      hasPull = true;
      break;
    case MQ_COMMAND_COMMIT:
      SKYLU_LOG_DEBUG(G_LOGGER)<<"commit successful";
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

  m_topic.insert({topic,""});

}
void Consumer::poll() {

  MqBusd::connect();
  m_loop->loop();

}
void Consumer::subscribeInLoop(const TcpConnection::ptr& conne) {
  Buffer buff;
  std::vector<MqPacket> vec;
  std::unordered_set<std::string> topic;
  for(const auto & it : m_topic){
    std::string name = conne->getName();
    int start = name.find(':')+1;
    int end = name.find('#');
    std::string addr = name.substr(start,end-start);
    if(!it.second.empty()&&addr == it.second){
      MqPacket msg;
      msg.msgCreateTime = Timestamp::now().getMicroSeconds();
      msg.command = MQ_COMMAND_SUBSCRIBE;
      msg.topicBytes = it.first.size();
      msg.consumerGroupId = m_groupId;
      vec.push_back(msg);
      topic.insert(it.first);
      SKYLU_LOG_FMT_INFO(G_LOGGER,"describe topic[%s] to conne[%s]",it.first.c_str(),conne->getName().c_str());
    }
  }

  serializationToBuffer(vec,topic,buff);
  conne->send(&buff);

}
void Consumer::cancelSubscribe(const std::string &topic) {
  if(m_vaild_topic_conne.find(topic) == m_vaild_topic_conne.end() || !m_vaild_topic_conne[topic]){
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"topic[%s] conne is invaild.",topic.c_str());
    return ;
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



}
void Consumer::commit() {


  if(m_recv_messages.empty()){
    return ;
  }
  std::vector<MqPacket> vec(m_recv_messages.size());
  int i =0;
  for(const auto & it : m_recv_messages){
    /// 提交每个主题收到的最后一个消息
    if(it.second.empty()){
      continue;
    }
    Buffer buff;
    vec[i].command = MQ_COMMAND_COMMIT;
    vec[i].consumerGroupId = m_groupId;
    vec[i].offset = it.second.back().offset + 1;
    vec[i].topicBytes = it.first.size();
    serializationToBuffer(&(vec[i]),it.first,buff);
    const auto & conne = m_vaild_topic_conne[it.first];
    conne->send(&buff);

    m_commit_offset[it.first].setOffset(conne->getName(),vec[i].offset);
    ++i;
  }
  m_recv_messages.clear();

}
void Consumer::pullInLoop() {

  MqPacket msg{};
  msg.command = MQ_COMMAND_PULL;
  msg.maxEnableBytes = m_maxEnableBytes;
  msg.msgCreateTime = Timestamp::now().getMicroSeconds();
  msg.consumerGroupId = m_groupId;
  msg.offset = 0;

  if(!m_commit_offset.empty()){
    for(const auto & it: m_commit_offset){
      if(m_vaild_topic_conne.find(it.first) == m_vaild_topic_conne.end()){
        SKYLU_LOG_FMT_ERROR(G_LOGGER,"topic[%s] conne is invaild.",it.first.c_str());
      }else{
        Buffer buff;
        const auto & conne = m_vaild_topic_conne[it.first];
        msg.topicBytes = it.first.size();
        msg.offset = it.second.getOffset(conne->getName());
        serializationToBuffer(&msg,it.first,buff);
        conne->send(&buff);
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"pull topic[%s|offset: %d]",it.first.c_str(),msg.offset);
      }

    }
  }else{
    for(const auto & i : m_mqserver_clients){
      Buffer buff;
      serializationToBuffer(&msg,buff);
      const auto & conne = i.second->getConnection();
      if(conne){
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"pull message to conne[%s]",conne->getName().c_str());
        conne->send(&buff);
      }

    }
  }
}

void Consumer::onConnectionToMqServer(const TcpConnection::ptr &conne) {
  MqBusd::onConnectionToMqServer(conne);
  subscribeInLoop(conne);
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
      m_loop->runEvery(m_pullIntervalMs *Timestamp::kSecondToMilliSeconds,std::bind(&Consumer::pullInLoop,this));
    }else{
      /// 服务端对取消订阅的响应
      SKYLU_LOG_FMT_INFO(G_LOGGER,"success cancel subscribe topic %s",topic.c_str());
      if(m_cancel_subscribe_cb){
        m_cancel_subscribe_cb(topic);
      }
      m_vaild_topic_conne.erase(topic);
      m_topic.erase(topic);
    }
  }else if(msg->retCode == ErrCode::UNKNOW_TOPIC){
    SKYLU_LOG_ERROR(G_LOGGER)<<"send invaild describe to conne[%s].",conne->getName().c_str();
  }else{
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild reCode =%d",msg->retCode);
  }
}
void Consumer::handlePull(const MqPacket *msg,const TcpConnection::ptr &conne) {

  if(msg->retCode == ErrCode::UNKNOW_CONSUMER){
    SKYLU_LOG_WARN(G_LOGGER)<<"subscribe timeout . repeat send subscribe";
    subscribeInLoop(conne);
  }
  if(msg->retCode == ErrCode::SUCCESSFUL){

    std::string topic,message;
    getTopicAndMessage(msg,topic,message);
    if(m_recv_messageId.find(msg->messageId) == m_recv_messageId.end()) {
      m_recv_messageId[msg->messageId] = topic;
      m_recv_messages[topic].push_back(MessageInfo(msg->messageId,msg->offset,message));
      SKYLU_LOG_FMT_DEBUG(G_LOGGER, "pull a message[%d|%s][offset= %d] from topic[%s]",
                          msg->messageId, topic.c_str(),msg->offset, message.c_str());
    }else{
      ///收到重复消息 可能是因为consumer还在处理消息
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"recv a repeat message[%d|%s][offset=%d] from topic[%s]",
                          msg->messageId,message.c_str(),msg->offset,topic.c_str());
      if(msg->offset == m_commit_offset[topic].getOffset(conne->getName())){
        /// TODO  出现的原因： broker 重启，producer 将重传队列的数据发送过去了。但是broker本地上是已经持久化了这部分的内容，只是还没有ack到producer
        m_commit_offset[topic].incOffset(conne->getName());
      }
    }

  }else{
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"invaild reCode =%d",msg->retCode);
  }
}


void Consumer::connectToMqServer() {

  static int i = 0;
  for( auto &  topic : m_topic) {
    ///topic.first = topic, second = host

    for (auto it : m_mqserver_info) {
      /// it.first = host:port , second = std::unorder_map<topic,num>
      /// 添加最新的Client FIXME  这里会有性能的问题
      if (it.second.find(topic.first) != it.second.end()){  ///找到有相关主题的broker 进行连接

        topic.second =  it.first;
        if(m_mqserver_clients.find(it.first) == m_mqserver_clients.end()) {

              int flag = it.first.find(':');
              std::string host(it.first.substr(0, flag++));
              int port =
                  atoi(it.first.substr(flag, it.first.size() - flag).c_str());
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
              m_mqserver_clients[it.first]->setCloseCallback(std::bind(&Consumer::removeInvaildConnection,
                                                                       this,std::placeholders::_1));
        }
      }
    }
  }
}
void Consumer::pull(const std::string &topic,const std::string & conneName, uint64_t offset) {
  m_commit_offset[topic].setOffset(conneName,offset);
}
void Consumer::removeInvaildConnection(const TcpConnection::ptr &conne)  {
  /// FIXME 有点累赘
  std::vector<std::string> invaild;
  for(auto & it : m_vaild_topic_conne){
    if(it.second == conne){
      invaild.push_back(it.first);
    }
  }
  for(const auto & it : invaild){
    m_vaild_topic_conne.erase(it);
  }



}
