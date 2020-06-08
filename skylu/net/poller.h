//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_POLLER_H
#define HASHTEST_POLLER_H

#include <vector>
#include <map>
#include <sys/poll.h>
#include "eventloop.h"
#include "timestamp.h"
#include "../base/nocopyable.h"
#include "../base/log.h"
#include "poll.h"
struct pollfd;

namespace skylu{

    class Poller :public Poll{
    public:

        Poller(EventLoop * loop);
        ~Poller() = default;
       virtual Timestamp poll(int timeoutMS,ChannelList& activeChannels);

       virtual void updateChannel(Channel* channel);


       virtual void removeChannel(Channel* channel);


    private:
        void fillActiveChannels(int numEvents,ChannelList& activeChannels)const;

    private:
        typedef std::vector<struct pollfd> PollList;
        PollList m_pollfds;

    };

}



#endif //HASHTEST_POLLER_H
