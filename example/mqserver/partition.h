//
// Created by jimlu on 2020/7/29.
//

#ifndef MQSERVER_PARTITION_H
#define MQSERVER_PARTITION_H
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <memory>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>
#include <skylu/net/buffer.h>
#include <skylu/base/nocopyable.h>
#include <skylu/base/file.h>
#include <skylu/base/fdmanager.h>
#include <skylu/base/btree.h>
#include <skylu/net/tcpconnection.h>
#include <skylu/proto/mq_proto.h>

using namespace skylu;
class Partition :Nocopyable{
  static const int ksingleFileMaxSize = 1024*1024*1024; /// 1G
  static const int kMsgblockMaxSize =  4096;  ///消息达到多大的时候稀疏索引 +  1
  static const int  kIndexMinInteral = 5;
  static const char * kIndexFileName ; ////保存当前分区最后提交的sequence

public:

  Partition(const std::string &topic,int id);
  ~Partition();
  /**
   * 只有在broker 中才会被调用
   * @param msg
   */
  void addToLog(Buffer  *msg);
  void sendToConsumer(const MqPacket * info,const TcpConnection::ptr &conne);

  std::string getTopic (){return m_topic;}

private:
  void loadIndex();
  ///找到当前分区的最后一个包序号
  void findLastSequenceMsg();

  void writeCommitToFile() const;
  void initCommitSequence();



private:
  const std::string m_prefix ;
  typedef std::map<long,long> OffsetAndPosition;
  File::ptr m_lastIndexFile;  /// 用于追加到最后的index文件
  File::ptr m_lastLogFile;

  std::map<long,OffsetAndPosition> m_indexs; /// file_sequence : offset,positon

  long m_lastOffsetIndexInFile; // 最后一个保存在文件里面的offset值
  long m_sequence;  /// 最后一个消息的序号 (消息总数
  long m_commitSequence; /// 最后提交的offset

  std::string m_topic;
  int m_id;

};

#endif // MQSERVER_PARTITION_H
