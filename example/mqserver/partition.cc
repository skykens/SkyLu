//
// Created by jimlu on 2020/7/29.
//

#include "partition.h"
const char * Partition::kIndexFileName = ".commitIndex";
Partition::Partition(const std::string &topic, int id)
    :m_prefix(topic + std::to_string(id) + "/"),m_lastOffsetIndexInFile(0),m_sequence(0),m_commitSequence(-1)
      ,m_topic(topic),m_id(id)
{
  if(!File::isExits(topic + std::to_string(id))){
    /// 文件夹不存在 创建,同时创建index 索引文件
    File::createDir(topic + std::to_string(id));
    std::string s(m_prefix + kIndexFileName);
  }else{
    loadIndex();
    findLastSequenceMsg();
    initCommitSequence();
  }

}

Partition::~Partition() {
}
void Partition::addToLog(Buffer *msg) {

  if(m_lastLogFile&&m_lastLogFile->getFilesize() > ksingleFileMaxSize){
    m_lastLogFile.reset();
    m_lastIndexFile.reset();
  }
  if(!m_lastIndexFile || !m_lastLogFile){
    assert(!m_lastIndexFile);
    assert(!m_lastLogFile);


    std::map<long,long> m;
    m_lastIndexFile = Fdmanager::FdMagr::GetInstance()->open(m_prefix +std::to_string(m_sequence)+ ".index",O_CREAT|O_RDWR|O_APPEND);
    m_indexs.insert({m_sequence,m});
    m_lastLogFile = Fdmanager::FdMagr::GetInstance()->open(m_prefix+std::to_string(m_sequence) + ".log",O_CREAT|O_RDWR|O_APPEND);

  }
  int position = m_lastLogFile->getFilesize();
  ssize_t size =  m_lastLogFile->write(msg->curRead(),msg->readableBytes());
  assert(size > 0);
  int in_file_offset = m_sequence - m_indexs.rbegin()->first;
  if (size > kMsgblockMaxSize ||
        in_file_offset > m_lastOffsetIndexInFile +
                             kIndexMinInteral /// 在文件中的offset > 最小的间隔
        || in_file_offset == 0) {
      /// 写入索引文件
      m_lastIndexFile->writeNewLine(std::to_string(m_sequence) + "," +
                                    std::to_string(position));
      m_lastOffsetIndexInFile =
          in_file_offset; /// 至少kIndexMinInteral间隔触发一次写索引
      m_indexs.rbegin()->second.insert({in_file_offset, position});
  }
  ++m_sequence;



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
    return std::stol(s1.substr(prefix)) > std::stol(s2.substr(prefix));
  });
  m_lastIndexFile = Fdmanager::FdMagr::GetInstance()->open(vec_index.back(),O_RDWR|O_APPEND);

  FSUtil::ListAllFile(vec_log,m_prefix,".log");
  assert(!vec_log.empty());

  std::sort(vec_log.begin(),vec_log.end(),[](const std::string s1,const std::string  & s2){
    int prefix = s1.find('/')+2;
    return std::stol(s1.substr(prefix)) > std::stol(s2.substr(prefix));
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
  }


}
void Partition::sendToConsumer(const MqPacket *info,
                               const TcpConnection::ptr &conne) {

}
void Partition::writeCommitToFile() const {
  std::ofstream file(kIndexFileName,std::ios::trunc);
  file<<m_commitSequence;
}
void Partition::initCommitSequence() {
  std::ifstream  file(m_prefix + kIndexFileName);
  file>>m_commitSequence;
}
void Partition::findLastSequenceMsg() {
  size_t sz = m_lastLogFile->getFilesize();
  if(sz == 0){
    return ;
  }
  int fd = ::open(m_lastLogFile->getPath().c_str(),O_RDONLY);

  void * buff = mmap(nullptr,sz,PROT_READ, MAP_SHARED, fd,0);
   MqPacket * msg = static_cast<MqPacket * >(buff);
   int sequence = 0;

  std::cout<<(char *)buff - reinterpret_cast<char *>(msg);
  while((char *)buff - reinterpret_cast<char *>(msg) >= 0){
    std::cout<<(char *)buff - reinterpret_cast<char *>(msg)<<std::endl;
    msg += MqPacketLength(msg);
     sequence++;
   }

   /*
  for(i = MqPacketLength(msg);i <= sz; i+=MqPacketLength(msg)){
    msg += MqPacketLength(msg);

  }
    */

  m_sequence = sequence + m_indexs.rbegin()->first;

}
