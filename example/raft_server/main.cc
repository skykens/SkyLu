//
// Created by jimlu on 2020/6/11.
//

#include "proto.h"
#include "raft.h"
#include "raftserver.h"
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <jansson.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <skylu/base/daemon.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void applier(void *state, Update update, bool snapshot) {
  json_error_t error;

  json_t *patch = json_loadb(update.data, update.len, 0, &error);
  if (!patch) {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
                        "error parsing json at position %d: %s\n",
                        error.column,
                        error.text
    );
  }

  if (snapshot) {
    json_object_clear(static_cast<json_t *>(state));
  }

  if (json_object_update(static_cast<json_t *>(state), patch)) {
    SKYLU_LOG_ERROR(G_LOGGER)<<"error updating state\n";
  }

  json_decref(patch);

  char *encoded = json_dumps(static_cast<json_t *>(state), JSON_INDENT(4) | JSON_SORT_KEYS);
  if (encoded) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                        "applied %s: the new state is %s\n",
                        snapshot ? "a snapshot" : "an update",
                        encoded
    );
  } else {
    SKYLU_LOG_FMT_ERROR(G_LOGGER,
                        "applied %s, but the new state could not be encoded\n",
                        snapshot ? "a snapshot" : "an update"
    );
  }
  assert(encoded);
  free(encoded);
}

static Update snapshooter(void *state) {
  Update shot{};
  shot.data = json_dumps(static_cast<json_t *>(state), JSON_SORT_KEYS);
  shot.len = strlen(shot.data);
  if (shot.data) {
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,
                        "snapshot taken: %.*s", shot.len, shot.data);
  } else {
    SKYLU_LOG_ERROR(G_LOGGER)<<
                             "failed to take a snapshot";
  }

  return shot;
}
using namespace skylu;
EventLoop * g_loop = new EventLoop();
int main(int argc,char ** argv){
  Signal::hook(SIGPIPE,[](){});

  skylu::raft::RaftServer  server(g_loop,"memcached server",argc,argv);
  server.setApplier(applier);
  server.setSnapshooter(snapshooter);

  server.init();

  server.run();

  return 0;

}

