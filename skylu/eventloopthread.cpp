//
// Created by jimlu on 2020/5/19.
//

#include "eventloopthread.h"
#include <assert.h>

namespace skylu{
    EventLoopThread::EventLoopThread(const ThreadInitCallback &cb, const std::string &name)
            :m_loop(NULL)
            ,m_thread(std::bind(&EventLoopThread::ThreadFunc,this),name)
            ,m_mutex()
            ,isExit(false)
            ,m_callback(cb)
            ,m_cond(m_mutex){


    }
    EventLoopThread::~EventLoopThread() {
        isExit = true;
        if(m_loop != NULL){
            m_loop->quit();
            m_thread.join();
        }
    }
    EventLoop * EventLoopThread::startLoop() {
        assert(!m_thread.isStart());
        m_thread.start();
        EventLoop *loop = NULL;
        {
            Mutex::Lock lock(m_mutex);
            while(m_loop ==NULL ){
                m_cond.wait();
            }
            loop=m_loop;
        }
        return loop;
    }

    void EventLoopThread::ThreadFunc() {
        EventLoop loop;
        if(m_callback){
            m_callback(&loop);

        }
        {
            Mutex::Lock lock(m_mutex);
            m_loop = &loop;
            m_cond.notify();

        }
        loop.loop();

        Mutex::Lock lock(m_mutex);
        m_loop = NULL;
    }

}