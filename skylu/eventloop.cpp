//
// Created by jimlu on 2020/5/19.
//

#include "eventloop.h"
#include "log.h"
#include <assert.h>
#include "poll.h"
#include "timerqueue.h"
#include "util.h"
#include "defalutPoll.h"
#include <sys/eventfd.h>

namespace skylu{
    const int Eventloop::kPollTimeMS = 5;
    thread_local Eventloop* t_loopInThread = 0;

    int createEventfd()
    {
        int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (evtfd < 0)
        {
            SKYLU_LOG_ERROR(G_LOGGER)<< "Failed in eventfd";
        }
        return evtfd;
    }
    Eventloop::Eventloop()
        :isLooping(false)
        ,m_threadid(getThreadId())
        ,m_poll(defaultNewPoll(this))
        ,m_timerqueue(new TimerQueue(this))
        ,m_wakeupfd(createEventfd())
        ,m_wakeupChannel(new Channel(this,m_wakeupfd)){

        SKYLU_LOG_DEBUG(G_LOGGER)<<"Event Loop create "<<this<<"in thread "<<m_threadid;
        if(t_loopInThread){
            SKYLU_LOG_DEBUG(G_LOGGER)<<"Anther Eventloop  "<<t_loopInThread
                        <<"  exists in this thread" <<m_threadid;
        }else{
            t_loopInThread = this;


        }
        m_wakeupChannel->setReadCallback(std::bind(&Eventloop::handleRead,this));
        m_wakeupChannel->enableReading();

    }
    Eventloop::~Eventloop() {
        assert(!isLooping);
    }
    void Eventloop::assertInLoopThread() {
        assert(isInLoopThread());;
    }

    Eventloop * Eventloop::getEventLoopOfCurrentThread() {
        return t_loopInThread;
    }

    void Eventloop::loop() {
        assert(!isLooping);
        assertInLoopThread();

        isLooping = true;
        isQuit = false;

        while(!isQuit){
            m_activeChannels.clear();
           Timestamp now =  m_poll->poll(kPollTimeMS,m_activeChannels);
            for(auto it : m_activeChannels){
                it->handleEvent(now);
            }
            doPendingFunctors();
        }



        SKYLU_LOG_DEBUG(G_LOGGER)<< "EventLoop "<<this<<"  is stop.";
        isLooping = false;

    }
    void Eventloop::doPendingFunctors() {
        std::vector<Functor > functors;
        isCallingPendingFunctors = true;
        {
            Mutex::Lock lock(m_mutex);
            functors.swap(m_pendingFunctors);
        }

        for(auto it : functors){
            it();
        }

        isCallingPendingFunctors = false;
    }
    void Eventloop::updateChannel(Channel *channel) {
        assert(channel->ownerLoop() == this);
        assertInLoopThread();
        m_poll->updateChannel(channel);
    }


    Timerid Eventloop::runAt(const Timestamp &time, const Timer::TimerCallback &cb) {

        return m_timerqueue->addTimer(cb,time,0);
    }

    Timerid Eventloop::runAfter(double delay, const Timer::TimerCallback &cb) {
        Timestamp time(addSecond(Timestamp::now(),delay));
        return runAt(time,cb);
    }
    Timerid Eventloop::runEvery(double interval, const Timer::TimerCallback &cb) {
        Timestamp time(addSecond(Timestamp::now(),interval));
        return m_timerqueue->addTimer(cb,time,interval);
    }


    void Eventloop::runInLoop(const Functor &cb) {
        if(isInLoopThread()){
            cb();

        }else{
            queueInLoop(cb);
        }

    }

    void Eventloop::queueInLoop(const Functor &cb) {
        {
            Mutex::Lock lock(m_mutex);
            m_pendingFunctors.push_back(std::move(cb));
        }

        if(!isInLoopThread() || isCallingPendingFunctors){
            wakeup();
        }

    }

    void Eventloop::wakeup() {
        uint64_t  one = 1;
        ssize_t ret = ::write(m_wakeupfd,&one,sizeof(one));
        if(ret != sizeof(one)){
            SKYLU_LOG_ERROR(G_LOGGER)<< "EventLoop::wakeup() writes " << ret<< " bytes instead of 8";
        }
    }

    void Eventloop::handleRead() {
        uint64_t  one = 1;
        ssize_t ret = ::read(m_wakeupfd,&one,sizeof(one));
        if(ret != sizeof(one)){

            SKYLU_LOG_ERROR(G_LOGGER)<< "EventLoop::wakeup() reads " << ret<< " bytes instead of 8";
        }
    }

    void Eventloop::quit() {
        isQuit = true;
        if(!isInLoopThread()){
            wakeup();
        }
    }

    void Eventloop::removeChannel(Channel * channel) {
        assert(channel->ownerLoop() == this);
        assert(isInLoopThread());
        m_poll->removeChannel(channel);

    }
}