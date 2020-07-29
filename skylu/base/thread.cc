#include "thread.h"
#include "log.h"
#include "util.h"

namespace skylu{

static thread_local Thread* t_thread=nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

//static skylu::

Thread::Thread(std::function<void()> cb,const std::string & name,bool autoStart)
    :m_cb(cb)
    ,m_name(name)
    ,m_isStart(autoStart)
{
    if(name.empty())
        m_name = "UNKNOW";
    int ret = pthread_create(&m_thread,nullptr,&Thread::run, this);
    if(ret)
    {
       SKYLU_LOG_ERROR(G_LOGGER) << "pthread_create thread fail.ret ="<< ret<< " name="<<name;
    }else{
        m_semaphore.wait(); //保证在run函数相关准备完成之后构造函数才结束
    }

}

void Thread::start() {
    m_isStart = true;
    m_semaphore.notify();
}
Thread::~Thread()
{
    stop();
    if(m_thread)
    {
        pthread_detach(m_thread);
    }
}

void Thread::join()
{
    if(m_thread)
    {
        int ret = pthread_join(m_thread,nullptr);
        if(ret)
        {
 //           SKYLU_LOG_ERROR(G_LOGGER) << "pthread_join thread fail. ret= "<<ret<<" name="<<m_name;
            throw std::logic_error("pthread_join error");
       }
        m_thread=0;
    }
}

//线程函数
void * Thread::run(void *arg)
{
    Thread* thread = (Thread *)arg;
    t_thread = thread; //当前所处的线程实例
    t_thread_name = thread->m_name;
    thread->m_id = skylu::getThreadId(); //在util中调用syscall
    pthread_setname_np(pthread_self(),thread->m_name.substr(0,15).c_str()); //重命名 线程名 
    std::function<void()> cb;
    cb.swap(thread->m_cb);
    thread->m_semaphore.notify(); //线程创建完毕了，可以唤醒构造函数
    while(thread->m_isStart == false){
        thread->m_semaphore.wait();
    }
    cb();
    thread->m_isStart = false;

    return nullptr;
}

Thread* Thread::GetThis()
{
    return t_thread;
}

const std::string & Thread::GetName()
{
    return t_thread_name;

}


void Thread::SetName(const std::string & name) 
{
    if(name.empty())
        return ;
    if(t_thread)
    {
        t_thread->m_name = name;

    }
    t_thread_name = name;
}
void Thread::stop() {
  m_isStart = false;
  m_semaphore.notify();
}

}
