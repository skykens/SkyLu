//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TIMERQUEUE_H
#define HASHTEST_TIMERQUEUE_H

#include "eventloop.h"
#include "timer.h"
#include "../base/nocopyable.h"
#include <sys/types.h>
#include <sys/timerfd.h>
#include "timestamp.h"
#include "timerid.h"
#include <memory>
#include <set>

namespace skylu{
    class TimerQueue {
        typedef std::pair<Timestamp,Timer *> Entry;
        typedef std::set<Entry> TimerSet;
        typedef std::pair<Timer*,int64_t > ActiveTimer;
        typedef std::set<ActiveTimer > ActiveTimerSet;
    public:
        TimerQueue(EventLoop * loop);
        ~TimerQueue();

        Timerid addTimer(const Timer::TimerCallback &cb,Timestamp when,double interval);

        /**
         * 线程安全函数  用于取消定时器
         * @param timer
         */
        void cancelTimer(Timerid timer);


    private:
      /**
       * Timerfd到期后的回调函数
       */
        void handleRead();
        /**
         * 获得到期的定时器
         * @param now  当前时间
         * @return
         * 当Timerfd 到期后 找到队列中
         */
        std::vector<Entry> getExpired(Timestamp now);
        void reset(const std::vector<Entry>& expired,Timestamp now);
        bool insert(Timer* timer);
        void cancelTimerInLoop(Timerid timer);
        void addTimerInLoop(Timer*  timer);
    private:

        EventLoop* m_loop;
        const int m_timerfd;
        Channel m_timerfdChannel;
        TimerSet m_timers;
        ActiveTimerSet m_activerTimers;
        ActiveTimerSet m_cancelTimers;
        bool isCallingExpired; //是否处在Expired函数中





    };

}


#endif //HASHTEST_TIMERQUEUE_H
