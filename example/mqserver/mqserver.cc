//
// Created by jimlu on 2020/7/22.
//

#include "mqserver.h"
#include <skylu/base/daemon.h>
#include <string.h>
#include <string>
MqServer::MqServer(EventLoop *loop, const Address::ptr &local_addr,
                   std::vector<Address::ptr> &peerdirs_addr,
                   const std::string &name)
    :m_name(name)
    ,m_loop(loop)
    ,m_server(loop,local_addr,name + "#TcpServer#")
      ,m_dirpeer_clients(peerdirs_addr.size())
    ,m_local_addr(local_addr){

  Signal::hook(SIGPIPE,[](){});
  for(size_t i = 0 ; i < peerdirs_addr.size(); ++i){
    m_dirpeer_clients[i].reset(new TcpClient(m_loop
                                             ,peerdirs_addr[i]
                                             ,"#DirPeerClientID #" + std::to_string(i)));
    m_dirpeer_clients[i]->enableRetry();
    m_dirpeer_clients[i]->setConnectionCallback(std::bind(&MqServer::sendRegisterWithMs,this,std::placeholders::_1));
    m_dirpeer_clients[i]->setMessageCallback(std::bind(&MqServer::onMessageFromDirPeer
                                                       ,this,std::placeholders::_1,std::placeholders::_2));


  }
  m_commit_partition.reset(new CommitPartition(".commit"+m_name));

  //// 定时器任务 ：持久化提交数据
  m_loop->runEvery(kPersistentCommmitTimeMs *Timestamp::kSecondToMilliSeconds,
                   std::bind(&MqServer::persistentCommitWithMs,this));

  /// 定时器任务  ： 持久化分区数据
  m_loop->runEvery(kPersistentTimeMs *Timestamp::kSecondToMilliSeconds,
                   std::bind(&MqServer::persistentPartitionWithMs,this));

  /// 定时器任务  ： 发送心跳
  m_loop->runEvery(kSendKeepAliveMs * Timestamp::kSecondToMilliSeconds,
                   std::bind(&MqServer::sendKeepAliveWithMs, this));

  m_server.setMessageCallback(std::bind(&MqServer::onMessageFromMqBusd
                                      ,this,std::placeholders::_1,std::placeholders::_2));
  m_server.setCloseCallback(std::bind(&MqServer::removeInvaildConnection,
                                      this,std::placeholders::_1)); ///移除无效连接
}

void MqServer::init() {
  std::vector<std::string> allfile;
  DIR * curDir = opendir(".");
  struct dirent * dp = nullptr;
  if(curDir){
    while((dp = readdir(curDir)) != nullptr){
      if(dp->d_type == DT_DIR){
        if(!strcmp(dp->d_name,".")||!strcmp(dp->d_name,"..")){
          continue;
        }
        std::string dirname(dp->d_name);
        int index = dirname.find('-');
        if(index == -1){
          SKYLU_LOG_FMT_ERROR(G_LOGGER,"error dir[%s] exist. ",dirname.c_str());
          continue ;
        }
        int id = std::stoi(dirname.substr(index + 1));
        std::string topicName = dirname.substr(0,index);
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"load partition[%s-%d]",topicName.c_str(),id);
        m_partition[topicName].reset(new Partition(topicName,id));
      }
    }

  }
  for(auto & m_dirpeer_client : m_dirpeer_clients){
    m_dirpeer_client->connect();

  }


}



void MqServer::run() {
  init();
  m_server.start();
  m_loop->loop();
}
void MqServer::sendRegisterWithMs(const TcpConnection::ptr &conne) {
  Buffer buff;
  char tmp[sizeof(DirServerPacket)];
  size_t length = 0;
  std::string host = m_local_addr->toString();
  buff.append(tmp,sizeof(DirServerPacket));
  buff.append(m_local_addr->toString() + "\r\n");
  length += host.size() + 2;
  for(const auto &  it : m_partition){
    std::string str = it.first + ":" + std::to_string(it.second->getSize()) + "\r\n";
    buff.append(str);
    length += str.size() ;
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"append to register buffer  str = %s ",str.c_str());
  }
  const auto* msg_const = reinterpret_cast<const DirServerPacket *>(buff.curRead());
  auto * msg = const_cast<DirServerPacket * >(msg_const);
  msg->command = COMMAND_REGISTER;
  msg->Msgbytes = length;
  conne->send(&buff);

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send RegisterRequest to conne[%s]",conne->getName().c_str());

}
void MqServer::sendKeepAliveWithMs() {
  for(const auto& it : m_dirpeer_clients){
    const auto & conne = it->getConnection();
    if(conne){

      sendRegisterWithMs(conne);
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send register to conne[%s]",it->getName().c_str());

    }
  }

}

void MqServer::onMessageFromDirPeer(const TcpConnection::ptr &conne,Buffer *buff) {
 const  auto * msg = reinterpret_cast<const DirServerPacket*>(buff->curRead());

 buff->updatePos(sizeof(DirServerPacket));


 if(msg->command == DirProto::COMMAND_ACK){
   /// 处理确认包
     /// 开始发送心跳保活
     static bool  isVaild = false;
     if(!isVaild) {
     SKYLU_LOG_FMT_DEBUG(G_LOGGER, "send registerWithMs[%s] to peers[num = %d] ",
                         kSendKeepAliveToMqMs,m_dirpeer_clients.size());
     m_loop->runEvery(kSendKeepAliveMs * Timestamp::kSecondToMilliSeconds,
                      std::bind(&MqServer::sendRegisterWithMs, this,conne));
     isVaild = true;
   }

   }else{
   SKYLU_LOG_FMT_ERROR(G_LOGGER,"INVALILD COMMAND = %d from conne[%s]"
                       ,msg->command,conne->getName().c_str());
 }



}
void MqServer::onMessageFromMqBusd(const TcpConnection::ptr &conne,
                                   Buffer *buff) {
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"msg from conne[%s]",conne->getName().c_str());
  if(buff->readableBytes() < sizeof(MqPacket)){
    conne->shutdown();
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"conne[%s] send error packet size[%d]",conne->getName().c_str(),buff->readableBytes());
    return ;
  }
  while(buff->readableBytes()>sizeof(MqPacket)) {
    const MqPacket *msg = serializationToMqPacket(buff); ///这里面会更新
    if(!msg){
      ///FIXME  目前发现的原因是生产者发送的数据太大了，导致需要分段接受 。 目前的解决方法是先shutdown,降低重发队列的大小 之后再重新建立连接发送》
      conne->shutdown();
      SKYLU_LOG_FMT_WARN(G_LOGGER,"conne[%s] send too larget packet size[%d]",conne->getName().c_str(),buff->getWPos());
      return ;
    }
    switch (msg->command) {
    case MQ_COMMAND_DELIVERY:
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv msg->command = DELIVERY",conne->getName().c_str());
      /// 投递
      handleDeliver(msg);
      simpleSendReply(msg->messageId,MQ_COMMAND_DELIVERY,conne);
      break;
    case MQ_COMMAND_PULL:
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv msg->command = PULL",conne->getName().c_str());
      /// 拉取消息
      handlePull(conne,msg);
      break;
    case MQ_COMMAND_CANCEL_SUBSCRIBE:
    case MQ_COMMAND_SUBSCRIBE:
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv msg->command = SUBSCRIBE / CANCEL_SUBCRIBE",conne->getName().c_str());
      /// 订阅
      handleSubscribe(conne,msg);
      break;
    case MQ_COMMAND_COMMIT:
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv msg->command = COMMIT",conne->getName().c_str());
      handleCommit(msg);
      simpleSendReply(msg->messageId,MQ_COMMAND_COMMIT,conne);
      break;
    default:
      SKYLU_LOG_FMT_WARN(G_LOGGER,"recv a invaild command[%d].",msg->command);
      break;

    }
  }



}
void MqServer::simpleSendReply(uint64_t messageId,char command, const TcpConnection::ptr &conne) {
  Buffer buff;
  createCommandMqPacket(&buff,command,messageId);
  conne->send(&buff);
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"message[%d] has send ack to conne[%s]",messageId,conne->getName().c_str());
}
void MqServer::handleDeliver(const MqPacket *msg) {

  ///投递
  if(m_messageId_set.find(msg->messageId) == m_messageId_set.end()){
    std::string topic, message;
    getTopicAndMessage(msg,topic,message);
    Buffer tmp;
    MqPacket * commd = const_cast<MqPacket *>(msg);
    commd->command = MQ_COMMAND_PULL;
    serializationToBuffer(msg,topic,message,tmp);
    if(m_partition.find(topic) == m_partition.end()){
      m_partition[topic].reset(new Partition(topic,0));
    }
    m_partition[topic]->addToLog(&tmp);
    SKYLU_LOG_FMT_INFO(G_LOGGER,"recv msg[%d] topic[%s] message[%s]",msg->messageId,topic.c_str(),message.c_str());

  }else{
    SKYLU_LOG_FMT_WARN(G_LOGGER,"recv a repeat msg[%d]",msg->messageId);
  }
  /// 如果messageId已经存在在set中就不做处理。
}
void MqServer::handleSubscribe(const TcpConnection::ptr& conne,const MqPacket *msg) {

  std::string topic;
  getTopic(msg,topic);
  Buffer buff;
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv subscribe topic[%s]",conne->getName().c_str(),topic.c_str());
  if(msg->command == MQ_COMMAND_CANCEL_SUBSCRIBE){
    m_consumer[conne].erase(topic);

  }else{
    if(m_partition.find(topic) == m_partition.end()){
      auto* info = const_cast<MqPacket * >(msg);
      info->retCode = ErrCode::UNKNOW_TOPIC;
    }else{
      m_consumer[conne].insert(topic);
    }
  }
  serializationToBuffer(msg,topic,buff);
  conne->send(&buff);


}

void MqServer::handlePull(const TcpConnection::ptr& conne,const MqPacket *msg) {
  Buffer buff;

  if(m_consumer.find(conne) == m_consumer.end()){
    /// 没有订阅
    SKYLU_LOG_FMT_WARN(G_LOGGER,"recv a unknown consumer. conne[%s]",conne->getName().c_str());
    auto * info = const_cast<MqPacket *>(msg);
    info->topicBytes = 0;
    info->retCode = ErrCode::UNKNOW_CONSUMER;
    serializationToBuffer(info,buff);
    conne->send(&buff);
    return ;
  }

  if(msg->topicBytes == 0) {

    for (const auto &topic : m_consumer[conne]) {
      if (msg->offset == 0) {
        auto * info = const_cast<MqPacket *>(msg);
        info->offset =
            m_commit_partition->getOffset(info->consumerGroupId, topic);
        assert(m_partition.find(topic) != m_partition.end());
        m_partition[topic]->sendToConsumer(info, conne);

      } else {

        m_partition[topic]->sendToConsumer(msg, conne);
      }
    }
  }else{
    /// 处理单个请求主题具体offset的情况
    std::string topic;
    getTopic(msg,topic);
    if(msg->offset < 0){
      auto * info = const_cast<MqPacket *>(msg);
      info->offset = m_commit_partition->getOffset(info->consumerGroupId,topic);
    }
    m_partition[topic]->sendToConsumer(msg,conne);

  }

}

void MqServer::handleCommit(const MqPacket *msg) {
  std::string topic;
  getTopic(msg,topic);
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"commit topic[%s] offset = %d",topic.c_str(),msg->offset);
  m_commit_partition->commit(msg->consumerGroupId,topic,msg->offset);

}
void MqServer::persistentPartitionWithMs() {
  for(auto &it  : m_partition){
    it.second->syncTodisk();

  }

}

void MqServer::persistentCommitWithMs() {
  m_commit_partition->persistentDisk();
}
void MqServer::removeInvaildConnection(const TcpConnection::ptr &conne) {
  if(m_consumer.find(conne) != m_consumer.end()){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"m_consumer[conne : %s] delete .",conne->getName().c_str());
    m_consumer.erase(conne);
  }
}
