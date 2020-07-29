//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_EventLoop_H
#define HASHTEST_EventLoop_H


#include "../base/nocopyable.h"
#include "../base/util.h"
#include "channel.h"
#include "timerid.h"
#include "timestamp.h"
#include "../base/mutex.h"
#include <sys/types.h>
#include <memory>

namespace skylu{

/**
 * 提前声明 ， 避免头文件的互相引用
 */
    class TimerQueue;
    class Poll;

    int createEventfd();

    /**
     * one thread one loop
     */
    class EventLoop :Nocopyable{
        typedef std::function<void()> Functor;
    public:
        EventLoop();
        ~EventLoop();
        // 阻塞于多路复用模型的监听接口上
        void loop(int timeOutMs = kPollTimeMS);
        // 判断是否处于IO线程中
        void assertInLoopThread();
        /**
         * 更新通道 （必须处在IO线程中调用）
         * @param channel
         */
        void updateChannel(Channel * channel);

        static EventLoop* getEventLoopOfCurrentThread();

        bool isInLoopThread()const{return m_threadid == getThreadId();}

        /**
         * 移除通道 （必须处在IO线程中调用 ）
         * @param channel
         */
        void removeChannel(Channel *channel);

        bool isStop(){return isQuit;}
        /**
         * 关闭  如果所在不是IO线程则唤醒eventfd
         */
        void quit();

        /**
         * 只会到时间了调用一次就删除
         * @param time 时间戳
         * @param cb
         * @return  定时器ID
         */
        Timerid runAt(const Timestamp& time,const Timer::TimerCallback& cb);
        /**
         * 只会调用一次之后就删除
         * @param delay
         * @param cb
         * @return
         */
        Timerid runAfter(double delay,const Timer::TimerCallback& cb);
        /**
         *
         * @param interval  间隔多久调用一次
         * @param cb
         * @return
         */
        Timerid runEvery(double interval,const Timer::TimerCallback& cb);

        void cancelTimer(Timerid timer);

        /**
         * 将函数的调用移动到IO线程中
         * @param cb
         */
        void runInLoop(const Functor &cb);

        void queueInLoop(const Functor &cb);

        void setPollTimeoutMS(int ms){m_poll_timeoutMs = ms;}

    private:
      /**
       * 处理的是eventfd的唤醒事件
       */
        void handleRead() const;
        void doPendingFunctors();

        /**
         * 唤醒eventfd
         */
        void wakeup() const;



    private:

        static const int kPollTimeMS;

        typedef std::vector<Channel *>ChannelList;
        bool isQuit{};
        ChannelList m_activeChannels;
        bool isLooping;
        const pid_t m_threadid;
      // 使用unique可以避免头文件引用
        std::unique_ptr<Poll> m_poll;
        std::unique_ptr<TimerQueue> m_timerqueue;
        bool isCallingPendingFunctors{};
        const int m_wakeupfd;
        std::unique_ptr<Channel> m_wakeupChannel;
        Mutex m_mutex;

        std::vector<Functor> m_pendingFunctors;
        int m_poll_timeoutMs;

    };
}


#endif //HASHTEST_EventLoop_H
