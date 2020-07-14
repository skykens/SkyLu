//
// Created by jimlu on 2020/5/19.
//

#include "timerqueue.h"
#include <assert.h>
#include <iterator>
#include "../base/log.h"

namespace skylu{


    static
    int createTimerfd(){
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                   TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        SKYLU_LOG_ERROR(G_LOGGER) << "Failed in timerfd_create";
    }
    return timerfd;
    }

struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.getMicroSeconds()
                           - Timestamp::now().getMicroSeconds();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts{};
    ts.tv_sec = static_cast<time_t>(
            microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
            (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    if (n != sizeof howmany)
    {
        SKYLU_LOG_ERROR(G_LOGGER) << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue{};
    struct itimerspec oldValue{};
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        SKYLU_LOG_ERROR(G_LOGGER)<< "timerfd_settime()";
    }
}
    TimerQueue::TimerQueue(EventLoop *loop)
            :m_loop(loop)
            ,m_timerfd(createTimerfd())
            ,m_timerfdChannel(loop,m_timerfd)
            ,isCallingExpired(false){

        m_timerfdChannel.setReadCallback(std::bind(&TimerQueue::handleRead,this));
        m_timerfdChannel.enableReading();

    }
    TimerQueue::~TimerQueue() {
        m_timerfdChannel.disableAll();
        m_timerfdChannel.remove();
        ::close(m_timerfd);
        for(auto it: m_timers){
           delete it.second;
        }



    }
    bool TimerQueue::insert(Timer *timer) {
        assert(m_loop->isInLoopThread());
        bool ret = false;
        Timestamp when = timer->getExpiration();
        auto it = m_timers.begin();

        if(it == m_timers.end()||when < it->first){
            ret = true;
        }

        {
            std::pair<TimerSet::iterator ,bool> result = m_timers.insert(Entry(when,timer));
            assert(result.second);
        }
        {
            std::pair<ActiveTimerSet ::iterator ,bool> result
                    = m_activerTimers.insert(ActiveTimer(timer,timer->getSequence()));
            assert(result.second);

        }
        assert(m_timers.size() == m_activerTimers.size());
        return ret;

    }

    void TimerQueue::handleRead() {
        assert(m_loop->isInLoopThread());
        Timestamp now(Timestamp::now());
        readTimerfd(m_timerfd,now);
        std::vector<Entry> expired = getExpired(now);
        isCallingExpired = true;
        for(const Entry &it :expired){
            it.second->run();
        }
        isCallingExpired = false;

        reset(expired,now);

    }

    void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now) {
        Timestamp nextExpire;

        for(const Entry& it:expired) {
            ActiveTimer timer(it.second, it.second->getSequence());
            if (it.second->repeat() && m_cancelTimers.find(timer) == m_cancelTimers.end()) {
                it.second->restart(now);
                insert(it.second);
            }else{
                delete it.second;  /// TODO 直接删除的话这个地方可能会有风险
            }

        }
        if(!m_timers.empty()){
            nextExpire = m_timers.begin()->second->getExpiration();
        }

        if(nextExpire.isVaild()){
            resetTimerfd(m_timerfd,nextExpire);

        }
    }
    std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now) {
        assert(m_timers.size() == m_activerTimers.size());
        std::vector<Entry > expired;  /// 存放到期后的定时器
        Entry sentry = std::make_pair(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
        auto  it = m_timers.lower_bound(sentry);
        assert(it == m_timers.end() || now < it->first);

        std::copy(m_timers.begin(),it,std::back_inserter(expired));
        m_timers.erase(m_timers.begin(),it);

        for(const Entry & tmp : expired){
            ActiveTimer timer(tmp.second,tmp.second->getSequence());
            size_t n = m_activerTimers.erase(timer);
            assert(n == 1);

        }

        assert(m_timers.size() == m_activerTimers.size());
        return expired;

    }
    Timerid TimerQueue::addTimer(const Timer::TimerCallback &cb,Timestamp when, double interval) {
        auto * timer = new Timer(cb,when,interval);
        m_loop->runInLoop(std::bind(&TimerQueue::addTimerInLoop,this,timer));
        return {timer,static_cast<uint64_t>(timer->getSequence())};
    }

    void TimerQueue::cancelTimer(Timerid timer) {
        m_loop->runInLoop(std::bind(&TimerQueue::cancelTimerInLoop,this,timer));
    }

    void TimerQueue::cancelTimerInLoop(Timerid timer) {
        assert(m_loop->isInLoopThread());
        assert(m_timers.size() == m_activerTimers.size());
        ActiveTimer  active(timer.m_timer,timer.m_sequence);
        auto it = m_activerTimers.find(active);

        if(it != m_activerTimers.end()){
            /// Timer尚未被调用 可以安全删除
            size_t  n = m_timers.erase(Entry(it->first->getExpiration(),it->first));
            assert(n ==1);
            delete it->first;
            m_activerTimers.erase(it);
        }else if(isCallingExpired){
            m_cancelTimers.insert(active);
        }
    }

    void TimerQueue::addTimerInLoop(Timer *timer) {
        assert(m_loop->isInLoopThread());
        bool ret = insert(timer);
        if(ret){
            resetTimerfd(m_timerfd,timer->getExpiration());
        }
    }




}
