//
// Created by jimlu on 2020/5/22.
//

#include "epoller.h"
#include "../base/log.h"
#include "assert.h"
#include <sys/types.h>
#include <sys/epoll.h>

namespace skylu{
        Epoller::Epoller(EventLoop *loop)
        :Poll(loop)
        ,m_fd(::epoll_create1(EPOLL_CLOEXEC))
        ,m_retevent(kInitSize){
            //cloexec fork的时候在子进程自动m_fd

        }
        Epoller::~Epoller(){
            ::close(m_fd);
        }



        Timestamp Epoller::poll(int timeoutMS, ChannelList &activeChannel) {
            int nowEvents = ::epoll_wait(m_fd, reinterpret_cast<struct epoll_event *>(m_retevent.data()),
                                         static_cast<int>(m_retevent.size()),timeoutMS);
            Timestamp now = Timestamp::now();
            if(nowEvents > 0){
                fillActiveChannels(nowEvents,activeChannel);
                if(static_cast<size_t>(nowEvents) == m_retevent.size()){
                    m_retevent.resize(nowEvents * 2);
                }
            }else if(nowEvents != 0){
               if(errno != EINTR){
                   SKYLU_LOG_ERROR(G_LOGGER)<<"epoll_wait errrno ="<<errno
                            << "   strerrno= "<<strerror(errno);
               }

            }

            return now;


    }
    void Epoller::fillActiveChannels(int numEvents, ChannelList &activeChannels) const {
            assert(static_cast<size_t>(numEvents) <= m_retevent.size());
            for(int i = 0;i<numEvents;++i){
                Channel * channel = static_cast<Channel *>(m_retevent[i].data.ptr);
                int fd = channel->getFd();
                assert(m_channels.find(fd) != m_channels.end() && m_channels.at(fd) == channel);
                channel->setRetevent(m_retevent[i].events);
                activeChannels.push_back(channel);
            }
        }


        void Epoller::updateChannel(Channel *channel) {
            assert(ownerLoop->isInLoopThread());
            int index = channel->getIndex();
            if(index == kNew || index == kDel ){   //kNew是刚创建出来时候的值   kDel是当所有事件都取消后的状态，这时候并没有在m_channels中删除对应的channel
                int fd = channel->getFd();
                if(index == kNew){
                    m_channels[fd] = channel;   //map

                }
                channel->setIndex(kAdd);
                update(EPOLL_CTL_ADD,channel);
            }else{
                int fd = channel->getFd();
                assert(m_channels.find(fd) != m_channels.end());
                assert(m_channels[fd] == channel);
                assert(index == kAdd);

                if(channel->isNoneEvent()){  //没有事件的时候删除即可 .同时置index为kDel,保证如果要开启时间的话可以走到上面的分支
                    update(EPOLL_CTL_DEL,channel);
                    channel->setIndex(kDel);
                }else{
                    update(EPOLL_CTL_MOD,channel);
                }

            }
        }

        void Epoller::removeChannel(Channel *channel) {
            ownerLoop->assertInLoopThread();
            int fd = channel->getFd();

            assert(m_channels.find(fd) != m_channels.end());
            assert(m_channels[fd]  ==  channel);
            assert(channel->isNoneEvent());


            int index = channel->getIndex();

            assert(index == kAdd || index == kDel);  //不可能是刚创建出来的
            size_t n = m_channels.erase(fd);
            assert(n == 1);

            if(index == kAdd){
                update(EPOLL_CTL_DEL,channel);
            }
            channel->setIndex(kNew);  //保证下次再更新状态可以加入事件
        }

        void Epoller::update(int operation, Channel *channel) {
            struct epoll_event event ;
            bzero(&event,sizeof(struct epoll_event));
            event.events = channel->getEvent();
            event.data.ptr = channel;
            int ret = ::epoll_ctl(m_fd,operation,channel->getFd(),&event);
            if( ret < 0){
                SKYLU_LOG_ERROR(G_LOGGER) << " epoll_ctl("<<channel->getFd()<<") errno ="<<errno
                        <<"   strerror="<<strerror(errno);

            }

        }



}