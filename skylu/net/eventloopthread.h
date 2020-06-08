//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_EventLoopTHREAD_H
#define HASHTEST_EventLoopTHREAD_H

#include "../base/thread.h"
#include "eventloop.h"
#include "mutex"
#include <functional>
#include <memory>
namespace skylu{
class EventLoopThread {
public:
    typedef std::function<void(EventLoop*)> ThreadInitCallback;
    typedef std::shared_ptr<EventLoopThread> ptr;

    EventLoopThread(const ThreadInitCallback & cb ,const std::string & name);

    ~EventLoopThread();

    EventLoop * startLoop();


private:
    void ThreadFunc();

private:
    EventLoop * m_loop;
    Thread m_thread;
    Mutex m_mutex;
    bool isExit;

    ThreadInitCallback m_callback;

    Condition m_cond;





};

}

#endif //HASHTEST_EventLoopTHREAD_H
