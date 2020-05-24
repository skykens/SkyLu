//
// Created by jimlu on 2020/5/22.
//

#ifndef HASHTEST_DEFALUTPOLL_H
#define HASHTEST_DEFALUTPOLL_H

#include "poll.h"
#include "eventloop.h"
#include "poller.h"
#include "epoller.h"


namespace skylu {
    Poll *defaultNewPoll(Eventloop *loop) {
        SKYLU_LOG_INFO(G_LOGGER) << "Default create epoller.";
      //  Poll *ret = new Poller(loop);
        Poll *ret = new Epoller(loop);
        return ret;
    }
}


#endif //HASHTEST_DEFALUTPOLL_H
