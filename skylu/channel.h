//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_CHANNEL_H
#define HASHTEST_CHANNEL_H

#include "nocopyable.h"
#include "Timestamp.h"
#include <functional>
#include <sys/types.h>
#include <memory>
namespace skylu{
    class Eventloop;
    class Channel: Nocopyable {
    public:
        typedef std::shared_ptr<Channel> ptr;
        typedef std::function<void()> EventCallback;
        typedef std::function<void(Timestamp &)> ReadEventCallback;
        Channel(Eventloop * loop,int fd);
        ~Channel();
        void handleEvent(Timestamp & receiveTime);
        void setReadCallback(const ReadEventCallback & cb){m_readCallback = cb;}
        void setWriteCallback(const EventCallback& cb){m_writeCallback = cb;}
        void setErrorCallback(const EventCallback& cb){m_errorCallback = cb;}
        void setCloseCallback(const EventCallback& cb){m_closeCallback = cb;}

        int getFd()const {return m_fd;}
        int getEvent()const {return m_events;}
        int getRetevent()const {return m_retevents;}
        void setRetevent(int retevent){m_retevents = retevent;}
        bool isNoneEvent()const {return m_events == kNoneEvent;}

        void enableReading() {m_events |= kReadEvent;update();}
        void enableWriting() {m_events |= kWriteEvent;update();}
        void disableWriting() {m_events &= ~kWriteEvent; update();}
        void disableAll(){m_events = kNoneEvent;update();}
        bool isWriting() const {return m_events & kWriteEvent;}

        void remove();



        int getIndex()const{return m_index;}
        void setIndex(int index){m_index = index;}

        Eventloop *ownerLoop(){return m_loop;}

    private:
        void update();
    private:
        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        Eventloop* m_loop;
        const int m_fd;
        int m_events;
        int m_retevents;
        int m_index;
        bool m_callingevent;
        ReadEventCallback m_readCallback;
        EventCallback m_writeCallback;
        EventCallback m_errorCallback;
        EventCallback m_closeCallback;


    };

}



#endif //HASHTEST_CHANNEL_H
