//
// Created by jimlu on 2020/8/6.
//

#ifndef MQSERVER_COMMITPARTITION_H
#define MQSERVER_COMMITPARTITION_H
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <atomic>
#include <skylu/base/file.h>
#include <skylu/base/fdmanager.h>
#include <skylu/base/nocopyable.h>
#include <skylu/base/singleton.h>
#include <skylu/base/thread.h>
#include <skylu/base/log.h>
#include <skylu/net/buffer.h>

using namespace skylu;
struct CommitInfo{
  CommitInfo() = default;
  uint64_t  findOffset(const std::string & topic){
    if(info.find(topic) == info.end()){
      return 0;
    }
    return info[topic];
  }
  void commitOffset(const std::string & topic,uint64_t offset){
    auto it =  info.find(topic);
    if(it == info.end() || it->second < offset){
      info[topic] = offset;
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"update topic[%s] offset[%d]",topic.c_str(),offset);
    }else{
      SKYLU_LOG_FMT_WARN(G_LOGGER,"update topic[%s] offset[%d] fail. offset = %d. ",topic.c_str(),it->second,offset);
    }
  }
  std::unordered_map<std::string, uint64_t> info;
};

class CommitPartition :Nocopyable{
public:
  typedef std::unique_ptr<CommitPartition> ptr;
  CommitPartition(const std::string & name);
  ~CommitPartition();
  void commit(uint64_t id,const std::string & topic,uint64_t offset);
  uint64_t getOffset(int id,const std::string &topic);
  void persistentDisk(){
    m_cond.notify();
  }

private:
  void persistentToDiskInThread();
  void init();

private:
  Mutex m_mutex;
  Condition m_cond;
  std::unordered_map<uint64_t,CommitInfo> m_infos; //// id:topic:offset
  const std::string kCommitPartitionFilename;
  const std::string kCommitPartitionbakFilename;
  Thread m_thread;
  bool isQuit;
  bool isDirty;



};

#endif // MQSERVER_COMMITPARTITION_H
