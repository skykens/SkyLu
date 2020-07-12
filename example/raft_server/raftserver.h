//
// Created by jimlu on 2020/7/9.
//

#ifndef SKYLU_RAFT_RAFTSERVER_H
#define SKYLU_RAFT_RAFTSERVER_H

#include <memory>
#include <utility>
#include <unordered_map>
#include "skylu/net/tcpserver.h"
#include "skylu/net/eventloop.h"
#include "skylu/net/udpconnection.h"
#include "raft.h"
#include "proto.h"


namespace skylu{
namespace raft{

class RaftServer {

  explicit RaftServer(EventLoop *owner
                      , std::string  name
                      ,int argc,char **argv);
  ~RaftServer() = default;
  /**
   * 设置快照函数
   * @param cb
   */
  void setSnapshooter(Snapshooter cb){m_snapshooter_cb = std::move(cb);}
  /**
   * 设置数据落地的函数
   * @param cb
   */
  void setApplier(Applier cb){m_applier_cb = std::move(cb);}
  void setThreadNum(int num){m_server->setThreadNum(num);}
  /**
   * 设置客户端消息来的时候的回调
   * @param cb
   */
  void setMessageCallback(const TcpConnection::MessageCallback &cb){m_server->setMessageCallback(cb);}
  void init();
  void run();

private:
  bool loadConfig();



private:
  // TODO 这里用指针直接存储是因为这个对象生命期是整个程序的生命期
  Raft * m_raft;
  std::unique_ptr<TcpServer> m_server;  // 处理客户端数据
  UdpConnection::ptr m_conne;  // 处理peer的数据
  EventLoop * m_loop;
  int m_id;
  std::string m_name;
  char * m_log;
  bool isDaemonize;
  Snapshooter m_snapshooter_cb;
  Applier m_applier_cb;
  int m_argc;
  char ** m_argv;
  bool m_init;

};
}
}

#endif // SKYLU_RAFT_RAFTSERVER_H
