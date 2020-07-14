//
// Created by jimlu on 2020/7/9.
//

#include "raftserver.h"

#include <memory>
#include <utility>
#include <jansson.h>
#include "skylu/base/daemon.h"
namespace skylu{
namespace raft{

RaftServer::RaftServer(EventLoop *owner
                       , std::string name
                       ,int argc,char** argv)
    : m_raft(nullptr)
      ,m_server(nullptr)
      ,m_conne(nullptr)
      ,m_loop(owner)
      ,m_id(-1)
      ,m_name(std::move(name))
      ,m_log(nullptr)
      ,isDaemonize(false)
      ,m_snapshooter_cb(nullptr)
      ,m_applier_cb(nullptr)
      ,m_argc(argc)
      ,m_argv(argv)
      ,m_init(false){

}
void RaftServer::run() {
  assert(m_snapshooter_cb);
  assert(m_applier_cb);
  assert(m_init);

  m_server->setThreadNum(0);
  m_server->start();
  m_loop->runEvery(m_raft->getConfig()->heartbeat_ms * Timestamp::kSecondToMilliSeconds
      ,std::bind(&Raft::tick,m_raft,m_raft->getConfig()->heartbeat_ms));
  m_raft->resetTimer();
  m_loop->loop();

}
bool RaftServer::loadConfig() {

  int id;
  char *host;
  char *str;
  int port;
  int opt;
  m_id = -1;
  m_state = json_object();
  Config rc;
  rc.peernum_max = 64;
  rc.heartbeat_ms = 50;
  rc.election_ms_min = 500;
  rc.election_ms_max = 700;
  rc.log_len = 10;
  rc.chunk_len = 400;
  rc.msg_len_max = 500;
  rc.userdata = m_state;
  rc.applier = m_applier_cb;
  rc.snapshooter = m_snapshooter_cb;
  assert(Raft::ConfigIsOk(rc));
  m_raft = new Raft(rc,m_loop);
  m_loop->setPollTimeoutMS(rc.heartbeat_ms);

  int peernum = 0;
  while ((opt = getopt(m_argc, m_argv, "hi:r:l:")) != -1) {
    switch (opt) {
    case 'i':
      m_id = atoi(optarg);
      break;
    case 'l':
      m_log = optarg;
      isDaemonize = true;
      break;
    case 'h':
      usage(m_argv[0]);
      exit(EXIT_FAILURE);
      break;
    default:
      usage(m_argv[0]);
      exit(EXIT_FAILURE);
    case 'r':
      if (m_id == -1) {
        usage(m_argv[0]);
        exit(EXIT_FAILURE);
      }

      str = strtok(optarg, ":");
      if (str) {
        id = atoi(str);
      } else {
        usage(m_argv[0]);
        exit(EXIT_FAILURE);
      }

      host = strtok(nullptr, ":");

      str = strtok(nullptr, ":");
      if (str) {
        port = atoi(str);
      } else {
        exit(EXIT_FAILURE);
      }

      Raft::Peer::ptr p = m_raft->peerUp(id, host, port, id == m_id);
      if (p == nullptr){
        exit(EXIT_FAILURE);
      }

      if (id == m_id) {
        Address::ptr addr = IPv4Address::Create(host,port);
        m_server.reset(new TcpServer(m_loop,addr,m_name));
        m_server->setMessageCallback(std::bind(&RaftServer::onMessage,this,std::placeholders::_1,std::placeholders::_2));
        m_conne.reset(new UdpConnection(m_loop,p->host,"raft_server " +std::to_string(id)+" udp "));
        m_conne->setMessageCallback(std::bind(&Raft::recvMessage
                                              ,m_raft
                                              ,std::placeholders::_1
                                              ,std::placeholders::_2
                                              ,std::placeholders::_3));
        m_raft->setConneToPeer(m_conne);

      }
      peernum++;
      break;
    }
  }

  if(m_server == nullptr){
    usage(m_argv[0]);
    exit(EXIT_FAILURE);
  }
  return true;
}
void RaftServer::init() {
  assert(m_applier_cb);
  assert(m_snapshooter_cb);

  loadConfig();
  if(m_loop){
    if (!freopen(m_log, "a", stdout)) {
      exit(EXIT_FAILURE);
    }
    if (!freopen(m_log, "a", stderr)) {
      exit(EXIT_FAILURE);
    }
  }
  if(isDaemonize){
    Daemon::start();
  }

  m_server->setConnectionCallback([this](const TcpConnection::ptr &conn){
    if(!m_raft->isLeader()){
      // 只有leader 可以接收client请求
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "not a leader ,disconnecting the accepted connection name = %s"
                          ,conn->getName().c_str());
      conn->shutdown();
      return;
    }

  });
  m_raft->setCheckWaitClientCallback(std::bind(&RaftServer::CheckClientApplied,this));

  m_init = true;
  // TODO 屏蔽一些信号

}
void RaftServer::onMessage(const TcpConnection::ptr &conne, Buffer *buff) {


  if(!m_raft->isLeader())
  {
    conne->shutdown();
    return ;
  }

  assert(m_wait_clients.find(conne) == m_wait_clients.end()); /// TODO 不能接收来自wait map 中的client   后面考虑改进一下
  int index;
  auto * msg = (Message *)const_cast<char *>(buff->curRead());
  if (msg->meaning == MEAN_SET) {

    auto *c = new Client ;
    char buf[sizeof(Message) + 10];
    snprintf(buf, sizeof(buf), "{\"%s\": \"%s\"}", msg->key.data, msg->value.data);
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"emit update: %s", buf);
    Update update{static_cast<int>(strlen(buf)), buf, nullptr};
    index = m_raft->emit(update);
    if (index < 0) {
      /// 一般在log满的时候
      SKYLU_LOG_FMT_ERROR(G_LOGGER,"failed to emit a raft update index = %d",index);
      delete c;

    } else {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"client is waiting for %d", index);
      c->expect = index;
      m_wait_clients.insert({conne,c});  // 进入等待状态队列 ~

    }
  } else if (msg->meaning == MEAN_GET) {
    json_t *jval = json_object_get(m_state, msg->key.data);
    if (jval == NULL) {
      msg->meaning = MEAN_FAIL;
    } else {
      msg->meaning = MEAN_OK;
      strncpy(msg->value.data, json_string_value(jval), sizeof(msg->value));
    }
    conne->send(buff);
    // 写请求可以直接发送回去了
  } else {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"unknown meaning %d of the client's message\n", msg->meaning);
    conne->shutdown();
  }




}
void RaftServer::CheckClientApplied() {
  if(m_wait_clients.empty())
    return;
  for (auto it = m_wait_clients.begin();it!= m_wait_clients.end();) {
    Client *c = it->second;
    assert(c->expect >= 0);
    if(!m_raft->applied(m_id,c->expect))
    {
      continue;
    }
    c->expect = -1;
    Buffer* buff = it->first->getInputBuffer(); /// TODO 这里存在一个风险吧  如果客户端持续往里面写的话InputBuffer就会被破坏，所以onMessage加上了一个不能够在WAITING状态写数据。后面考虑优化
    auto * msg = (Message *)const_cast<char *>(buff->curRead());
    msg->meaning = MEAN_OK;
    it->first->send(buff);
    m_wait_clients[it->first] = nullptr;
     m_wait_clients.erase(it++);

    delete c;

  }
}
void RaftServer::onCloseConnection(const TcpConnection::ptr &conne) {
  m_wait_clients.erase(conne);

}

}
}
