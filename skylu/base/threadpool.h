/*************************************************************************
	> File Name: threadpool.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 14时27分17秒
    线程池的实现
 ************************************************************************/

#ifndef _THREADPOOL_H
#define _THREADPOOL_H
#include "thread.h"
#include "mutex.h"
#include "nocopyable.h"
#include "safequeue.h"
#include <vector>
#include <string>
#include <string.h>
#include <functional>
#include <iostream>
#include "singleton.h"
namespace skylu{
class ThreadPool : Nocopyable{
public:
    typedef std::function<void()> Task;
    /*
     * @brief 初始化线程池相关参数
     * @param[in] threads: 初始化的线程数
     * @param[in] taskCapacity: 任务队列容量
     * @param[in] start :  是否开始
     */
    ThreadPool(int threads = 4,int taskCapacity = 0,bool start = true);
    /*
     * @brief 析构
     */
    ~ThreadPool() = default;
    /*
     * @brief 手动启动
     */
    void start();
    /*
     * @brief 回收线程
     */
    void join();
    /*
     * @brief 添加任务
     */
    bool addTask(Task &&task);
    bool addTask(Task &task){return addTask(Task(task));}
    /*
     * @brief 返回就绪任务数
     */
    size_t  TaskSize(){return m_tasks.size();}

private:
    bool isStop;
    SafeQueue<Task> m_tasks;
    std::vector<Thread::ptr> m_threads;
public:
    typedef  SingletonPtr<ThreadPool> ThrPoolPtr;
};
}
#endif
