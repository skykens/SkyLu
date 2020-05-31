//
// Created by jimlu on 2020/5/19.
//

#include "poller.h"
#include "log.h"
#include <assert.h>
#include <algorithm>
#include <functional>

namespace skylu{

    Poller::Poller(EventLoop *loop)
            :Poll(loop){

    }
    Timestamp Poller::poll(int timeoutMS, ChannelList &activeChannels) {

        int nowEvents = ::poll(m_pollfds.data(),m_pollfds.size(),timeoutMS);
        Timestamp now =Timestamp::now();
        if(nowEvents > 0){
            fillActiveChannels(nowEvents,activeChannels);
        }
        return now;
    }


    void Poller::fillActiveChannels(int numEvents, ChannelList &activeChannels) const {
        for(PollList::const_iterator pfd = m_pollfds.begin();pfd!= m_pollfds.end()&&numEvents>0;++pfd){
            if(pfd->revents > 0){
                --numEvents;
                auto ch = m_channels.find(pfd->fd);
                Channel * channel = ch->second;
                assert(ch!= m_channels.end());
                assert(channel->getFd() == pfd->fd);

                channel->setRetevent(pfd->revents);
                activeChannels.push_back(channel);
            }

        }
    }

    void Poller::updateChannel(Channel *channel) {
        assert(ownerLoop->isInLoopThread());
        if(channel->getIndex() < 0){
            assert(m_channels.find(channel->getFd()) == m_channels.end());
            struct pollfd pfd;
            pfd.fd = channel->getFd();
            pfd.events = static_cast<short>(channel->getEvent());
            pfd.revents = 0;
            m_pollfds.push_back(pfd);
            int index = static_cast<int>(m_pollfds.size())-1;
            channel->setIndex(index);
            m_channels[pfd.fd] = channel;
        }else{
            assert(m_channels.find(channel->getFd()) != m_channels.end());
            assert(m_channels[channel->getFd()] == channel);
            int index = channel->getIndex();
            assert(0 <= index && index < static_cast<int>(m_pollfds.size()));
            struct pollfd & pfd = m_pollfds[index];
            assert(pfd.fd == channel->getFd() || pfd.fd == -channel->getFd()-1);
            pfd.fd = channel->getFd();
            pfd.events = static_cast<short>(channel->getEvent());
            pfd.revents = 0;
            if(channel->isNoneEvent()){
                pfd.fd = -channel->getFd()-1; //ignore fd
            }
        }
    }

    void Poller::removeChannel(Channel* channel) {
        assert(ownerLoop->isInLoopThread());

        assert(m_channels.find(channel->getFd()) != m_channels.end());
        assert(m_channels[channel->getFd()] == channel);
        assert(channel->isNoneEvent());

        int index = channel->getIndex();
        assert(index>=0 && index< static_cast<int>(m_pollfds.size()));

        const struct pollfd& pfd = m_pollfds[index];
        assert(pfd.fd == -channel->getFd()-1 && pfd.events == channel->getEvent());
        size_t n = m_channels.erase(channel->getFd());

        assert(n == 1);

        if(static_cast<size_t>(index) == m_pollfds.size() -1){
            m_pollfds.pop_back();
        }else{
            int channelAtEnd = m_pollfds.back().fd;
            std::iter_swap(m_pollfds.begin()+index,m_pollfds.end() -1);
            if(channelAtEnd < 0){
                channelAtEnd = -channelAtEnd -1;

            }

            m_channels[channelAtEnd]->setIndex(index);
            m_pollfds.pop_back();
        }

    }





}
