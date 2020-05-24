/*************************************************************************
	> File Name: thread.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月13日 星期一 10时50分26秒
    对线程的封装
 ************************************************************************/

#ifndef _THREAD_H
#define _THREAD_H
#include "nocopyable.h"
#include "mutex.h"
#include <memory>
namespace skylu{
/*
 * @brief 线程的封装类  包含信号
 */
class Thread :Nocopyable{
public:
    typedef std::shared_ptr<Thread>  ptr;

    /**
     * @brief 构造函数 
     * @param[in] cb: 线程的执行函数
     * @param[in] name: 线程名
     * @param[in] autoStart 是否自动开始运行
     */
    Thread(std::function<void()>cb, const std::string& name,bool autoStart = false);
    /**
     * @brief 析构函数
     * @descri : 调用detach 回收线程资源
     */
    ~Thread();
    /**
     * @brief 返回线程Id
     */
    pid_t getId()const {return m_id;}

    /**
     * 是否开启
     * @return
     */
    bool isStart()const {return m_isStart;}

    /**
     * @brief 发送信号开启线程;
     */
    void start();
    /**
     * @brief 返回线程名
     */
    const std::string& getName()const {return m_name;}
    /**
     * @brief 回收线程 
     * @descri 出错会抛出异常 且打印日志
     */
    void join();
    /**
     * @brief 获取当前所处的线程示例
     */
    static Thread* GetThis();
    /**
     * @brief 获得当前所处的线程示例名称
     */
    static const std::string & GetName();
    /**
     * @brief 修改当前所处线程示例名称
     */
    static void SetName(const std::string& name);
private:
    /**
     * @brief 初始化的时候使用的函数
     */
    static void * run(void *arg);

protected:
    pid_t m_id = -1;
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;
    Semaphore m_semaphore;
    bool m_isStart;

};
}
#endif
