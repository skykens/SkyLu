//
// Created by jimlu on 2020/7/26.
//

#include "producer.h"
Producer::Producer(const std::vector<Address::ptr> & dir_addrs,const std::string &name)
        : MqBusd(name)
        ,m_thread()
        ,m_dir_addrs(dir_addrs)
        ,m_isInit(false)
        ,m_empty_queueAndsResend(m_mutex){


}

void Producer::sendInLoop(bool isRetry) {

  m_loop->assertInLoopThread();
  Buffer buff;

  ProduceQueue queue;
  {
    Mutex::Lock  lock(m_mutex);
    queue = m_send_queue;
    m_send_queue.clear();
  }
  if(m_hashHost.empty()){
    m_isVaild = false;
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"all server[%d] m_mqserver_info [%d] is sick.",m_mqserver_clients.size(),m_mqserver_info.size());
    if(m_send_cb)
       m_send_cb(false,queue);
    if(!isRetry)
      return ; ///如果可以重试的话就将msg放到重发队列中
  }
  MurMurHash hash;
  while(!queue.empty()) {
    /// 每个主题都会发送不同的broker 上，但是同个主题的内容都会通过一致性哈希放在同一个broker上
    const IdAndTopicMessagePair front = queue.front();
    auto it = m_hashHost.find(hash(front.second.first));////对主题进行哈希找到节点
    TcpConnection::ptr conne = nullptr;
    if(it!=m_hashHost.end()){
      assert(m_vaild_conne.size());
      conne = m_vaild_conne[it->second.conneName];
    }

    for (; !queue.empty();) {
      ///取出队列的数据将其全部序列化到buff中,同时放到重发队列里
      const auto &data = queue.front();
      if(data.second.first != front.second.first){
        break; ///不同的主题，当前的主题退出循环后开始发送
      }
      MqPacket msg{};
      msg.command = MQ_COMMAND_DELIVERY;
      msg.messageId = data.first;
      msg.msgCreateTime = Timestamp::now().getMicroSeconds();
      msg.topicBytes = data.second.first.size();
      msg.msgBytes = data.second.second.size();
      serializationToBuffer(&msg, data.second.first, data.second.second, buff);
      if (m_resend_set.find(msg.messageId) == m_resend_set.end()) {
        SKYLU_LOG_FMT_DEBUG(G_LOGGER, "queue in resendSet  topic-msg[%s|%s].",
                            data.second.first.c_str(),
                            data.second.second.c_str());
        m_resend_set.insert({msg.messageId, {data.second}});
      }
      queue.pop_front();
    }
    if(conne){
      static bool first = true;
      if(first){
        first = false;
        SKYLU_LOG_FMT_ERROR(G_LOGGER,"this is no error! just for test . now : %d",Timestamp::now().getMicroSeconds());
      }
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"send topic[%s] to conne[%s]",front.second.first.c_str(),conne->getName().c_str());
      conne->send(&buff);
    }
  }



}

void Producer::start() {
  m_thread.reset(new Thread(std::bind(&Producer::run,shared_from_this()),m_name + "Thread"));
  m_thread->start();
  m_isInit = true;
}
bool Producer::send(bool isRetry) {
  if(isRetry){
    while(!m_loop);
  }
  if(!m_isVaild && !isRetry){
    SKYLU_LOG_ERROR(G_LOGGER)<<"current all connections is sick";
    return false;
  }
  m_loop->runInLoop(std::bind(&Producer::sendInLoop,this,isRetry));
  return true;
}
void Producer::put(const std::string &topic, const std::string &message) {
  Mutex::Lock lock(m_mutex);
  auto id = createMessageId();
  m_send_queue.push_back({id,{topic,message}});
}
void Producer::run() {

  EventLoop loop;
  m_loop = &loop;
  init(m_dir_addrs);
  m_loop->runEvery(kReSendTimerWithMs * Timestamp::kSecondToMilliSeconds,std::bind(&Producer::checkReSendSetTimer,this));
  MqBusd::connect();
  m_loop->loop();
  m_loop = nullptr;

}
void Producer::onMessageFromMqServer(const TcpConnection::ptr &conne,
                                     Buffer *buff) {
  while(buff->readableBytes()>sizeof(MqPacket)) {
    const MqPacket *msg = serializationToMqPacket(buff);
    switch (msg->command) {
    case MQ_COMMAND_ACK:
    case MQ_COMMAND_DELIVERY:
      handleDelivery(msg);
      break;
    default:
      SKYLU_LOG_FMT_ERROR(G_LOGGER, "invaild command[%d] form conne[%s] ",
                          msg->command, conne->getName().c_str());
      break;
    }
  }

}
void Producer::checkReSendSetTimer() {
  if(m_resend_set.empty()){
    return ;
  }
  {
    Mutex::Lock lock(m_mutex); ///FIXME 这里的锁粒度有点大
    SKYLU_LOG_DEBUG(G_LOGGER)<<"m_resend_set.size ="<<m_resend_set.size()<<"   m_send_queue ="<<m_send_queue.size();
    for(const auto& it : m_resend_set){
      m_send_queue.emplace_back(it.first,it.second);

    }

  }
  sendInLoop(true);
}
Producer::ptr
Producer::createProducer(const std::vector<Address::ptr> &dir_addrs,
                         const std::string &name) {
  Producer::ptr ptr(new Producer(dir_addrs,name));
  return ptr;

}


Producer::~Producer(){
  SKYLU_LOG_INFO(G_LOGGER)<<"Producer Destroy";
}
void Producer::stop() {
  if(m_isInit){
    while(!m_loop);///如果正在初始化的时候说明线程已经启动了 这时候需要简单等待一会m_loop在线程中被赋值
    m_loop->quit();
  }
  m_thread->join();

}
void Producer::handleDelivery(const MqPacket * msg) {

  SKYLU_LOG_FMT_DEBUG(G_LOGGER, "erase messageId(%u) from m_resend_set ",
                      msg->messageId);
  ///  assert(m_resend_set.find(msg->messageId) != m_resend_set.end());
  /// FIXME 可能会有一个现象:连续收到两个ACK（第一个是之前超时的，第二个是定时器重发拿到的,当第二个来的时候set里面已经没有当前的MessageId了
  if (m_resend_set.find(msg->messageId) != m_resend_set.end()) {
    if (m_send_ok_cb) {
      m_send_ok_cb({msg->messageId, m_resend_set[msg->messageId]});
    }
  }
  m_resend_set.erase(msg->messageId);
  if (m_resend_set.empty() && m_send_queue.empty()) {
    m_empty_queueAndsResend.notify();
  }
}
void Producer::onConnectionToMqServer(const TcpConnection::ptr &conne) {
  MqBusd::onConnectionToMqServer(conne);
  ////先默认50个虚拟节点
  for(int i =0 ;i<kVirtualNodeNum;++i){
    std::string name = conne->getName();
    int start = name.find(':') + 1;
    int end = name.find('#',start);
    vnode_t note(name.substr(start,end-start),name,i);
    m_hashHost.insert(note);
  }
  m_vaild_conne.insert({conne->getName(),conne});
  SKYLU_LOG_DEBUG(G_LOGGER)<<"add real node successful";
}
void Producer::removeInvaildConnection(const TcpConnection::ptr &conne) {
  m_vaild_conne.erase(conne->getName());
  for(int i =0 ;i<kVirtualNodeNum;++i){
    std::string name = conne->getName();
    int start = name.find(':') + 1;
    int end = name.find('#',start);
    vnode_t note(name.substr(start,end-start),name,i);
    m_hashHost.erase(note);
  }
}
void Producer::connectToMqServer() {
  static int i = 0;
  for (auto it : m_mqserver_info) {
    /// 添加最新的Client FIXME  这里会有性能的问题
    if (m_mqserver_clients.find(it.first) == m_mqserver_clients.end()
        || !m_mqserver_clients[it.first]->getConnection()){
      int flag = it.first.find(':');
      std::string host(it.first.substr(0, flag++));
      int port = atoi(it.first.substr(flag, it.first.size() - flag).c_str());
      Address::ptr addr = IPv4Address::Create(host.c_str(), port);
      m_mqserver_clients[it.first].reset(new TcpClient(
          m_loop, addr, "mqServerClient Id" + std::to_string(++i)));

      SKYLU_LOG_FMT_INFO(G_LOGGER,
                         "initMqServerClients| client host : %s port : %d",
                         host.c_str(), port);

      m_mqserver_clients[it.first]->connect();
      m_mqserver_clients[it.first]->setConnectionCallback(std::bind(
          &Producer::onConnectionToMqServer, this, std::placeholders::_1));
      m_mqserver_clients[it.first]->setMessageCallback(
          std::bind(&Producer::onMessageFromMqServer, this,
                    std::placeholders::_1, std::placeholders::_2));
      m_mqserver_clients[it.first]->setCloseCallback(std::bind(&Producer::removeInvaildConnection,
                                                                       this,std::placeholders::_1));
    }
  }
}
