//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TIMERQUEUE_H
#define HASHTEST_TIMERQUEUE_H

#include "nocopyable.h"
#include "eventloop.h"
#include "timer.h"
#include <sys/types.h>
#include <sys/timerfd.h>
#include "Timestamp.h"
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
        TimerQueue(Eventloop * loop);
        ~TimerQueue();

        Timerid addTimer(const Timer::TimerCallback &cb,Timestamp when,double interval);

        /**
         * 线程安全函数  用于取消定时器
         * @param timer
         */
        void cancelTimer(Timerid timer);


    private:
        void handleRead();
        std::vector<Entry> getExpired(Timestamp now);    //获得到期定时器
        void reset(const std::vector<Entry>& expired,Timestamp now);
        bool insert(Timer* timer);
        void cancelTimerInLoop(Timerid timer);
        void addTimerInLoop(Timer*  timer);
    private:

        Eventloop* m_loop;
        const int m_timerfd;
        Channel m_timerfdChannel;
        TimerSet m_timers;;
        ActiveTimerSet m_activerTimers;
        ActiveTimerSet m_cancelTimers;
        bool isCallingExpired; //是否处在Expired函数中





    };

}


#endif //HASHTEST_TIMERQUEUE_H
