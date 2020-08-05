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

  static const uint64_t ksingleFileMaxSize = 1024 * 50; /// 64M
  static const int kMsgblockMaxSize =  4096;  ///单个消息超过多大的时候稀疏索引 +  1
  static const int  kIndexMinInteral = 5;

public:
  typedef std::unique_ptr<Partition> ptr;

  Partition(const std::string &topic,int id);
  ~Partition();
  void addToLog(Buffer  *msg);


  uint64_t sendToConsumer(const MqPacket * info,const TcpConnection::ptr &conne);

  size_t getSize()const  {return m_size;}

  std::string getTopic (){return m_topic;}
  /**
   * 将mmap映射内容同步到磁盘 ， 同时将索引缓冲区的内容落地
   */
  void syncTodisk();

private:
  void loadIndex();
  ///找到当前分区的最后一个包序号
  void findLastSequenceMsg();


  /**
   * 获得末尾的index文件的最后一行索引（index）
   * @return
   */
  long getBackIndexInLastFile(){
    const auto & index = m_indexs.rbegin()->second;
    if(index.empty()){
      return 0;
    }
    return index.rbegin()->first;
  }


  void mmapNewLog();
  void unmmapLog();

private:
  const std::string m_prefix ; /// eg: "partitionId/"
  typedef std::map<uint64_t ,uint64_t> OffsetAndPosition;
  File::ptr m_lastIndexFile;  /// 用于追加到最后的index文件
  char * m_lastLogMmap;  ////映射原地址
  File::ptr m_lastLogFile;
  uint64_t m_lastLogFileLength;
  Buffer m_index_buffer; ///索引缓冲区

  std::map<uint64_t ,OffsetAndPosition> m_indexs; /// file_sequence : offset,positon

  uint64_t m_lastOffsetIndexInFile; // 最后一个保存在文件里面的offset值
  uint64_t m_size;  /// 最后一个消息的序号 (消息总数

  std::string m_topic;
  int m_id;
  bool m_isDirty; ///脏页 需要sync

};

#endif // MQSERVER_PARTITION_H
