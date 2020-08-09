//
// Created by jimlu on 2020/7/25.
//
#include "mq_proto.h"
const MqPacket * serializationToMqPacket(Buffer * buff){
  const auto * msg = reinterpret_cast<const MqPacket * >(buff->curRead());
  if(buff->readableBytes() < MqPacketLength(msg)){
    buff->resetAll();
    return nullptr;  ///可能一次性写入的量太大了导致需要分段
  }
  if(!checkMqPacketEnd(msg)){
    buff->updatePos(MqPacketLength(msg));
    return nullptr;
  }
  buff->updatePos(MqPacketLength(msg));
  return msg;

}
void serializationToBuffer(std::vector<MqPacket> & msg,Buffer &buff){

  for(auto & i : msg){

    int length = MqPacketLength(&i);
    char tmp[length];
    auto * ptr = reinterpret_cast<MqPacket * >(tmp);
    memcpy(tmp, reinterpret_cast<const char *>(&i),sizeof(MqPacket));
    setMqPacketEnd(ptr);
    buff.append(tmp,length);
  }

}
void serializationToBuffer(const MqPacket* msg,Buffer& buff){

  int length = MqPacketLength(msg);
  char tmp[length];
  auto * ptr = reinterpret_cast<MqPacket * >(tmp);
  memcpy(tmp, reinterpret_cast<const char *>(msg),sizeof(MqPacket));
  setMqPacketEnd(ptr);
  buff.append(tmp,length);
}

void serializationToBuffer(std::vector<MqPacket> & msg,const std::unordered_set<std::string> &topic,Buffer &buff){
  assert(msg.size()==topic.size());
  int i = 0;
  for(const auto &it : topic){

    assert(msg[i].topicBytes == it.size());
    int length = MqPacketLength(&msg[i]);
    char tmp[length];
    auto * ptr = reinterpret_cast<MqPacket * >(tmp);
    char * body = &(ptr->body);
    memcpy(tmp, reinterpret_cast<const char *>(&msg[i]),sizeof(MqPacket));
    memcpy(body,it.c_str(),it.size());

    setMqPacketEnd(ptr);

    buff.append(tmp,length);
    ++i;

  }

}
void serializationToBuffer(const MqPacket* msg,const std::string &topic ,Buffer& buff){

  ///TODO 可以优化一下
  assert(msg->topicBytes == topic.size());
  int length = MqPacketLength(msg);
  char tmp[length];
  auto * ptr = reinterpret_cast<MqPacket * >(tmp);
  char * body = &(ptr->body);
  memcpy(tmp, reinterpret_cast<const char *>(msg),sizeof(MqPacket));
  memcpy(body,topic.c_str(),topic.size());
  setMqPacketEnd(ptr);


  buff.append(tmp,length);
}
void serializationToBuffer(const MqPacket* msg,const std::string &topic ,const std::string &message,Buffer& buff){
  ///TODO 可以优化一下
  assert(msg->topicBytes == topic.size());
  assert(msg->msgBytes == message.size());
  int length = MqPacketLength(msg);
  char tmp[length];
  auto * ptr = reinterpret_cast<MqPacket * >(tmp);
  char * body = &(ptr->body);
  memcpy(tmp, reinterpret_cast<const char *>(msg),sizeof(MqPacket));
  memcpy(body,topic.c_str(),topic.size());
  memcpy(body + msg->topicBytes, message.c_str(),message.size());
  setMqPacketEnd(ptr);


  buff.append(tmp,length);

}
void getTopicAndMessage(const MqPacket * msg,std::string &topic,std::string& message ){

  assert(msg->command !=  MQ_COMMAND_ACK );
  assert(msg->command != MQ_COMMAND_HEARTBEAT);
  const auto * ptr = reinterpret_cast<const char * >(&(msg->body));

  topic.assign(ptr, msg->topicBytes);
  message.assign(ptr + msg->topicBytes , msg->msgBytes);

}
void getTopic(const MqPacket * msg,std::string & topic){
  assert(msg->command !=  MQ_COMMAND_ACK );
  assert(msg->command != MQ_COMMAND_HEARTBEAT);
  const auto * ptr = reinterpret_cast<const char * >(&(msg->body));
  topic.assign(ptr, msg->topicBytes);
}

void getMessage(const MqPacket * msg,std::string &message){

  assert(msg->command !=  MQ_COMMAND_ACK );
  assert(msg->command != MQ_COMMAND_HEARTBEAT);
  const auto * ptr = reinterpret_cast<const char * >(&(msg->body));
  message.assign(ptr, msg->topicBytes);

}
uint64_t getMessageIdFromBuffer(Buffer *buff){
  const auto * msg = reinterpret_cast<const MqPacket * >(buff->curRead());
  return msg->messageId;
}

uint64_t  createMessageId(){


 return  SnowflakeIdWorker::IdWorker::GetInstance()->getId();

}

void createCommandMqPacket(Buffer * buff,char command,uint64_t messageId){
  MqPacket msg{};
  msg.command = command;
  msg.messageId = messageId;
  msg.topicBytes = 0;
  msg.msgBytes = 0;
  buff->resetAll();
  serializationToBuffer(&msg,*buff);

}


