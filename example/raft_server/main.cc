//
// Created by jimlu on 2020/6/11.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include <csignal>
#include <cerrno>
#include <sys/wait.h>
#include <ctime>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "raft.h"
#include "proto.h"
#include "raftserver.h"
#include <jansson>

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
    json_object_clear(state);
  }

  if (json_object_update(state, patch)) {
    SKYLU_LOG_ERROR(G_LOGGER)<<"error updating state\n";
  }

  json_decref(patch);

  char *encoded = json_dumps(state, JSON_INDENT(4) | JSON_SORT_KEYS);
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
  free(encoded);
}

static Update snapshooter(void *state) {
  Update shot{};
  shot.data = json_dumps(state, JSON_SORT_KEYS);
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


int main(int argc,char **argv)
{



}
