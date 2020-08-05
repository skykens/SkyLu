//
// Created by jimlu on 2020/7/22.
//

#include "mqserver.h"
#include <skylu/base/daemon.h>
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

  m_loop->runEvery(kPersistentTimeMs *Timestamp::kSecondToMilliSeconds,
                   std::bind(&MqServer::persistentPartitionWithMs,this));
  m_loop->runEvery(kSendKeepAliveMs * Timestamp::kSecondToMilliSeconds,
                   std::bind(&MqServer::sendKeepAliveWithMs, this));
  m_server.setMessageCallback(std::bind(&MqServer::onMessageFromMqBusd
                                      ,this,std::placeholders::_1,std::placeholders::_2));
}

void MqServer::init() {
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
    SKYLU_LOG_FMT_WARN(G_LOGGER,"conne[%s] send error packet size[%d]",conne->getName().c_str(),buff->readableBytes());
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
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv msg->command = %d",conne->getName().c_str(),msg->command);
    switch (msg->command) {
    case MQ_COMMAND_DELIVERY:
      /// 投递
      handleDeliver(msg);
      simpleSendReply(msg->messageId,MQ_COMMAND_DELIVERY,conne);
      break;
    case MQ_COMMAND_HEARTBEAT:
      ///消费者发过来的心跳
      handleHeartBeat(msg);
      break;
    case MQ_COMMAND_PULL:
      /// 拉取消息
      handlePull(conne,msg);
      break;
    case MQ_COMMAND_SUBSCRIBE:
      /// 订阅
      handleSubscribe(conne,msg);
      simpleSendReply(msg->messageId,MQ_COMMAND_SUBSCRIBE,conne);
      break;
    case MQ_COMMAND_CANCEL_SUBSCRIBE:
      handleCancelSubscribe(msg);
      simpleSendReply(msg->messageId,MQ_COMMAND_CANCEL_SUBSCRIBE,conne);
      break;
    case MQ_COMMAND_COMMIT:
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
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"message[%ud] has send ack to conne[%s]",messageId,conne->getName().c_str());
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
    SKYLU_LOG_FMT_INFO(G_LOGGER,"recv msg[%ld] topic[%s] message[%s]",msg->messageId,topic.c_str(),message.c_str());

  }
  /// 如果messageId已经存在在set中就不做处理。
}
void MqServer::handleSubscribe(const TcpConnection::ptr& conne,const MqPacket *msg) {

  std::string topic;
  getTopic(msg,topic);
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"conne[%s] recv subscribe topic[%s]",conne->getName().c_str(),topic.c_str());
  m_consumer[conne].push_back(topic);

}
void MqServer::handleHeartBeat(const MqPacket *msg) {

}
void MqServer::handlePull(const TcpConnection::ptr& conne,const MqPacket *msg) {
  Buffer buff;

  for(const auto  & topic : m_consumer[conne]) {
    if (msg->offset < 0) {
      MqPacket info = *msg;
      assert(MqPacketLength(&info) == MqPacketLength(msg));
      info.offset = m_consumer_commit[info.consumerGroupId][topic];
      m_partition[topic]->sendToConsumer(&info,conne);

    } else {

      m_partition[topic]->sendToConsumer(msg, conne);
    }
  }



}
void MqServer::handleCancelSubscribe(const MqPacket *msg) {

}
void MqServer::handleCommit(const MqPacket *msg) {

}
void MqServer::persistentPartitionWithMs() {
  for(auto &it  : m_partition){
    it.second->syncTodisk();

  }

}
