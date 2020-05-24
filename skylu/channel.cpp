//
// Created by jimlu on 2020/5/19.
//

#include "channel.h"
#include "eventloop.h"
#include <assert.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include "log.h"
namespace skylu{
    const int Channel::kNoneEvent = 0;
    const int Channel::kReadEvent = POLLIN | POLLPRI;
    const int Channel::kWriteEvent = POLLOUT;


    Channel::Channel(Eventloop *loop, int fd)
            :m_loop(loop)
            ,m_fd(fd)
            ,m_events(0)
            ,m_retevents(0)
            ,m_index(-1)
            ,m_callingevent(false){
    }
    Channel::~Channel(){
        assert(!m_callingevent); //不可能在析构的时候还在处理事件
    }

    void Channel::update() {
        m_loop->updateChannel(this);
    }

    void Channel::remove() {
        m_loop->removeChannel(this);
    }

    void Channel::handleEvent(Timestamp & receiveTime) {

        m_callingevent = true;


        if(m_retevents & POLLNVAL){
            SKYLU_LOG_WARN(G_LOGGER)<< "Channel::handle_event() POLLNVAL";
        }

        if((m_retevents & POLLHUP) && !(m_retevents & POLLIN)){
            SKYLU_LOG_WARN(G_LOGGER)<<"Channel::handle_event() POLLHUP";
            if(m_closeCallback)
                m_closeCallback();
        }
        if(m_retevents & (POLLERR | POLLNVAL)){
            if(m_errorCallback)
                m_errorCallback();
        }
        if(m_retevents & ( POLLIN | POLLPRI| POLLRDHUP)){
            if(m_readCallback)
                m_readCallback(receiveTime);
        }
        if(m_retevents & POLLOUT){
            if(m_writeCallback)
                m_writeCallback();
        }

        m_callingevent = false;
    }
}