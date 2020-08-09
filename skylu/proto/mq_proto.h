//
// Created by jimlu on 2020/7/23.
//

#ifndef MQSERVER_MQ_PROTO_H
#define MQSERVER_MQ_PROTO_H
#include "../base/snowflake.h"
#include "../net/buffer.h"
#include <unordered_set>

using namespace skylu;
typedef std::pair<std::string,std::string> TopicAndMessage;

const static int  kSendKeepAliveToMqMs = 500;
const static int kCheckKeepAliveOnMqMs = 2000;
enum MqProto{
  MQ_COMMAND_DELIVERY,  /// 投递
  MQ_COMMAND_SUBSCRIBE,  /// 订阅
  MQ_COMMAND_CANCEL_SUBSCRIBE,
  MQ_COMMAND_COMMIT, ///提交
  MQ_COMMAND_PULL,
  MQ_COMMAND_ACK,
  MQ_COMMAND_HEARTBEAT

};
enum ErrCode{
  SUCCESSFUL = 0,
  UNKNOW_CONSUMER,  /// 身份未注册 or 在服务端的身份过期
  UNKNOW_TOPIC


};

struct MqPacket{
public:
  MqPacket() = default;
  char   command = MQ_COMMAND_ACK;
  char    retCode = SUCCESSFUL;
  uint64_t messageId = 0;  // produceMqbusd 产生，全局唯一ID
  int64_t  msgCreateTime = 0;
  uint64_t  sendTimerWithMs = 0; // 定时发送  0: 马上发送
  int32_t  consumerGroupId = -1;  /// -1: 不进入任何组
  uint32_t  pullInteval = 0; // 拉取间隔
  int64_t  maxEnableBytes = 0; // consumer允许最多的消息
  int64_t  minEnableBytes = 0; /// consumer最少需要多少消息
  uint64_t offset = 0; // 唯一确定每条消息在分区里面的位置
  uint32_t topicBytes = 0;  //主题长度
  uint32_t msgBytes =0; //消息长度
  char body{};  //消息用/r/n多个消息分割


};

typedef uint32_t MqPacketEnd;

const MqPacketEnd mqPacketEnd  =  {0xFFFFFFFF};

void  getTopicAndMessage(const MqPacket * msg,std::string &topic,std::string& message );
const MqPacket * serializationToMqPacket(Buffer * buff);
void getTopic(const MqPacket *msg,std::string & topic);
void getMessage(const MqPacket *msg,std::string & message);
uint64_t getMessageIdFromBuffer(Buffer *buff);
void serializationToBuffer(const MqPacket* msg,const std::string &topic ,const std::string &message,Buffer& buff);
void serializationToBuffer(const MqPacket* msg,const std::string &topic ,Buffer& buff);
void serializationToBuffer(const MqPacket* msg,Buffer& buff);
void serializationToBuffer(std::vector<MqPacket> & msg,const std::unordered_set<std::string> &topic,Buffer &buff);
void serializationToBuffer(std::vector<MqPacket> & msg,Buffer &buff);

inline  size_t MqPacketLength(const MqPacket * msg){return sizeof(MqPacket)
                                              +msg->topicBytes + msg->msgBytes
                                              +sizeof(MqPacketEnd);}
inline uint32_t getMqPacketEnd(const MqPacket *msg){
  return static_cast<uint32_t>(*( &(msg->body)
                                 + msg->topicBytes + msg->msgBytes));}
inline void setMqPacketEnd(MqPacket *msg){
  auto * ptr = reinterpret_cast<char *>(&(msg->body));
  memcpy(ptr+msg->topicBytes+msg->msgBytes,&mqPacketEnd,sizeof(MqPacketEnd));
}
inline bool checkMqPacketEnd(const MqPacket *msg){return getMqPacketEnd(msg) == mqPacketEnd;}

uint64_t createMessageId();

void createCommandMqPacket(Buffer * buff,char command,uint64_t messageId);


#endif // MQSERVER_MQ_PROTO_H
