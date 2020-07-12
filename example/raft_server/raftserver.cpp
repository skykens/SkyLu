//
// Created by jimlu on 2020/7/9.
//

#include "raftserver.h"

#include <memory>
#include <utility>
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

  m_server->start();

}
bool RaftServer::loadConfig() {

  int id;
  char *host;
  char *str;
  int port;
  int opt;
  m_id = -1;
  json_t* state = json_object();
  Config rc;
  rc.peernum_max = 64;
  rc.heartbeat_ms = 50;
  rc.election_ms_min = 300;
  rc.election_ms_max = 600;
  rc.log_len = 10;
  rc.chunk_len = 400;
  rc.msg_len_max = 500;
  rc.userdata = state;
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
        m_conne.reset(new UdpConnection(m_loop,p->host,"raft_server " +std::to_string(id)+" udp "));
        m_conne->setMessageCallback(std::bind(&Raft::recvMessage
                                              ,m_raft
                                              ,std::placeholders::_1
                                              ,std::placeholders::_2
                                              ,std::placeholders::_3));
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
  assert(m_conne);

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
    }
  });

  // 定期更新本地peers的状态
  m_loop->runEvery(m_raft->getConfig()->heartbeat_ms
                   ,std::bind(&Raft::checkIsLeaderWithMs,m_raft));
  m_raft->resetTimer();


  m_init = true;
  // TODO 屏蔽一些信号

}

}
}
