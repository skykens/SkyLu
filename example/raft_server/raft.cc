//
// Created by jimlu on 2020/6/11.
//

#include "raft.h"
#include <arpa/inet.h>
#include <skylu/net/udpconnection.h>
namespace skylu{
namespace raft{

bool Raft::ConfigIsOk(Config& config) {
  bool ok = true;
  if(config.peernum_max < 3){
    SKYLU_LOG_ERROR(G_LOGGER)<<"please ensure peernum_max >=3";
    ok = false;
  }

  if (config.heartbeat_ms >= config.election_ms_min) {
    SKYLU_LOG_ERROR(G_LOGGER)<<"please ensure heartbeat_ms < election_ms_min (substantially)";
    ok = false;
  }

  if (config.election_ms_min >= config.election_ms_max) {
    SKYLU_LOG_ERROR(G_LOGGER)<<"please ensure election_ms_min < election_ms_max";
    ok = false;
  }

  if(sizeof(MsgUpdate)+config.chunk_len-1>kUdpMaxSize){
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
                        "please ensure chunk_len <= %lu, %d is too much for UDP\n",
                        kUdpMaxSize - sizeof(MsgUpdate) + 1,
                        config.chunk_len
                        );
    ok = false;
  }

  if (static_cast<size_t>(config.msg_len_max) < sizeof(MsgAny)) {
    SKYLU_LOG_ERROR(G_LOGGER)<<"please ensure msg_len_max >= "<<sizeof(MsgAny);
    ok = false;
  }

  return ok;
}
Raft::Raft(const skylu::raft::Config &config,EventLoop * loop)
      :m_term(0)
      ,m_voteFor(NOBODY)
      ,m_role(Follower)
      ,m_me(NOBODY)
      ,m_votes(0)
      ,m_leaderId(NOBODY)
      ,m_loop(loop)
      ,m_config(config)
      ,m_log(config.log_len)
      ,m_peers(config.peernum_max)
      ,m_peerNum(0){
  assert(static_cast<int>(m_peers.size()) == m_config.peernum_max);
  for(auto & m_peer : m_peers){
    m_peer.reset(new Peer);
  }

}
Raft::Peer::ptr Raft::peerUp(int id, const std::string& host, int port, bool self) {

  auto p =  m_peers[id];

  if( static_cast<int>(m_peers.size()) > m_config.peernum_max || id > m_config.peernum_max){
    SKYLU_LOG_ERROR(G_LOGGER)<<"too many peers";
    return nullptr;

  }
  p->up = true;
  Address::ptr addr = IPv4Address::Create(host.c_str(),port);
  p->host = Socket::CreateUDP(addr);
  if(self){
    m_socket = p->host;
    if(m_me != NOBODY){
      SKYLU_LOG_ERROR(G_LOGGER)<<"cannot set  'self ' peer multiple times ";
    }
    m_socket->bind(addr);
    m_me = id;
    srand(id);
    resetTimer();
  }
  m_peerNum++;

  return p;


}


void Raft::peerDown(int id) {
  auto p  = m_peers[id];
  p->up = false;
  if(m_me == id){
    m_me = NOBODY;
  }
  m_peerNum--;
}



void Raft::resetTimer() {


  if(m_role == Leader){

    if(m_timer == m_config.heartbeat_ms){

      m_timer += m_config.heartbeat_ms;
    }else{

      m_timer = m_config.heartbeat_ms;
    }

  }else {
    /// Candidate and Follower  当超过这个时间的时候还没有收到beat就可以开始选举了
    m_timer = randBetweenTime(m_config.election_ms_min,m_config.election_ms_max);
  }
}
bool Raft::applied(int id, int index) const {
  if(m_me == id){
    return m_log.applied > index;

  }else{
    auto p = m_peers[id];
    if(!p->up){
      return false;
    }
    return p->applied > index;
  }
}

int Raft::apply() {
  int applied_now = 0;

  while(m_log.applied < m_log.acked && (m_log.applied <= logLastIndex())){
    Entry * e = log(m_log.applied);
    assert(e->update.len == e->bytes);
    m_config.applier(m_config.userdata,e->update,false);
    m_log.applied++;
    applied_now++;
  }

  return applied_now;

}




bool Raft::msgSize(MsgData *m, int mlen) {
  switch (m->msgtype) {
  case kMsgUpdate:
    return mlen == static_cast<int>(sizeof(MsgUpdate)) + ((MsgUpdate*)m)->len -1;
  case kMsgDone:
    return mlen == sizeof(MsgDone);
  case kMsgClaim:
    return mlen == sizeof(MsgClaim);
  case kMsgVote:
    return mlen == sizeof(MsgVote);

  }
  return false;

}

void Raft::send(int dst, void *data, int len) {

  assert(m_peers[dst]->up);
  assert(len <= m_config.msg_len_max);
  assert(msgSize((MsgData *)data,len));
  assert(((MsgData*)data)->msgtype >= 0);
  assert(((MsgData*)data)->msgtype < 4);
  assert(dst >= 0);
  assert(dst < m_config.peernum_max);
  assert(dst != m_me);
  assert(((MsgData*)data)->from == m_me);
  assert(m_conne_to_peer);

 // auto peer = m_peers[dst];


  //int res = m_socket->sendTo(data,len,peer->host->getLocalAddress());
   m_conne_to_peer->send(data,len,m_peers[dst]->host->getLocalAddress());

  /*f (res == -1) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
        "failed to send a msg to [%d]: %s\n",
        dst, strerror(errno)
    );
  }
   */
}

void Raft::beat(int dst){
  if(dst == NOBODY){
    int i;
    for(i = 0; i< m_config.peernum_max;i++){
      if(!m_peers[i]->up)
        continue;
      if(i == m_me)
        continue;
      beat(i);
    } ///DFS
    return;
  }

  assert(m_leaderId == m_me);
  assert(m_role == Leader);

  auto p = m_peers[dst];

  auto msg = (MsgUpdate*)malloc(sizeof(MsgUpdate)+m_config.chunk_len - 1);

  msg->msg.msgtype = kMsgUpdate;
  msg->msg.curterm = m_term;
  msg->msg.from = m_me;

  if (p->acked.entries <= logLastIndex()) {
    // 同步之前的log
    int sendindex;

    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"%d has acked %d:%d\n", dst, p->acked.entries, p->acked.bytes);

    if (p->acked.entries < logFirstIndex()) {
      // 这里的情况通常是peer 重新上线

      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending the snapshot to %d\n", dst);
      sendindex = logFirstIndex();
      //发送snapshot
      assert(log(sendindex)->snapshot);
    } else {
      // Peer 只有少量没有提交的情况
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending update %d snapshot to %d\n", p->acked.entries, dst);
      sendindex = p->acked.entries;
    }

    msg->snapshot = log(sendindex)->snapshot;

    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"will send index %d to %d\n", sendindex, dst);

    msg->previndex = sendindex - 1;
    Entry *entry = log(sendindex);

    if (msg->previndex >= 0) {
      msg->prevterm = log(msg->previndex)->term;
    } else {
      msg->prevterm = -1;
    }
    msg->entryTerm = entry->term;
    msg->totalLen = entry->update.len;
    msg->empty = false;
    msg->offset = p->acked.bytes;
    msg->len = std::min(m_config.chunk_len, msg->totalLen - msg->offset);
    assert(msg->len > 0);
    memcpy(msg->data, entry->update.data + msg->offset, msg->len);
  } else {
    // 最新的entry  发送心跳
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending empty heartbeat to %d\n", dst);
    msg->empty = true;
    msg->len = 0;
  }
  msg->acked = m_log.acked;

  p->sequence++;
  msg->msg.sequence = p->sequence;
  if (!msg->empty) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
        "sending seqno=%d to %d: offset=%d size=%d total=%d term=%d snapshot=%s\n",
        msg->msg.sequence, dst, msg->offset, msg->len, msg->totalLen, msg->entryTerm, msg->snapshot ? "true" : "false"
    );
  } else {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending seqno=%d to %d: heartbeat\n", msg->msg.sequence, dst);
  }

  send(dst, msg, sizeof(MsgUpdate) + msg->len - 1);
  assert(msg);
  free(msg);


}

void Raft::resetBytesAcked() {
  for(int i=0;i<m_config.peernum_max;i++){
    m_peers[i]->acked.bytes = 0;

  }
}

void Raft::resetSilentTime(int id) {
  for(int i=0;i<m_config.peernum_max;i++){
    if((i == id)||(id == NOBODY)){
    //  Mutex::Lock lock(m_peers[i]->mutex);
      m_peers[i]->silent_ms = 0;
    }
  }
}


int Raft::increaseSilentTime(int ms) {
  int recent_peers = 1;
  for(int i=0;i<m_config.peernum_max;i++){
    if(!m_peers[i]->up)
      continue;
    if( i == m_me)
      continue;
   // Mutex::Lock  lock(m_peers[i]->mutex);
    m_peers[i]->silent_ms +=ms;

    if(m_peers[i]->silent_ms < m_config.election_ms_max){
      recent_peers++;
    }
  }

  return recent_peers;
}


bool Raft::becomeLeader() {
  if(m_votes *2 > static_cast<int>(m_peerNum)){
    m_role = Leader;
    m_leaderId = m_me;
    resetBytesAcked();
    resetSilentTime(NOBODY);
    resetTimer();
    SKYLU_LOG_DEBUG(G_LOGGER)<<"become the leader";
    return  true;
  }
  return  false;
}


void Raft::claim() {
  assert(m_role == Candidate);
  assert(m_leaderId == NOBODY);

  m_votes = 1;
  if(becomeLeader()){
    return ;
  }

  MsgClaim m{};
  m.msg.msgtype = kMsgClaim;
  m.msg.curterm = m_term;
  m.msg.from = m_me;
  m.index = logLastIndex();

  if(m.index >= 0){
    m.lastTerm = log(m.index)->term;
  }else{
    m.lastTerm = -1;
  }

  for(int i=0;i<m_config.peernum_max;i++){
    if(!m_peers[i]->up)
      continue;
    if(i == m_me)
      continue;
    auto p= m_peers[i];
    p->sequence++;
    m.msg.sequence = p->sequence;

    send(i,&m,sizeof(m));
  }
}

void Raft::refreshAcked() {

  for (int i = 0; i < m_config.peernum_max; i++) {
    auto p = m_peers[i];
    if (i == m_me) continue;
    if (!p->up) continue;

    int newacked = p->acked.entries;

    if (newacked <= m_log.acked)
      continue;

    int replication = 1; // 日志复制成功的数量 （包括自己）
    for (int j = 0; j < m_config.peernum_max; j++) {
      if (j == m_me) continue;

      auto tmp = m_peers[j];
      if (tmp->acked.entries >= newacked) {
        replication++;
      }
    }

    assert(replication <= m_peerNum);

    if (replication * 2 > m_peerNum) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"===== GLOBAL PROGRESS: %d\n", newacked);
      m_log.acked = newacked;
    }
  }

  int applied = apply();
  if (applied) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"applied %d updates\n", applied);
  }
}


void Raft::tick(int msec) {

  m_timer -= msec;
  if(m_timer < 0) {
    switch (m_role) {
    case Follower:
      SKYLU_LOG_INFO(G_LOGGER) << "lost the leader,"
                                  " claiming leadership";
      m_leaderId = NOBODY;
      m_role = Candidate;
      m_term++;
      claim();
      break;
    case Candidate:
      SKYLU_LOG_INFO(G_LOGGER) << "the vote failed,"
                                  " claiming leadership";
      m_term++;
      claim();
      break;
    case Leader:
      beat(NOBODY);
      break;
    }
    resetTimer();
  }
   refreshAcked();
   checkIsLeaderWithMs();
   if(m_check_cient_cb && isLeader())
      m_check_cient_cb();


}

void Raft::checkIsLeaderWithMs()
{
  // 通过计算每个peer的选举心跳时间来判断自己现在是不是leader
  int recent_peers = increaseSilentTime(m_config.heartbeat_ms);
  if ((m_role == Leader)
      && (recent_peers * 2 <= m_peerNum)) {
    SKYLU_LOG_INFO(G_LOGGER)<<"lost quorum, demoting";
    becomeFollower();
  }


}

int Raft::compact() {

  Log *logTmp = &m_log;
  int compacted = 0;
  // 本地log
  for (int i = logTmp->first; i < logTmp->applied; i++) {
    Entry *entry = log(i);

    entry->snapshot = false;
    assert(entry->update.data);
    free(entry->update.data);
    entry->update.len = 0;
    entry->update.data = nullptr;

    compacted++;
  }
  if (compacted) {
    logTmp->first += compacted - 1;
    logTmp->size -= compacted - 1;
    Entry*entry = log(logFirstIndex());
    entry->update = m_config.snapshooter(m_config.userdata);
    entry->bytes = entry->update.len;
    entry->snapshot = true;
    assert(logTmp->first == logTmp->applied - 1);

    for (int i = 0; i < m_config.peernum_max; i++) {
      auto p = m_peers[i];
      if (!p->up) continue;
      if (i == m_me) continue;
      if (p->acked.entries + 1 <= logTmp->first)
        p->acked.bytes = 0;
    }
  }
  return compacted;
}


bool Raft::restore(int previndex, Entry *e) {
  int i;

  assert(e->bytes == e->update.len);
  assert(e->snapshot);

  ///清空log
  for(i = logFirstIndex(); i<= logLastIndex();i++){
    Entry * victim = log(i);
    assert(victim->update.data);
    free(victim->update.data);
    victim->update.len = 0;
    victim->update.data = nullptr;
  }
  int index = previndex + 1;
  m_log.first = index;
  m_log.size = 1;
  Entry * newEntry =  log(index);
  *newEntry =  *e;
  e->update.data = nullptr;
  e->update.len = 0;
  e->update.userdata = nullptr;
  e->term = 0;
  e->snapshot = false;
  e->bytes = 0;

  m_config.applier(m_config.userdata,log(index)->update,true /*snapshot*/);
  m_log.applied = index + 1;
  return true;



}

int Raft::emit(Update update){
  assert(m_leaderId == m_me);
  assert(m_role == Leader);

  if(logIsFull()){
    int compacted = compact();
    if(compacted>1){
      SKYLU_LOG_DEBUG(G_LOGGER)<< "compacted "<<compacted<<" entries ";
    }else{
      SKYLU_LOG_INFO(G_LOGGER)<<"cannot emit new entries ,the log is full and cannot be compacted";
      return -1;
    }
  }

  int newindex = logLastIndex()+1;
  Entry * entry = log(newindex);
  entry->term = m_term;
  assert(entry->update.len == 0);
  assert(entry->update.data == nullptr);
  entry->update.len = update.len;
  entry->bytes = update.len;
  entry->update.data =(char *)malloc(update.len);
  memcpy(entry->update.data,update.data,update.len);
  m_log.size++;

  beat(NOBODY);
  resetTimer();
  return newindex;

}
bool Raft::appendable(int previndex, int prevterm) {
  int low=0,high=0;
  low = logFirstIndex();
  if(low == 0)
  {
    low = -1;
  }
  high = logLastIndex();
  if(!inRange(low,previndex,high)){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                        "previndex %d is outside log range %d-%d",
                        previndex,low,high);
    return false;
  }

  if(previndex != -1){
    Entry * entry = log(previndex);
    if(entry->term != prevterm){
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "Log term %d != prevterm %d",entry->term,prevterm);
      return false;
    }
  }

  return true;
}
bool Raft::append(int previndex, int prevterm, Entry *e) {

  assert(e->bytes == e->update.len);
  assert(!e->snapshot);


  SKYLU_LOG_FMT_DEBUG(G_LOGGER,
      "log_append(%p, previndex=%d, prevterm=%d,"
      " term=%d)\n",
      (void *)&m_log, previndex, prevterm,
      e->term
  );

  if (!appendable(previndex, prevterm))
  {
    return false;
  }

  if (previndex == logLastIndex()) {
    SKYLU_LOG_DEBUG(G_LOGGER)<<"previndex == last";

    if (logIsFull()) {
      SKYLU_LOG_DEBUG(G_LOGGER)<<"log is full";
      int compacted = compact();
      if (compacted) {
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"compacted %d entries", compacted);
      } else {
        return false;
      }
    }
  }

  int index = previndex + 1;
  Entry *slot = log(index);

  if (index <= logLastIndex()) {
    if (slot->term != e->term) {
      // 条目冲突 删除这个条目
      m_log.size = index - m_log.first;
    }
    assert(slot->update.data);
    free(slot->update.data);
  }else{
    //成功追加
    m_log.size++;

  }

  *slot = *e;
  e->term = 0;
  e->update.data = nullptr ;
  e->update.userdata = nullptr;
  e->update.len = 0;
  e->bytes = 0;
  e->snapshot = false;

  return true;
}
void Raft::handleUpdate(MsgUpdate *msg) {

  int sender = msg->msg.from;

  MsgDone reply;
  reply.msg.msgtype = kMsgDone;
  reply.msg.curterm = m_term;
  reply.msg.from = m_me;
  reply.msg.sequence = msg->msg.sequence;
  reply.success = false;

  Entry *entry = &m_log.newentry;
  Update *update = &entry->update;

  // 该条目 应该是 心跳包或快照或者是日志条目
  if (!msg->empty && !msg->snapshot &&
      !appendable(msg->previndex, msg->prevterm))
    goto finish;

  if (logLastIndex() >= 0) {
    reply.entryTerm = log(logLastIndex())->term;
  } else {
    reply.entryTerm = -1;
  }

  if (msg->msg.curterm < m_term) {
    // 拒绝旧的信息
    SKYLU_LOG_FMT_DEBUG(G_LOGGER, "refuse old message %d < %d",
                        msg->msg.curterm, m_term);
    goto finish;
  }

  if (sender != m_leaderId) {
    SKYLU_LOG_FMT_INFO(G_LOGGER, "changing leader to %d", sender);
    m_leaderId = sender;
  }
  {
    m_peers[sender]->silent_ms = 0;
  }
  resetTimer();

  if (msg->acked > m_log.acked) {
    // 更新 本地log的acked
    m_log.acked = std::min(
        m_log.first + m_log.size,
        msg->acked
    );
    auto p = m_peers[sender];
    p->acked.entries = m_log.acked;
    p->acked.bytes = 0;
  }

  if (!msg->empty) {
    // snapshot 或 entry

    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
        "got a chunk sequence=%d from %d: offset=%d size=%d total=%d term=%d snapshot=%s",
        msg->msg.sequence, sender, msg->offset, msg->len, msg->totalLen, msg->entryTerm, msg->snapshot ? "true" : "false"
    );

    if ((msg->offset > 0) && (entry->term != msg->entryTerm)) {
      // 收到其他任期的msg  重置entry的任期避免损坏
      SKYLU_LOG_INFO(G_LOGGER)<<"a chunk of another version of the entry received, resetting progress to avoid corruption";
      entry->term = msg->entryTerm;
      entry->bytes = 0;
      goto finish;
    }

    if (msg->offset > entry->bytes) {
      // chunk 过大 .拒绝
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "unexpectedly large offset %d for a chunk, ignoring to avoid gaps"
                          , msg->offset);
      goto finish;
    }

    update->len = msg->totalLen;
    update->data =(char *)realloc(update->data, msg->totalLen);


    memcpy(update->data + msg->offset, msg->data, msg->len);
    entry->term = msg->entryTerm;
    entry->bytes = msg->offset + msg->len;
    assert(entry->bytes <= update->len);

    entry->snapshot = msg->snapshot;

    if (entry->bytes == update->len) {
      //这里的包完整接受了
      if (msg->snapshot) {
        if (!restore(msg->previndex, entry)) {
          SKYLU_LOG_INFO(G_LOGGER)<<"restore from snapshot failed";
          goto finish;
        }
      } else {
        if (!append(msg->previndex, msg->prevterm, entry)) {
          SKYLU_LOG_DEBUG(G_LOGGER)<<"log_append failed";
          goto finish;
        }
      }
    }
  } else {
    // 心跳包
    entry->bytes = 0;
  }

  if (logLastIndex() >= 0) {
    reply.entryTerm = log(logLastIndex())->term;
  } else {
    reply.entryTerm = -1;
  }
  reply.applied = m_log.applied;

  reply.success = true;
  finish:
  reply.process.entries = logLastIndex() + 1;
  reply.process.bytes = entry->bytes;

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,
      "replying with %s to %d, our progress is %d:%d",
      reply.success ? "ok" : "reject",
      sender,
      reply.process.entries,
      reply.process.bytes
  );
  send(sender, &reply, sizeof(reply));
}
void Raft::handleDone(MsgDone *msg) {
    if (m_role != Leader) {
      return;
    }

    int sender = msg->msg.from;
    if (sender == m_me) {
      return;
    }

    auto peer = m_peers[sender];
    if (msg->msg.sequence != peer->sequence) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "[from %d] ============= mseqno(%d) != sseqno(%d)"
                          ,sender, msg->msg.sequence, peer->sequence);
      return;
    }
    peer->sequence++;
    if (msg->msg.curterm < m_term) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "[from %d] ============= msgterm(%d) != term(%d)"
                          , sender, msg->msg.curterm, m_term);
      return;
    }


    peer->applied = msg->applied;

    if (msg->success) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= done (%d, %d)",
                            sender, msg->process.entries, msg->process.bytes);
      peer->acked = msg->process;
      peer->silent_ms = 0;
    } else {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= refused", sender);
      if (peer->acked.entries > 0) {
        peer->acked.entries--;
        peer->acked.bytes = 0;
      }
    }

    if (peer->acked.entries <= logLastIndex()) {
      // 发送下一个entries
      beat(sender);
    }



  }

  void Raft::setTerm(int term) {
    assert(term > m_term);
    m_term = term;
    m_voteFor = NOBODY;
    m_votes = 0;
  }


  void Raft::handleClaim(MsgClaim *msg) {

    int candidate = msg->msg.from;

    if (msg->msg.curterm >= m_term) {
      if (m_role != Follower) {
        SKYLU_LOG_INFO(G_LOGGER)<<"there is another candidate, demoting myself";
      }
      if (msg->msg.curterm > m_term) {
        setTerm(msg->msg.curterm);
      }
      m_role = Follower;
    }

    MsgVote reply{};
    reply.msg.msgtype = kMsgVote;
    reply.msg.curterm = m_term;
    reply.msg.from = m_me;
    reply.msg.sequence = msg->msg.sequence;

    reply.granted = false;

    if (msg->msg.curterm < m_term) goto finish;

    // 检查日志是否是最新的
    if (msg->index < logLastIndex()) goto finish;
    if (msg->index == logLastIndex()) {
      if ((msg->index >= 0) && (log(msg->index)->term != msg->lastTerm)) {
        goto finish;
      }
    }

    // 当手上还有选票 或者 上一轮选举已经投给candidate 的时候 投出选票
    if ((m_voteFor == NOBODY) || (m_voteFor == candidate)) {
      m_voteFor = candidate;
      resetTimer();
      reply.granted = true;
    }
    finish:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "voting %s %d\n",
                          reply.granted ? "for" : "against", candidate);
    send(candidate, &reply, sizeof(reply));

  }

  void Raft::handleVote(MsgVote *msg) {
    auto peer = m_peers[msg->msg.from];
    if (msg->msg.sequence != peer->sequence)
    {
      // 可能出现因为网络原因过期的包
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "old msg sequence = %d, current msg sequence = %d"
                          ,msg->msg.sequence,peer->sequence);
      return;
    }

    peer->sequence++;
    if (msg->msg.curterm < m_term)
    {

      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "old term  = %d, current term = %d"
      ,msg->msg.curterm,m_term);
      return ;
    }

    if (m_role != Candidate)
    {
      return ;
    }


    if (msg->granted)
    {
      m_votes++;
    }

    becomeLeader();

  }
  void Raft::handleMessage(MsgData *msg) {

    if (msg->curterm > m_term) {
      if (m_role != Follower) {
        SKYLU_LOG_INFO(G_LOGGER)<<"I have an old term, demoting myself";
      }
      setTerm(msg->curterm);
      m_role = Follower;
    }

    assert(msg->msgtype >= 0);
    assert(msg->msgtype < 4);
    switch (msg->msgtype) {
    case kMsgUpdate:
      handleUpdate((MsgUpdate *)msg);
      break;
    case kMsgDone:
      handleDone((MsgDone*)msg);
      break;
    case kMsgClaim:
      handleClaim((MsgClaim *)msg);
      break;
    case kMsgVote:
      handleVote((MsgVote *)msg);
      break;
    default:
      SKYLU_LOG_ERROR(G_LOGGER)<<"unknown message type";
    }
  }

  void Raft::recvMessage(const UdpConnection::ptr& conne,const Address::ptr& remote,Buffer *buff) {

    auto msg = (MsgData*)buff->curRead();

    if (!msgSize(msg, buff->readableBytes())) {
      SKYLU_LOG_ERROR(G_LOGGER)<<
          "a corrupt msg recved from "<<
          remote->getAddrForString()<<
          "  Port:"<<remote->getPort()
      ;
      buff->resetAll();
      return ;

    }

    if ((msg->from < 0) || (msg->from >= m_config.peernum_max)) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the 'from' is out of range (%d)\n",
          msg->from
      );
      buff->resetAll();
      return ;
    }

    if (msg->from == m_me) {
      SKYLU_LOG_INFO(G_LOGGER)<<"the message is from myself ";
      buff->resetAll();
      return ;
    }

    auto peer =m_peers[msg->from];
    // 识别发送主机是否一致
    if (peer->host->getLocalAddress()->getAddrForString() != remote->getAddrForString()) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the message is from a wrong address %s = %d"
          " (expected from %s = %d)\n",
          peer->host->getLocalAddress()->getAddrForString().c_str(),
          peer->host->getLocalAddress()->getAddr(),
          remote->getAddrForString().c_str(),
          remote->getAddr()
      );
      buff->resetAll();
    return ;
    }
    // 端口是否一致
    if (peer->host->getLocalAddress()->getPort() != remote->getPort()) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the message is from a wrong port %d"
          " (expected from %d)\n",
          peer->host->getLocalAddress()->getPort(),
          remote->getPort()
      );
      buff->resetAll();
      return ;
    }

    //m_remote_to_peer.reset(remote.get());
   // m_conne_to_peer.reset(conne.get());

    handleMessage(msg);
    buff->resetAll();





  }
  void Raft::becomeFollower() {
    m_leaderId = NOBODY;
    m_role = Follower;
  }
  }
}

