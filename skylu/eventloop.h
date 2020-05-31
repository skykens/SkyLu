//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_EventLoop_H
#define HASHTEST_EventLoop_H


#include "nocopyable.h"
#include "util.h"
#include "channel.h"
#include "timerid.h"
#include "timestamp.h"
#include "mutex.h"
#include <sys/types.h>
#include <memory>

namespace skylu{

    class TimerQueue;
    class Poll;
    class EventLoop :Nocopyable{
        typedef std::function<void()> Functor;
    public:
        EventLoop();
        ~EventLoop();
        void loop();
        void assertInLoopThread();
        void updateChannel(Channel * channel);
        static EventLoop* getEventLoopOfCurrentThread();
        bool isInLoopThread()const{return m_threadid == getThreadId();}
        void removeChannel(Channel *channel);
        void quit();

        Timerid runAt(const Timestamp& time,const Timer::TimerCallback& cb);
        Timerid runAfter(double delay,const Timer::TimerCallback& cb);
        Timerid runEvery(double interval,const Timer::TimerCallback& cb);

        void runInLoop(const Functor &cb);
        void queueInLoop(const Functor &cb);

    private:
        void handleRead();
        void doPendingFunctors();
        void wakeup();



    private:

        static const int kPollTimeMS;

        typedef std::vector<Channel *>ChannelList;
        bool isQuit;
        ChannelList m_activeChannels;
        bool isLooping;
        const pid_t m_threadid;
        std::unique_ptr<Poll> m_poll;
        std::unique_ptr<TimerQueue> m_timerqueue;
        bool isCallingPendingFunctors;
        const int m_wakeupfd;
        std::unique_ptr<Channel> m_wakeupChannel;
        Mutex m_mutex;

        std::vector<Functor> m_pendingFunctors;


    };
}


#endif //HASHTEST_EventLoop_H
