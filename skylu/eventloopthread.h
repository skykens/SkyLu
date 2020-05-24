//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_EVENTLOOPTHREAD_H
#define HASHTEST_EVENTLOOPTHREAD_H

#include "thread.h"
#include "eventloop.h"
#include "mutex"
#include <functional>
#include <memory>
namespace skylu{
class EventLoopThread {
public:
    typedef std::function<void(Eventloop*)> ThreadInitCallback;
    typedef std::shared_ptr<EventLoopThread> ptr;

    EventLoopThread(const ThreadInitCallback & cb ,const std::string & name);

    ~EventLoopThread();

    Eventloop * startLoop();


private:
    void ThreadFunc();

private:
    Eventloop * m_loop;
    Thread m_thread;
    Mutex m_mutex;
    bool isExit;

    ThreadInitCallback m_callback;

    Condition m_cond;





};

}

#endif //HASHTEST_EVENTLOOPTHREAD_H
