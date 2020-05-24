//
// Created by jimlu on 2020/5/21.
//

#include "eventloopthreadpool.h"

#include <assert.h>
namespace skylu{
    EventLoopThreadPool::EventLoopThreadPool(Eventloop *base, const std::string &name)
            :m_baseLoop(base)
            ,m_isStart(false)
            ,m_name(name)
            ,m_numThreads(0)
            ,m_next(0){
    }

    void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
        assert(!m_isStart);
        assert(m_baseLoop->isInLoopThread());

        m_isStart = true;


        for(int i=0;i<m_numThreads;i++ ){
            std::string name = m_name + " ## " + std::to_string(i);
            EventLoopThread* thr = new EventLoopThread(cb,name);
            m_threads.push_back(std::unique_ptr<EventLoopThread>(thr));
            m_loops.push_back(thr->startLoop());
        }

        if(m_numThreads == 0 && cb){
            cb(m_baseLoop);
        }
    }

    Eventloop * EventLoopThreadPool::getNextLoop() {
        assert(m_baseLoop->isInLoopThread());
        assert(m_isStart);
        Eventloop *loop = m_baseLoop;
        if(!m_loops.empty()){
            loop = m_loops[m_next];
            ++m_next;
            if(m_next >= m_loops.size()){
                m_next = 0;
            }
        }
        return loop;
    }



}