//
// Created by jimlu on 2020/5/22.
//

#ifndef HASHTEST_POLL_H
#define HASHTEST_POLL_H

#include "nocopyable.h"
#include "timestamp.h"
#include <vector>
#include <map>


namespace skylu {
    class Channel;
    class EventLoop;
    class Poll : Nocopyable {
    public:
        typedef std::vector<Channel*> ChannelList;

        Poll(EventLoop *loop):ownerLoop(loop) {}
        virtual Timestamp poll(int timeoutMS,ChannelList& activeChannel) = 0;
        virtual ~Poll() = default;
        virtual  void updateChannel(Channel * channel) = 0;
        virtual void removeChannel(Channel *channel) = 0;

    protected:
        virtual void fillActiveChannels(int numEvents,ChannelList& activeChannels)const = 0;
        typedef std::map<int,Channel *> ChannelMap;
        EventLoop * ownerLoop;
        ChannelMap m_channels;






    };

}

#endif //HASHTEST_POLL_H
