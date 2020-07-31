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
    m_dirpeer_clients[i]->setConnectionCallback(std::bind(&MqServer::sendRegister,this,std::placeholders::_1));
    m_dirpeer_clients[i]->setMessageCallback(std::bind(&MqServer::onMessageFromDirPeer
                                                       ,this,std::placeholders::_1,std::placeholders::_2));


  }

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
void MqServer::sendRegister(const TcpConnection::ptr &conne) {
  char buff[64] ={0};
  DirServerPacket* msg = reinterpret_cast<DirServerPacket *>(buff);
  msg->Msgbytes = m_local_addr->toString().size();
  msg->command = DirProto::COMMAND_REGISTER;
  memcpy(&msg->hostAndPort,m_local_addr->toString().c_str(),msg->Msgbytes);
  conne->send(buff,sizeof(DirServerPacket) + msg->Msgbytes);
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send RegisterRequest to conne[%s]",conne->getName().c_str());

}
void MqServer::sendKeepAliveWithMs() {
  DirServerPacket msg{};
  msg.Msgbytes = 0;
  msg.command = DirProto::COMMAND_KEEPALIVE;
  for(const auto& it : m_dirpeer_clients){
    const auto & conne = it->getConnection();
    if(conne){

      conne->send(reinterpret_cast<char * >(&msg),sizeof(DirServerPacket));
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send Ack to conne[%s]",it->getName().c_str());

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
     SKYLU_LOG_FMT_DEBUG(G_LOGGER, "send heartBeat to peers[num = %d] ",
                         m_dirpeer_clients.size());
     m_loop->runEvery(kSendKeepAliveMs * Timestamp::kSecondToMilliSeconds,
                      std::bind(&MqServer::sendKeepAliveWithMs, this));
     isVaild = true;
   }

   }else{
   SKYLU_LOG_FMT_ERROR(G_LOGGER,"INVALILD COMMAND = %d from conne[%s]"
                       ,msg->command,conne->getName().c_str());
 }



}
void MqServer::onMessageFromMqBusd(const TcpConnection::ptr &conne,
                                   Buffer *buff) {
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"msg{%s} from conne[%s]",buff->curRead(),conne->getName().c_str());
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
      handlePull(msg);
      break;
    case MQ_COMMAND_SUBSCRIBE:
      /// 订阅
      handleCancelSubscribe(msg);
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
    serializationToBuffer(msg,topic,message,tmp);
    m_topicAndMsg[topic].push_front(tmp);
    m_messageId_set.insert({msg->messageId,m_topicAndMsg[topic].begin()});
    SKYLU_LOG_FMT_INFO(G_LOGGER,"recv msg[%ld] topic[%s] message[%s]",msg->messageId,topic.c_str(),message.c_str());
    /// 这里可以落地一下

  }
  /// 如果messageId已经存在在set中就不做处理。
}
void MqServer::handleSubscribe(const MqPacket *msg) {

  std::string topic;
  getTopic(msg,topic);


}
void MqServer::handleHeartBeat(const MqPacket *msg) {

}
void MqServer::handlePull(const MqPacket *msg) {

}
void MqServer::handleCancelSubscribe(const MqPacket *msg) {

}
void MqServer::handleCommit(const MqPacket *msg) {

}

