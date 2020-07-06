//
// Created by jimlu on 2020/6/11.
//

#include "raft.h"
#include <arpa/inet.h>
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

  if(sizeof(MsgUpdate)+config.chunk_len-1>UDP_SAFE_SIZE){
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
                        "please ensure chunk_len <= %lu, %d is too much for UDP\n",
                        UDP_SAFE_SIZE - sizeof(MsgUpdate) + 1,
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
Raft::Raft(const skylu::raft::Config &config)
      :m_term(0)
      ,m_voteFor(NOBODY)
      ,m_role(Follower)
      ,m_me(NOBODY)
      ,m_votes(0)
      ,m_leaderId(NOBODY)
      ,m_sockFd(-1)
      ,m_timer(0)
      ,m_config(config)
      ,m_log(config.log_len)
      ,m_peerNum(0)
      ,m_peers(config.peernum_max){
  assert(m_peers.size() == m_config.peernum_max);
  for(int i = 0 ;i<m_peers.size();i++){
    m_peers[i] = new Peer();


  }

}
bool Raft::peerUp(int id, std::string host, int port, bool self) {

  Peer  *p =  m_peers[id];
  struct addrinfo hint;  //域名
  struct addrinfo *a = nullptr;

  if(m_peerNum >= m_config.peernum_max){
    SKYLU_LOG_ERROR(G_LOGGER)<<"too many peers";
    return false;

  }

  p->up = true;
  p->host = host;
  p->port = port;

  bzero(&hint,sizeof(hint));
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_family = AF_INET;
  hint.ai_protocol = getprotobyname("udp")->p_proto;

  if(getaddrinfo(host.c_str(),std::to_string(port).c_str(),&hint,&a)){
    SKYLU_LOG_ERROR(G_LOGGER)<<"cannot convert the host string "<<host
                  <<"to a valid address: %s"<<gai_strerror(errno);
    return false;
  }

  assert(a != NULL && a->ai_addrlen <= sizeof(p->addr));
  memcpy(&p->addr,a->ai_addr,a->ai_addrlen);

  if(self){
    if(m_me != NOBODY){
      SKYLU_LOG_ERROR(G_LOGGER)<<"cannot set  'self ' peer multiple times ";
    }
    m_me = id;
    srand(id);
    resetTimer();
  }

  m_peerNum++;
  return true;


}


void Raft::peerDown(int id) {
  Peer * p  = m_peers[id];
  p->up = false;
  if(m_me == id){
    m_me = NOBODY;
  }
  --m_peerNum;
}



void Raft::resetTimer() {
  if(m_role == Leader){
    m_timer = m_config.heartbeat_ms;

  }else{
    /// 候选者

    m_timer = randBetweenTime(m_config.election_ms_min,m_config.election_ms_max);
  }
}
int Raft::progress() {
  return m_log.applied;
}
bool Raft::applied(int id, int index) {
  if(m_me == id){
    return m_log.applied > index;

  }else{
    Peer * p = m_peers[id];
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


void Raft::setRecvTimeout(int ms) {

  struct timeval tv;
  tv.tv_sec = ms / 1000;
  tv.tv_usec = ((ms % 1000) * 1000);
  if (setsockopt(m_sockFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"failed to set socket recv timeout: %s\n", strerror(errno));
  }
}

void Raft::setReuseaddr() {

  int optval = 1;
  if (setsockopt(
      m_sockFd, SOL_SOCKET, SO_REUSEADDR,
      (char const*)&optval, sizeof(optval)
  ) == -1) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,"failed to set socket to reuseaddr: %s\n", strerror(errno));
  }
}


int Raft::createUdpSocket() {
  assert(m_me != NOBODY);
  Peer  *me = m_peers[m_me];
  struct addrinfo hint;
  struct addrinfo *addrs = NULL;
  struct addrinfo *a;
  char portstr[6];
  int rc;
  memset(&hint, 0, sizeof(hint));
  hint.ai_socktype = SOCK_DGRAM;
  hint.ai_family = AF_INET;
  hint.ai_protocol = getprotobyname("udp")->p_proto;

  snprintf(portstr, 6, "%d", me->port);

  if ((rc = getaddrinfo(me->host.c_str(), portstr, &hint, &addrs)) != 0)
  {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
        "cannot convert the host string '%s'"
        " to a valid address: %s\n", me->host.c_str(), gai_strerror(rc));
    return -1;
  }

  for (a = addrs; a != NULL; a = a->ai_next)
  {
    m_sockFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_sockFd < 0) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,"cannot create socket: %s\n", strerror(errno));
      continue;
    }
    setReuseaddr();
    setRecvTimeout(m_config.heartbeat_ms);

    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"binding udp %s:%d\n", me->host.c_str(), me->port);
    if (bind(m_sockFd, a->ai_addr, a->ai_addrlen) < 0) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,"cannot bind the socket: %s\n", strerror(errno));
      close(m_sockFd);
      continue;
    }
    assert(a->ai_addrlen <= sizeof(me->addr));
    memcpy(&me->addr, a->ai_addr, a->ai_addrlen);
    return m_sockFd;
  }

  SKYLU_LOG_FMT_ERROR(G_LOGGER,"cannot resolve the host string '%s' to a valid address\n",
        me->host.c_str()
  );
  return -1;
}


bool Raft::msgSize(MsgData *m, int mlen) {
  switch (m->msgtype) {
  case kMsgUpdate:
    return mlen == sizeof(MsgUpdate) + ((MsgUpdate*)m)->len -1;
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

  Peer *peer = m_peers[dst];


  int sent = sendto(
      m_sockFd, data, len, 0,
      (struct sockaddr*)&peer->addr, sizeof(peer->addr)
  );
  if (sent == -1) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
        "failed to send a msg to [%d]: %s\n",
        dst, strerror(errno)
    );
  }
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

  assert(m_role == Leader);
  assert(m_leaderId == m_me);

  Peer * p = m_peers[dst];

  MsgUpdate *m = (MsgUpdate*)malloc(sizeof(MsgUpdate)+m_config.chunk_len - 1);




  if (p->acked.entries <= logLastIndex()) {
    int sendindex;

    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"%d has acked %d:%d\n", dst, p->acked.entries, p->acked.bytes);

    if (p->acked.entries < logFirstIndex()) {
      // The peer has woken up from anabiosis. Send the first
      // log entry (which is usually a snapshot).
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending the snapshot to %d\n", dst);
      sendindex = logFirstIndex();
      assert(log(sendindex)->snapshot);
    } else {
      // The peer is a bit behind. Send an update.
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending update %d snapshot to %d\n", p->acked.entries, dst);
      sendindex = p->acked.entries;
    }
    m->snapshot = log(sendindex)->snapshot;
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"will send index %d to %d\n", sendindex, dst);

    m->previndex = sendindex - 1;
    Entry *e = log(sendindex);

    if (m->previndex >= 0) {
      m->prevterm = log(m->previndex)->term;
    } else {
      m->prevterm = -1;
    }
    m->entryTerm = e->term;
    m->totalLen = e->update.len;
    m->empty = false;
    m->offset = p->acked.bytes;
    m->len = std::min(m_config.chunk_len, m->totalLen - m->offset);
    assert(m->len > 0);
    memcpy(m->data, e->update.data + m->offset, m->len);
  } else {
    // The peer is up to date. Send an empty heartbeat.
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending empty heartbeat to %d\n", dst);
    m->empty = true;
    m->len = 0;
  }
  m->acked = m_log.acked;

  p->sequence++;
  m->msg.sequence = p->sequence;
  if (!m->empty) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
        "sending seqno=%d to %d: offset=%d size=%d total=%d term=%d snapshot=%s\n",
        m->msg.sequence, dst, m->offset, m->len, m->totalLen, m->entryTerm, m->snapshot ? "true" : "false"
    );
  } else {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"sending seqno=%d to %d: heartbeat\n", m->msg.sequence, dst);
  }

  send(dst, m, sizeof(MsgUpdate) + m->len - 1);
  free(m);


}

void Raft::resetBytesAcked() {
  for(int i=0;i<m_config.peernum_max;i++){
    m_peers[i]->acked.bytes = 0;

  }
}

void Raft::resetSilentTime(int id) {
  for(int i=0;i<m_config.peernum_max;i++){
    if((i == id)||(id == NOBODY)){
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
    m_peers[i]->silent_ms +=ms;
    if(m_peers[i]->silent_ms < m_config.election_ms_max){
      recent_peers++;
    }
  }

  return recent_peers;
}


bool Raft::becomeLeader() {
  if(m_votes *2 > m_peerNum){
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

  MsgClaim m;
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
    Peer * p= m_peers[i];
    p->sequence++;
    m.msg.sequence = p->sequence;

    send(i,&m,sizeof(m));
  }
}

void Raft::refreshAcked() {

  // TODO: count 'acked' inside the entry itself to remove the nested loop here
  int i, j;
  for (i = 0; i < m_config.peernum_max; i++) {
    Peer *p = m_peers[i];
    if (i == m_me) continue;
    if (!p->up) continue;

    int newacked = p->acked.entries;
    if (newacked <= m_log.acked) continue;

    int replication = 1; // count self as yes
    for (j = 0; j < m_config.peernum_max; j++) {
      if (j == m_me) continue;

      Peer *pp = m_peers[j];
      if (pp->acked.entries >= newacked) {
        replication++;
      }
    }

    assert(replication <= m_peerNum);

    if (replication * 2 > m_peerNum) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"===== GLOBAL PROGRESS: %d\n", newacked);
      m_log.acked = newacked;
    }
  }

  // Try to apply all entries which have been replicated on a majority.
  int applied = apply();
  if (applied) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"applied %d updates\n", applied);
  }
}


void Raft::tick(int msec) {

  m_timer -= msec;
  if (m_timer < 0) {
    switch (m_role) {
    case Follower:
      SKYLU_LOG_INFO(G_LOGGER)<<
          "lost the leader,"
          " claiming leadership\n"
      ;
      m_leaderId = NOBODY;
      m_role = Candidate;
      m_term++;
      claim();
      break;
    case Candidate:
      SKYLU_LOG_INFO(G_LOGGER)<<
          "the vote failed,"
          " claiming leadership\n"
      ;
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

  int recent_peers = increaseSilentTime( msec);
  if ((m_role == Leader) && (recent_peers * 2 <= m_peerNum)) {
    SKYLU_LOG_INFO(G_LOGGER)<<"lost quorum, demoting";
    m_leaderId = NOBODY;
    m_role = Follower;
  }
}

int Raft::compact() {

  Log *l = &m_log;
  int i;
  int compacted = 0;
  for (i = l->first; i < l->applied; i++) {
    Entry *e = log(i);

    e->snapshot = false;
    free(e->update.data);
    e->update.len = 0;
    e->update.data = NULL;

    compacted++;
  }
  if (compacted) {
    l->first += compacted - 1;
    l->size -= compacted - 1;
    Entry*e = log(logFirstIndex());
    e->update = m_config.snapshooter(m_config.userdata);
    e->bytes = e->update.len;
    e->snapshot = true;
    assert(l->first == l->applied - 1);

    // reset bytes progress of peers that were receiving the compacted entries
    for (i = 0; i < m_config.peernum_max; i++) {
      Peer *p = m_peers[i];
      if (!p->up) continue;
      if (i == m_me) continue;
      if (p->acked.entries + 1 <= l->first)
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
    free(victim->update.data);
    victim->update.len = 0;
    victim->update.data = nullptr;
  }
  int index = previndex + 1;
  m_log.first = index;
  m_log.size = 1;
  Entry * newEntry =  log(index);
  *newEntry =  *e;
  m_config.applier(m_config.userdata,log(index)->update,true /*snapshot*/);
  m_log.applied = index + 1;






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
  Entry * e = log(newindex);
  e->term = m_term;
  assert(e->update.len == 0);
  assert(e->update.data == nullptr);
  e->update.len = update.len;
  e->bytes = update.len;
  e->update.data =(char *)malloc(update.len);
  memcpy(e->update.data,update.data,update.len);
  m_log.size++;

  beat(NOBODY);
  resetTimer();
  return newindex;

}
bool Raft::appendable(int previndex, int prevterm) {
  int low=0,high=0;
  low = logFirstIndex();
  if(low == 0) low = -1;
  high = logLastIndex();
  if(!inrange(low,previndex,high)){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                        "previndex %d is outside log range %d-%d\n",
                        previndex,low,high);
    return false;
  }

  if(previndex != -1){
    Entry * entry = log(previndex);
    if(entry->term != prevterm){
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                          "Log term %d != prevterm %d",entry->term,prevterm);
    }
  }

  return true;
}
bool Raft::append(int previndex, int prevterm, Entry *e) {

  assert(e->bytes == e->update.len);
  assert(!e->snapshot);

  Log *l = &m_log;

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,
      "log_append(%p, previndex=%d, prevterm=%d,"
      " term=%d)\n",
      (void *)l, previndex, prevterm,
      e->term
  );

  if (!appendable(previndex, prevterm)) return false;

  if (previndex == logLastIndex()) {
    SKYLU_LOG_DEBUG(G_LOGGER)<<"previndex == last\n";
    // appending to the end
    // check if the log can accomodate
    if (l->size == m_config.log_len) {
      SKYLU_LOG_DEBUG(G_LOGGER)<<"log is full";
      int compacted = compact();
      if (compacted) {
        SKYLU_LOG_FMT_DEBUG(G_LOGGER,"compacted %d entries\n", compacted);
      } else {
        return false;
      }
    }
  }

  int index = previndex + 1;
  Entry *slot = log(index);

  if (index <= logLastIndex()) {
    // replacing an existing entry
    if (slot->term != e->term) {
      // entry conflict, remove the entry and all that follow
      l->size = index - l->first;
    }
    assert(slot->update.data);
    free(slot->update.data);
  }

  if (index > logLastIndex()) {
    // increase log size if actually appended
    l->size++;
  }
  *slot = *e;
  // TODO raft_entry_init(e);

  return true;
}
void Raft::handleUpdate(MsgUpdate *msg) {

  int sender = msg->msg.from;

  MsgDone reply;
  reply.msg.msgtype = kMsgDone;
  reply.msg.curterm = m_term;
  reply.msg.from = m_me;
  reply.msg.sequence = msg->msg.sequence;

  Entry *e = &m_log.newentry;
  Update *u = &e->update;

  // The entry should either be an empty heartbeat, or be appendable, or be a snapshot.
  if (!msg->empty && !msg->snapshot && !appendable( msg->previndex, msg->prevterm)) goto finish;

  if (logLastIndex() >= 0) {
    reply.entryTerm = log(logLastIndex())->term;
  } else {
    reply.entryTerm = -1;
  }
  reply.success = false;

  // the message is too old
  if (msg->msg.curterm < m_term) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"refuse old message %d < %d\n", msg->msg.curterm, m_term);
    goto finish;
  }

  if (sender !=m_leaderId) {
    SKYLU_LOG_FMT_INFO(G_LOGGER,"changing leader to %d\n", sender);
    m_leaderId = sender;
  }

  m_peers[sender]->silent_ms = 0;
  resetTimer();

  // Update the global progress sent by the leader.
  if (msg->acked > m_log.acked) {
    m_log.acked = std::min(
        m_log.first + m_log.size,
        msg->acked
    );
    Peer *p = m_peers[sender];
    p->acked.entries = m_log.acked;
    p->acked.bytes = 0;
  }

  if (!msg->empty) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
        "got a chunk seqno=%d from %d: offset=%d size=%d total=%d term=%d snapshot=%s\n",
        msg->msg.sequence, sender, msg->offset, msg->len, msg->totalLen, msg->entryTerm, msg->snapshot ? "true" : "false"
    );

    if ((msg->offset > 0) && (e->term != msg->entryTerm)) {
      SKYLU_LOG_INFO(G_LOGGER)<<"a chunk of another version of the entry received, resetting progress to avoid corruption";
      e->term = msg->entryTerm;
      e->bytes = 0;
      goto finish;
    }

    if (msg->offset > e->bytes) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"unexpectedly large offset %d for a chunk, ignoring to avoid gaps\n", msg->offset);
      goto finish;
    }

    u->len = msg->totalLen;
    u->data =(char *)realloc(u->data, msg->totalLen);


    memcpy(u->data + msg->offset, msg->data, msg->len);
    e->term = msg->entryTerm;
    e->bytes = msg->offset + msg->len;
    assert(e->bytes <= u->len);

    e->snapshot = msg->snapshot;

    if (e->bytes == u->len) {
      // The entry has been fully received.
      if (msg->snapshot) {
        if (!restore(msg->previndex, e)) {
          SKYLU_LOG_INFO(G_LOGGER)<<"restore from snapshot failed";
          goto finish;
        }
      } else {
        if (!append(msg->previndex, msg->prevterm, e)) {
          SKYLU_LOG_DEBUG(G_LOGGER)<<"log_append failed";
          goto finish;
        }
      }
    }
  } else {
    // just a heartbeat
    e->bytes = 0;
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
  reply.process.bytes = e->bytes;

  SKYLU_LOG_FMT_DEBUG(G_LOGGER,
      "replying with %s to %d, our progress is %d:%d\n",
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

    Peer *peer = m_peers[sender];
    if (msg->msg.sequence != peer->sequence) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= mseqno(%d) != sseqno(%d)\n",
                          sender, msg->msg.sequence, peer->sequence);
      return;
    }
    peer->sequence++;
    if (msg->msg.curterm < m_term) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= msgterm(%d) != term(%d)\n", sender, msg->msg.curterm, m_term);
      return;
    }

    // The message is expected and actual.

    peer->applied = msg->applied;

    if (msg->success) {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= done (%d, %d)\n",
                            sender, msg->process.entries, msg->process.bytes);
      peer->acked = msg->process;
      peer->silent_ms = 0;
    } else {
      SKYLU_LOG_FMT_DEBUG(G_LOGGER,"[from %d] ============= refused\n", sender);
      if (peer->acked.entries > 0) {
        peer->acked.entries--;
        peer->acked.bytes = 0;
      }
    }

    if (peer->acked.entries <= logLastIndex()) {
      // send the next entry
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

    MsgVote reply;
    reply.msg.msgtype = kMsgVote;
    reply.msg.curterm = m_term;
    reply.msg.from = m_me;
    reply.msg.sequence = msg->msg.sequence;

    reply.granted = false;

    if (msg->msg.curterm < m_term) goto finish;

    // check if the candidate's log is up to date
    if (msg->index < logLastIndex()) goto finish;
    if (msg->index == logLastIndex()) {
      if ((msg->index >= 0) && (log(msg->index)->term != msg->lastTerm)) {
        goto finish;
      }
    }

    // Grant the vote if we haven't voted in the current term, or if we
    // have voted for the same candidate.
    if ((m_voteFor == NOBODY) || (m_voteFor == candidate)) {
      m_voteFor = candidate;
      resetTimer();
      reply.granted = true;
    }
    finish:
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"voting %s %d\n", reply.granted ? "for" : "against", candidate);
    send(candidate, &reply, sizeof(reply));

  }

  void Raft::handleVote(MsgVote *msg) {
    int sender = msg->msg.from;
    Peer *peer = m_peers[sender];
    if (msg->msg.sequence != peer->sequence) return;
    peer->sequence++;
    if (msg->msg.curterm < m_term) return;

    if (m_role != Candidate) return;

    if (msg->granted) {
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
      SKYLU_LOG_ERROR(G_LOGGER)<<"unknown message type\n";
    }
  }

  MsgData *Raft::recvMessage() {
    struct sockaddr_in addr;
    unsigned int addrlen = sizeof(addr);
    static char buf[UDP_SAFE_SIZE];

    //try to receive some data
    MsgData* m = (MsgData*)buf;
    int recved = recvfrom(
        m_sockFd, buf, sizeof(buf), 0,
        (struct sockaddr*)&addr, &addrlen
    );

    if (recved <= 0) {
      if (
          (errno == EAGAIN) ||
          (errno == EWOULDBLOCK) ||
          (errno == EINTR)
          ) {
        return NULL;
      } else {
        SKYLU_LOG_ERROR(G_LOGGER)<<"failed to recv: "<<strerror(errno);
        return NULL;
      }
    }

    if (!msgSize(m, recved)) {
      SKYLU_LOG_ERROR(G_LOGGER)<<
          "a corrupt msg recved from %s:%d\n"<<
          inet_ntoa(addr.sin_addr)<<
          ntohs(addr.sin_port)
      ;
      return NULL;
    }

    if ((m->from < 0) || (m->from >= m_config.peernum_max)) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the 'from' is out of range (%d)\n",
          m->from
      );
      return NULL;
    }

    if (m->from == m_me) {
      SKYLU_LOG_INFO(G_LOGGER)<<"the message is from myself O_o";
      return NULL;
    }

    Peer *peer =m_peers[m->from];
    if (memcmp(&peer->addr.sin_addr, &addr.sin_addr, sizeof(struct in_addr))) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the message is from a wrong address %s = %d"
          " (expected from %s = %d)\n",
          inet_ntoa(peer->addr.sin_addr),
          peer->addr.sin_addr.s_addr,
          inet_ntoa(addr.sin_addr),
          addr.sin_addr.s_addr
      );
    }

    if (peer->addr.sin_port != addr.sin_port) {
      SKYLU_LOG_FMT_ERROR(G_LOGGER,
          "the message is from a wrong port %d"
          " (expected from %d)\n",
          ntohs(peer->addr.sin_port),
          ntohs(addr.sin_port)
      );
    }

    return m;
  }
  bool Raft::isLeader() { return m_role == Leader; }
  int Raft::getLeader() { return m_leaderId; }

  }
}

