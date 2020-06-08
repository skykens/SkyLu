//
// Created by jimlu on 2020/5/22.
//

#ifndef HASHTEST_EPOLLER_H
#define HASHTEST_EPOLLER_H

#include "poll.h"
#include "eventloop.h"
#include "../base/nocopyable.h"
#include <sys/epoll.h>

namespace skylu{

    class Epoller :public Poll{
    public:
        enum  MODE{
            LT,
            ET = EPOLLET
        };
        enum {
            kNew = -1,  //在channel 初始化就是-1 为了和poller兼容
            kAdd = 0,  //对应kNoneEvent
            kDel
        };
        Epoller(EventLoop * loop);
        ~Epoller();
        Timestamp poll(int timeoutMS, ChannelList &activeChannel) override;
        void removeChannel(Channel *channel) override;
        void updateChannel(Channel *channel) override;
    private:
        void fillActiveChannels(int numEvents, ChannelList &activeChannels) const override;
        void update(int operation,Channel * channel);


    private:
        static const int kInitSize = 32;
        typedef std::vector<struct epoll_event> EpollList;
        int m_fd;
        EpollList m_retevent;


    };

}


#endif //HASHTEST_EPOLLER_H
