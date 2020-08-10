//
// Created by jimlu on 2020/7/29.
//

#include "partition.h"
Partition::Partition(const std::string &topic, int id,
                     uint64_t singleFileMaxSize, int msgblockMaxSize,
                     int indexMinInterval)
:m_prefix(topic +"-"+ std::to_string(id) + "/"),m_lastLogMmap(nullptr),m_lastLogFileLength(0),m_lastOffsetIndexInFile(0),m_size(0)
,m_topic(topic),m_id(id),m_isDirty(false),ksingleFileMaxSize(singleFileMaxSize)
,kMsgblockMaxSize(msgblockMaxSize),kIndexMinInterval(indexMinInterval)
{
  if(!File::isExits(topic+ "-" + std::to_string(id))){
    /// 文件夹不存在 创建,同时创建index 索引文件
    File::createDir(topic+ "-" + std::to_string(id));

  }else{
    loadIndex();
    findLastSequenceMsg();
  }

  if(!m_lastLogFile){
    ///着两个是一起的，一个没有另一个也一定没有
    assert(!m_lastIndexFile);
    mmapNewLog();
  }


}

Partition::~Partition() {
  syncTodisk();
  unmmapLog();
}
void Partition::addToLog(Buffer *msg) {



  assert(m_lastLogFile);
  assert(m_lastIndexFile);
  if(ksingleFileMaxSize - m_lastLogFileLength < msg->readableBytes() ){
    /// 需要写入的消息过大
    mmapNewLog();
  }

  std::string writIndex=""; ///要写入index文件的index内容
  uint64_t position = m_lastLogFileLength;
  char  * src = const_cast<char *>(msg->curRead());
  for(size_t i = position; i - position < msg->readableBytes();){
    auto * m = reinterpret_cast<MqPacket* >(src);
    int len = MqPacketLength(m);
    m->command = MQ_COMMAND_PULL;
    m->offset = m_size;  ///序号
    src += len;
    i +=len;
    m_size ++;
    uint64_t in_file_offset = m_size - m_indexs.rbegin()->first;
    if(len > kMsgblockMaxSize || in_file_offset > m_lastOffsetIndexInFile + kIndexMinInterval
        || in_file_offset == 0){
      m_lastOffsetIndexInFile = in_file_offset;
      m_indexs.rbegin()->second.insert({in_file_offset, position});
      writIndex += std::to_string(m_lastOffsetIndexInFile) + "," + std::to_string(i) + "\n";
    }
  }
  memcpy(m_lastLogMmap + m_lastLogFileLength,msg->curRead(),msg->readableBytes());
  m_lastLogFileLength += msg->readableBytes();
  msg->resetAll();

  if(!writIndex.empty()) {
    m_index_buffer.append(writIndex);

  }
  assert(m_size >= m_lastOffsetIndexInFile);
  m_isDirty = true;

}
void Partition::loadIndex() {


  std::vector<std::string> vec_index;
  std::vector<std::string> vec_log;
  FSUtil::ListAllFile(vec_index,m_prefix,".index");

  if(vec_index.empty()){
    return ;
  }
  std::sort(vec_index.begin(),vec_index.end(),[](const std::string s1,const std::string  & s2){
    int prefix = s1.find('/')+2;
    int n1 = std::stol(s1.substr(prefix)),n2 = std::stol(s2.substr(prefix));
    return n1 < n2;
  });
  m_lastIndexFile = Fdmanager::FdMagr::GetInstance()->open(vec_index.back(),O_RDWR|O_APPEND);

  FSUtil::ListAllFile(vec_log,m_prefix,".log");
  assert(!vec_log.empty());

  std::sort(vec_log.begin(),vec_log.end(),[](const std::string s1,const std::string  & s2){
    int prefix = s1.find('/')+2; /// eg: hello0//0.index
    int n1 = std::stol(s1.substr(prefix)),n2 = std::stol(s2.substr(prefix));
    return n1 < n2;
  });
  m_lastLogFile = Fdmanager::FdMagr::GetInstance()->open(vec_log.back(),O_RDWR|O_APPEND);

  for(const auto & it : vec_index){
    std::vector<std::string> allLine;
    OffsetAndPosition  content;
    File::ptr file = Fdmanager::FdMagr::GetInstance()->open(it);
    file->readAllLine(allLine);
    for(const auto & line : allLine){
      int index= line.find(',');
      content[std::stol(line)] = std::stol(line.substr(index + 1));
    }
    m_indexs[std::stol(it.substr(it.find('/')+2))] = content;
    Fdmanager::FdMagr::GetInstance()->del(file->getFd());
  }
  m_lastOffsetIndexInFile = getBackIndexInLastFile(); /// 最后一个.index 的最后一行
  m_lastLogMmap = static_cast<char *>(mmap(nullptr,ksingleFileMaxSize,PROT_READ|PROT_WRITE,MAP_SHARED,m_lastLogFile->getFd(),0));

}

uint64_t Partition::sendToConsumer(const MqPacket *info,
                               const TcpConnection::ptr &conne) {

  assert(info->command  == MQ_COMMAND_PULL);
  uint64_t offset = info->offset;
  if(m_size <= info->offset){
    ///拉取的offset 太超前了
    SKYLU_LOG_FMT_WARN(G_LOGGER,"conne[%s] pull offset = %d > partition's[%s] size[%d]",
                       conne->getName().c_str(),info->offset,m_topic.c_str(),m_size);
    Buffer buff;
    auto * msg = const_cast<MqPacket *>(info);
    msg->retCode = ErrCode::OFFSET_TOO_LARGET;
    msg->topicBytes = m_topic.size();
    serializationToBuffer(msg,m_topic,buff);
    conne->send(&buff);
    return 0;
  }
  auto it = m_indexs.lower_bound(offset) ; /// 起点文件
  if(it->first != offset){
    --it;
  }
  std::string filename = m_prefix+std::to_string(it->first) + ".log";

  int fd = -1;
  char  * buff = nullptr ;
  if(it->first == m_indexs.rbegin()->first){
    ///定位到了最后一个文件
    fd = m_lastLogFile->getFd();
    buff = m_lastLogMmap;
  }else{
    fd = ::open(filename.c_str(),O_RDONLY);
    buff = static_cast<char * > (mmap(nullptr,ksingleFileMaxSize,PROT_READ, MAP_PRIVATE, fd,0));
    if(buff == (void *)-1){
      SKYLU_LOG_FMT_ERROR(G_LOGGER,"mmap errno = %d,strerrno = %s",errno,strerror(errno));
      return 0;
    }
  }
  uint64_t indexInFile = offset - it->first; /// 需要查找的索引在文件中的索引量
  uint64_t  index = -1;
  long position = 0; ///记录在索引文件中的值
  auto line  = it->second.lower_bound(indexInFile); ///定位到的索引位置  offset-position
  if(line != it->second.end()) {
    if (line->first != indexInFile) {
      if (line != it->second.begin())
        --line;
    }

    if (line->first <= indexInFile) {
      index = line->first;
      position = line->second;
    } else {
      ///从头开始找
      index = 0;
      position = 0;
    }
  }else{
    ///可能会出现broker宕机的时候index没有写入导致.index文件里面是空的 之后就需要重新查找
    index = 0;
    position = 0;
  }

  assert(position>=0);
  char * content  = buff + position; /// 第offset个消息的起点

    for (uint64_t i = index; i < indexInFile; ++i) {
      auto *msg = reinterpret_cast<MqPacket *>(content);
      int length = MqPacketLength(msg);
      content += length;
      position += length;
      ///向下遍历找到需要发送的消息的消息
    }
  int eofPosition = position;

  auto * tmp = content;
  int count = 0 ; ///发送了多少个包
  ///  判断当前位置到下一个索引的位置是不是可以直接发送
  uint64_t  maxEnableBytes = info->maxEnableBytes >=0 ? info->maxEnableBytes : ksingleFileMaxSize;
  uint64_t  lastOffset = 0;
  for (uint32_t i = 0; i <= maxEnableBytes;) {

      auto *msg = reinterpret_cast<MqPacket *>(tmp);
      if(msg->command != MQ_COMMAND_PULL
          ||((MqPacketLength(msg)<=maxEnableBytes - i)&&!checkMqPacketEnd(msg))){
        ///非法数据 。如果直接用包尾判断的话会读到非法内存
        break;
      }
      /// 判断每次是不是都是自增1
      if(lastOffset == 0){
       lastOffset =  msg->offset;
      }else{
        assert(lastOffset+1 == msg->offset);
        lastOffset = msg->offset;
      }
      count++;
      int length = MqPacketLength(msg);
      tmp += length;
      i += length;
      if(i >= ksingleFileMaxSize-position){
        ///超出了文件大小
        break;
      }
    //SKYLU_LOG_FMT_DEBUG(G_LOGGER,"need offset = %d,send topic[%s] msg_offset[%d] to conne[%s]"
     //                     ,offset,m_topic.c_str(),msg->offset,conne->getName().c_str());
      eofPosition =
          i <= maxEnableBytes ? eofPosition + length : eofPosition;
  }
  int socket = conne->getSocketFd();
  assert(eofPosition > position);
  SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send msg from %s len = %d,position = %d, eofPosition = %d",filename.c_str(),eofPosition-position,position,eofPosition);
  int res = sendfile(socket,fd,static_cast<off_t*>(&position),eofPosition - position);
  if(res < 0){
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"sendfile ret = %d,errno = %d,strerrno = %s",res,errno,strerror(errno));
  }

  if(buff != m_lastLogMmap){
     munmap(buff,ksingleFileMaxSize);
   }
   if(m_lastLogFile->getFd() != fd){
     close(fd);
   }
  return count;


}

void Partition::findLastSequenceMsg() {
  if(!m_lastLogFile){
    return ;
  }
  int fd = ::open(m_lastLogFile->getPath().c_str(),O_RDONLY);
  char  * buff = static_cast<char * > (mmap(nullptr,ksingleFileMaxSize,PROT_READ, MAP_SHARED, fd,0));
  char * msg = static_cast<char * >(buff);
   int sequence = 0;
  while( checkMqPacketEnd(reinterpret_cast<MqPacket*>(msg))
          &&static_cast<uint64_t>(msg -buff)<ksingleFileMaxSize){
    int length = MqPacketLength(reinterpret_cast<MqPacket *>(msg));
    msg += length;
    m_lastLogFileLength += length;
     sequence++;
   }
  m_size = sequence + m_indexs.rbegin()->first;
  close(fd);

}
void Partition::syncTodisk() {
  if(m_isDirty) {

    assert(m_lastLogMmap&&m_lastLogMmap != (void *)-1);
    msync(m_lastLogMmap,ksingleFileMaxSize,MS_SYNC); ///同步落地到磁盘
    m_lastIndexFile->write(m_index_buffer.curRead(),
                           m_index_buffer.readableBytes());
    m_index_buffer.resetAll();
    m_isDirty = false;
  }

}
void Partition::mmapNewLog() {

  syncTodisk();
  unmmapLog();
  if(m_lastIndexFile){
    Fdmanager::FdMagr::GetInstance()->del(m_lastIndexFile->getFd());
  }
  m_lastIndexFile = Fdmanager::FdMagr::GetInstance()->open(m_prefix +std::to_string(m_size)+ ".index",O_CREAT|O_RDWR|O_APPEND);
  OffsetAndPosition  m;
  m_indexs.insert({m_size,m});
  if(m_lastLogFile){

    Fdmanager::FdMagr::GetInstance()->del(m_lastLogFile->getFd());
  }
  m_lastLogFile = Fdmanager::FdMagr::GetInstance()->open(m_prefix+std::to_string(m_size) + ".log",O_CREAT|O_RDWR|O_APPEND);
  ftruncate(m_lastLogFile->getFd(),ksingleFileMaxSize);
  m_lastLogMmap = static_cast<char *>(mmap(nullptr,ksingleFileMaxSize,PROT_READ|PROT_WRITE,MAP_SHARED,m_lastLogFile->getFd(),0));
  m_lastLogFileLength = 0;
  m_lastOffsetIndexInFile = 0;
  assert(m_lastLogMmap != (void *)-1);
}

void Partition::unmmapLog() {
  if(m_lastLogMmap == (void *)-1 || !m_lastLogMmap){
    return ;
  }
  munmap(m_lastLogMmap,ksingleFileMaxSize);
}
