//
// Created by jimlu on 2020/5/21.
//

#ifndef HASHTEST_EventLoopTHREADPOOL_H
#define HASHTEST_EventLoopTHREADPOOL_H

#include "../base/nocopyable.h"

#include "eventloop.h"
#include "eventloopthread.h"
namespace skylu {
    class EventLoopThreadPool  :Nocopyable{
        typedef std::function<void(EventLoop *)> ThreadInitCallback;

    public:
        EventLoopThreadPool(EventLoop *base,const std::string & name);
        ~EventLoopThreadPool() = default;
        void setThreadNum(int numThreads){ m_numThreads = numThreads;}
        void start(const ThreadInitCallback& cb = ThreadInitCallback());
        EventLoop * getNextLoop();
        bool isStart()const {return m_isStart;}
        const std::string & getName()const {return m_name;}




    private:
        EventLoop * m_baseLoop;
        bool m_isStart;
        std::string m_name;
        int m_numThreads;
        size_t m_next;
        std::vector<EventLoopThread::ptr> m_threads;
        std::vector<EventLoop *> m_loops;
    };
}


#endif //HASHTEST_EventLoopTHREADPOOL_H
