//
// Created by jimlu on 2020/6/30.
//

#ifndef SKYLU_RAFT_PROTO_H
#define SKYLU_RAFT_PROTO_H

#include <sys/types.h>
#include <functional>
#include <algorithm>
#include <vector>
#define MEAN_FAIL '!'
#define MEAN_OK   '.'
#define MEAN_GET  '?'
#define MEAN_SET  '='
typedef struct Key {
  char data[64];
} Key;

typedef struct Value {
  char data[64];
} Value;


typedef struct Message {
  char meaning;
  Key key;
  Value value;
} Message;


struct Update{
  int len;
  char * data;
  void *userdata;

};


/**
 * 日志条目
 */
struct Entry{
  int term;
  bool snapshot;
  Update update{};
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
  int applied; /// 当前日志复制所在的位置
  std::vector<Entry> entries;  /// 所有日志
  Entry newentry; /// 当前新任期下的日志
  explicit Log(size_t length): first(0)
      ,size(0)
      ,acked(0)
      ,applied(0)
      ,entries(length){



  }
};

/**
 * 日志条目发送的过程
 */
struct Process{
  int entries;  /// 已经接受了entry的 数量
  int bytes;  // 当前收到的apply's bytes
  Process()
      :entries(0)
      ,bytes(0){

  };
};





typedef std::function<void(void*,Update,bool)> Applier;  // 日志落地
typedef std::function<Update(void*)> Snapshooter;  //数据库快照

struct MsgData{
  int msgtype;
  int curterm;
  int from;
  int sequence;
};

/**
 * 用来更新日志条目的
 */
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
  char data[1]; // 这里的大小由Config中的chunk决定
};

/**
 * Update之后的应答包
 */
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
static void usage(char *prog) {
  printf(
      "Usage: %s -i [本机ID] -r ID:HOST:PORT [-r ID:HOST:PORT ...] [-l LOGFILE]\n"
      "   -l : 当后台运行的时候 日志写入LOGFILE\n",
      prog
  );
}
#endif // SKYLU_RAFT_PROTO_H
