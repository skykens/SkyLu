//
// Created by jimlu on 2020/6/11.
//

#ifndef SKYLU_MEMCACHED_RAFT_H
#define SKYLU_MEMCACHED_RAFT_H

#include <functional>
#include <memory>
#include <netinet/in.h>
#include <algorithm>
#include <netdb.h>

#include "skylu/base/mutex.h"
#include "skylu/base/threadpool.h"
#include "skylu/base/nocopyable.h"
#include "skylu/base/log.h"

/**
 * 根据MIT实验中的raft 从GO 迁移过来的C++ 版本
 *
 */



#define DEFAULT_LISTENHOST "0.0.0.0"
#define DEFAULT_LISTENPORT 6543
#define UDP_SAFE_SIZE 508
#define NOBODY -1

namespace skylu{
namespace raft {

/**
 *
 */
struct Update{
  int len;
  char * data;
  void *userdata;

};


/**
 * 心跳包
 */
struct Entry{
  int term;
  bool snapshot;
  Update update;
  int bytes;
  Entry(){
    term = 0;
    snapshot = false;
    bytes = 0;
    update.data = nullptr;
    update.userdata = nullptr;
    update.len = 0;
  }

};

/**
 *
 */
struct Log{
  int first;  /// 第一条的位置
  int size;  ///从第一条开始的数目
  int acked;  ///其他peer确认复制的数量
  int applied; /// 提交的数量
  std::vector<Entry> entries;  ///批量发出去的心跳包
  Entry newentry; /// 收到的心跳包
  Log(size_t length): first(0)
        ,size(0)
        ,acked(0)
        ,applied(0)
        ,entries(length){



  }
};

/**
 * 用来跟踪有多少个心跳包被发送
 */
struct Process{
  int entries;  /// 全部sent/acked 的心跳包
  int bytes;  ///  当前send/acked的总bytes
  Process()
      :entries(0)
        ,bytes(0){

  };
};

/**
 * 机器信息
 */
struct  Peer{
  bool up; ///是否使用
  int sequence;  ///rpc序号
  Process acked;  /// 接收的序号 ： acked
  int applied;  /// 心跳包发出去的数量 ： leader时
  std::string host;
  int port;
  struct sockaddr_in addr;
  int silent_ms;   /// 等待时间
  Peer():up(false)
        ,sequence(0)
        ,applied(0)
        ,host(DEFAULT_LISTENHOST)
        ,port(DEFAULT_LISTENPORT)
        ,silent_ms(0){
  }
};



typedef std::function<void(void*,Update,bool)> Applier;
typedef std::function<Update(void*)> Snapshooter;

struct Config{
  int peernum_max;
  int heartbeat_ms;

  int election_ms_min;
  int election_ms_max;

  int log_len;


  int chunk_len;  /// TODO
  int msg_len_max;  /// 在网络中传输的数据包最大长度

  void *userdata;
  Applier  applier;
  Snapshooter  snapshooter;
};
struct MsgData{
  int msgtype;
  int curterm;
  int from;
  int sequence;
};

struct MsgUpdate{
  MsgData msg;
  bool snapshot;
  int previndex;
  int prevterm;

  bool empty;  //心跳包的时候这个为真

  int entryTerm;
  int totalLen; //整个update大小
  int acked;
  int offset;
  int len;
  char data[1];
};

struct MsgDone{
  MsgData msg;
  int entryTerm;
  Process process;
  int applied;
  bool success;
};

struct MsgClaim{
  MsgData msg;
  int index;
  int lastTerm;

};

struct MsgVote{
  MsgData msg;
  bool granted;
};


typedef union{
  MsgUpdate update;
  MsgClaim claim;
  MsgVote vote;
  MsgDone done;
}MsgAny;



class Raft : public Nocopyable {


public:
  static bool ConfigIsOk(Config& config);
  Raft(const Config & config);
  bool peerUp(int id,std::string host,int port,bool self);
  int progress();
  void peerDown(int id);
  void resetTimer();

  int emit(Update update);
  bool applied(int id,int index);

  void tick(int msec);

  MsgData * recvMessage();
  void handleMessage(MsgData* msg);

  int createUdpSocket();

  bool isLeader();
  int getLeader();




private:

  inline bool inrange(int min,int x,int max){assert(min<=max); return (min<=x)&&(x<=max);}
  inline int randBetweenTime(int min,int max){
  //  srand(time(NULL));  不需要，在调用前用srand(id)播下随机数种子了
    return rand() % (max - min +1) +min;
  }
  inline Entry * log(int index){return &m_log.entries[(index)%m_config.log_len];}
  inline int logFirstIndex(){return m_log.first;}
  inline int logLastIndex(){return m_log.first + m_log.size-1;}
  inline bool logIsFull(){return m_log.size == m_config.log_len;}

  int apply();
  void setRecvTimeout(int ms);
  void setReuseaddr();
  bool msgSize(MsgData * m, int mlen);


  void send(int dst,void *data,int len);
  void beat(int dst);
  void resetBytesAcked();
  void resetSilentTime(int id);
  int increaseSilentTime(int ms);
  bool becomeLeader();
  void claim();
  void refreshAcked();
  int compact();
  bool restore(int previndex,Entry *e);
  bool appendable(int previndex,int prevterm);
  bool append(int previndex,int prevterm,Entry *e);
  void handleUpdate(MsgUpdate *msg);
  void handleDone(MsgDone *msg);
  void handleClaim(MsgClaim *msg);
  void handleVote(MsgVote *msg);
  void setTerm(int term);


public:
  enum Role{
    Follower,
    Candidate,
    Leader
  };

  static const int kMsgUpdate = 0;
  static const int kMsgDone = 1;
  static const int kMsgClaim = 2;
  static const int kMsgVote = 3;




private:
  int m_term;
  int m_voteFor;
  Role m_role;
  int m_me; // 自己的id
  int m_votes;//票数
  int m_leaderId;
  int m_sockFd;

  int m_timer;  // 选举时间
  Config m_config;

  Log m_log;

  int m_peerNum;
  std::vector<Peer *> m_peers;


};
}
}

#endif // SKYLU_MEMCACHED_RAFT_H
