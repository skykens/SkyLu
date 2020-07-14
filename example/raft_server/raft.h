//
// Created by jimlu on 2020/6/11.
//

#ifndef SKYLU_MEMCACHED_RAFT_H
#define SKYLU_MEMCACHED_RAFT_H

#include <algorithm>
#include <functional>
#include <memory>
#include <netinet/in.h>
#include <skylu/net/buffer.h>
#include <skylu/net/udpconnection.h>
#include <skylu/base/mutex.h>
#include <unordered_map>
#include <atomic>

#include "proto.h"
#include "skylu/base/log.h"
#include "skylu/base/mutex.h"
#include "skylu/base/nocopyable.h"
#include "skylu/base/threadpool.h"
#include "skylu/net/socket.h"
#include "skylu/net/eventloop.h"

namespace skylu{
namespace raft {



struct Config{
  int peernum_max;
  int heartbeat_ms;

  int election_ms_min;
  int election_ms_max;

  int log_len;


  int chunk_len;  /// Update中data的最大大小
  int msg_len_max;  /// 在网络中传输的数据包最大长度 (UDP)

  void *userdata;   //全部的数据
  Applier  applier;
  Snapshooter  snapshooter;
};

class Raft : public Nocopyable {



  static const int kMsgUpdate = 0;
  static const int kMsgDone = 1;
  static const int kMsgClaim = 2;
  static const int kMsgVote = 3;
  static const char * kDefaultHost;
  static const uint32_t kDefaultPort = 6543;
  static const uint32_t  kUdpMaxSize = 508;
  enum {
    NOBODY = -1
  };

public:
/**
 * 机器信息
 */
  struct  Peer{
    typedef std::shared_ptr<Peer> ptr;
    bool up; /// 机器是否上线
    int sequence;  ///包序号(保证确定性)
    Process acked;  /// 接收记录，Leader，一般当成为Leader的时候会重置这个
    int applied;  /// 心跳包发出去的数量 ：leader时
    Socket::ptr host;
    int silent_ms;   /// 等待时间
    Peer():up(false)
        ,sequence(0)
        ,applied(0)
        ,host(nullptr)
        ,silent_ms(0){
    }
  };

public:
  static bool ConfigIsOk(Config& config);
  explicit Raft(const Config & config,EventLoop *loop);

  /**
   * 建立 Peer
   * @param id
   * @param host
   * @param port
   * @param self
   * @return
   */
  Peer::ptr peerUp(int id,const std::string& host,int port,bool self);

  int progress() const{return m_log.applied;}
  void peerDown(int id);
  void resetTimer();
  void setConneToPeer(const UdpConnection::ptr & conne){m_conne_to_peer = conne;}
  void setCheckWaitClientCallback(const std::function<void()> &cb){ m_check_cient_cb = cb;}

  /**
   * 写请求到来需要发送同步信息到其他peer
   * @param update
   * @return
   */
  int emit(Update update);
  bool applied(int id,int index) const;

  void tick(int msec);
  Config* getConfig(){return &m_config;}

  /**
   * 收到其他Peer发来的消息
   * @return
   */
  void recvMessage(const UdpConnection::ptr& conne,const Address::ptr& remote,Buffer *buff);
  /**
   * 处理Peer传来的消息
   * @param msg
   */
  void handleMessage(MsgData* msg);


  bool isLeader(){return m_role == Leader;}
  int getLeader() const{return m_leaderId;}

  void checkIsLeaderWithMs();




private:


  static inline bool inRange(int min,int x,int max){assert(min<=max); return (min<=x)&&(x<=max);}
  static inline int randBetweenTime(int min,int max){
  //  srand(time(NULL));  不需要，在调用前用srand(id)播下随机数种子了
    return rand() % (max - min +1) +min;
  }
  inline Entry * log(int index){return &m_log.entries[(index)%m_config.log_len];}
  inline int logFirstIndex() const{return m_log.first;}
  inline int logLastIndex() const{return m_log.first + m_log.size-1;}
  inline bool logIsFull() const{return m_log.size == m_config.log_len;}

  /**
   * 应用到大部分的Peer上
   * @return
   */
  int apply();
  static bool msgSize(MsgData * m, int mlen);


  void send(int dst,void *data,int len);
  /**
   * 发送心跳 同时向peer同步本机之前的log(DFS的发)
   * @param dst  目标id
   */
  void beat(int dst);
  void resetBytesAcked();
  void resetSilentTime(int id);
/**
 * 增加每个peer等待时间
 * @param ms
 * @return 选举定时器超时的peer个数
 */
  int increaseSilentTime(int ms);
  bool becomeLeader();
  void becomeFollower();
  /**
   * 拉票
   */
  void claim();
  /**
   * 刷新Peer的确认 当大部分Peer都确认了就会调用Apply();
   */
  void refreshAcked();
  /**
   * 压缩 logApplier 初始化已经apply的条目
   * @return
   */
  int compact();
  /**
   * 从快照中恢复
   * @param previndex
   * @param e
   * @return
   */
  bool restore(int previndex,Entry *e);
  /**
   * 判断能不能追加日志
   * @param previndex
   * @param prevterm
   * @return
   */
  bool appendable(int previndex,int prevterm);
  /**
   * 追加日志
   * @param previndex
   * @param prevterm
   * @param e
   * @return
   */
  bool append(int previndex,int prevterm,Entry *e);
  /**
   * 处理心跳包、日志条目、快照
   * @param msg
   */
  void handleUpdate(MsgUpdate *msg);
  /**
   * 处理应答包(一般是Leader使用的)
   * @param msg
   */
  void handleDone(MsgDone *msg);
  /**
   * 处理选举(一般是Follower使用的)
   * @param msg
   */
  void handleClaim(MsgClaim *msg);
  /**
   *  处理投票(一般是Candidate使用)
   * @param msg
   */
  void handleVote(MsgVote *msg);
  void setTerm(int term);


public:
  enum Role{
    Follower,
    Candidate,
    Leader
  };

private:
  int m_term;
  int m_voteFor;
  Role m_role;
  int m_me; // 自己的id
  int m_votes;//票数
  int m_leaderId;
  int m_timer;  // 定时器
  Socket::ptr  m_socket; // udp socket   // 仅用来bind端口
  UdpConnection::ptr m_conne_to_peer;  // 发送数据给其他peer 的


  EventLoop *m_loop;
  Config m_config;

  Log m_log;

  std::vector<Peer::ptr> m_peers;
  int m_peerNum;  // 上线的peer数量
  std::function<void()> m_check_cient_cb;


};

}
}

#endif // SKYLU_MEMCACHED_RAFT_H
