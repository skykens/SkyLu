//
// Created by jimlu on 2020/8/6.
//

#include "commitpartition.h"
const char * CommitPartition::kLoggerName = "commit_logger";
CommitPartition::CommitPartition(const std::string& name,const std::string & logname)
    :m_cond(m_mutex),kCommitPartitionFilename(name),kCommitPartitionbakFilename(name + ".bak"),kCommitPartitionLogName(logname)
      ,m_thread(std::bind(&CommitPartition::persistentToDiskInThread,this),"CommitPartitionThread",true)
      ,isQuit(false),isDirty(false)
      {
  init();
  SKYLU_LOG_NAME(kLoggerName)->clearAppenders();
  if(!kCommitPartitionLogName.empty()){
    SKYLU_LOG_NAME(kLoggerName)->addAppender(LogAppender::ptr(new FileLogAppender(kCommitPartitionLogName)));
  }

}
CommitPartition::~CommitPartition() {
  isQuit = true;
  isDirty = true;
  m_cond.notify();
  m_thread.join();

}

void CommitPartition::persistentToDiskInThread() {

  while(!isQuit) {

    {
      Mutex::Lock lock(m_mutex);
      while(!isDirty){
        m_cond.wait();
      }
      isDirty = false;
    }
    if(isQuit){
      break;
    }
    SKYLU_LOG_DEBUG(SKYLU_LOG_NAME("commit logger"))<<"start persistent commit to disk . ";

    if(File::isExits(kCommitPartitionbakFilename)){
      File::remove(kCommitPartitionbakFilename);
    }
    File::ptr file = Fdmanager::FdMagr::GetInstance()->open(
        kCommitPartitionbakFilename, O_CREAT | O_RDWR);
    std::unordered_map<uint64_t, CommitInfo> replicate;
    {
      Mutex::Lock lock(m_mutex);
      replicate = m_infos;
    }
    Buffer buffer;
    for (const auto &it : replicate) {
      for (const auto &item : it.second.info) {
        std::ostringstream stream;
        stream << it.first << ':' << item.first << ':' << item.second
               << '\n'; //// id:topic:offset
        buffer.append(stream.str());
        SKYLU_LOG_DEBUG(SKYLU_LOG_NAME("commit logger"))<<" commit info : ["<< stream.str()<<"]";
      }
    }
    file->write(buffer.curRead(), buffer.readableBytes());
    Fdmanager::FdMagr::GetInstance()->del(file->getFd());
    File::rename(kCommitPartitionbakFilename,kCommitPartitionFilename);
  }

  SKYLU_LOG_WARN(SKYLU_LOG_NAME("commit logger"))<<" commit Thread exit .";

}
uint64_t CommitPartition::getOffset(int id,const std::string &topic) {
  if(m_infos.find(id) == m_infos.end()){
    ///先找有没有 id
    return  0;
  }
  return m_infos[id].findOffset(topic);
}
void CommitPartition::init() {
  if(File::isExits(kCommitPartitionFilename)){
    File::ptr file = Fdmanager::FdMagr::GetInstance()->open(kCommitPartitionFilename,O_RDONLY);
    std::vector<std::string> all;
    file->readAllLine(all);
    for(size_t i = 0;i<all.size();++i){
      std::string out;
      std::istringstream is(all[i]);
      uint64_t id = 0,offset=0;
      int status = 0;
      std::string topic;
      while(std::getline(is,out,':')){
        if(status == 0 ){
          id = std::stol(out);
          ++status;
        }else if(status == 1){
          topic = std::move(out);
          ++status;
        }else if(status == 2){
          offset = std::stol(out);
          ++status;
        }else{
          assert(status > 2);
        }
      }
      if(m_infos.find(id) == m_infos.end()){
        m_infos.insert({id,CommitInfo()});
      }
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"init commitOffset[%d:%s:%d]",id,topic.c_str(),offset);
      m_infos[id].commitOffset(topic,offset);
    }
  }

}
void CommitPartition::commit(uint64_t id, const std::string &topic, uint64_t offset) {

  Mutex::Lock  lock(m_mutex);
  if(m_infos.find(id) == m_infos.end()){
    m_infos.insert({id,CommitInfo()});
  }
  m_infos[id].commitOffset(topic,offset);
  isDirty = true;

}
