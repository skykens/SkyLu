#include "threadpool.h"

namespace skylu{
ThreadPool::ThreadPool(int threads,int taskCapacity,bool start)
    :isStop(false)
    ,m_tasks(taskCapacity)
{
    m_threads.resize(threads);
    if(start)
        this->start();
}

void ThreadPool::start(){
    int count  = 1;
    for(size_t i=0;i<m_threads.size();i++){
        m_threads[i].reset(new Thread([this](){
            while(!isStop){
                Task t = m_tasks.pop_wait();
                t();
            }
                
         },"ThreadPool# "+ std::to_string(count++),true));
    }

}

void ThreadPool::join(){
    isStop = true;
    for(auto it : m_threads)
        it->join();
}

bool ThreadPool::addTask(Task &&task)
{
   return m_tasks.push(move(task));
    
    
}
}
