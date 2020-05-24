/*************************************************************************
	> File Name: mutex.h
	> Author:
	> Mail:
	> Created Time: 2020年04月25日 星期六 11时22分20秒
    相关锁的封装
 ************************************************************************/

#ifndef _MUTEX_H
#define _MUTEX_H
#include <memory>
#include <iostream>
#include <pthread.h>
#include <mutex>
#include <semaphore.h>
#include "nocopyable.h"
namespace skylu{

/*
 * @brief RAII局部加锁模板
 */
template<class T>
class ScopedLockImpl{
public:
    ScopedLockImpl(T& mutex):m_mutex(mutex){
        m_mutex.lock();
        m_locked = true;

    }

    void lock(){
        if(!m_locked){
            m_locked = true;
            m_mutex.lock();
        }

    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;

        }

    }
    bool islock(){
        return m_locked;
    }

     ~ScopedLockImpl(){
        m_mutex.unlock();
    }

protected:
    T& m_mutex;
    bool m_locked;

};

/*
 * @brief RAII局部加读锁模板
 */
template<class T>
class ReadScopedLockImpl{
public:
    ReadScopedLockImpl(T& mutex):m_mutex(mutex){
        m_mutex.rdlock();
        m_locked = true;

    }

    void lock(){
        if(!m_locked){
            m_locked = true;
            m_mutex.lock();
        }

    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;

        }

    }
    bool islock(){
        return m_locked;
    }

     ~ReadScopedLockImpl(){
        m_mutex.unlock();
    }

protected:
    T& m_mutex;
    bool m_locked;

};


/*
 * @brief RAII局部加写锁模板
 */
template<class T>
class WriteScopedLockImpl{
public:
    WriteScopedLockImpl(T& mutex):m_mutex(mutex){
        m_mutex.wrlock();
        m_locked = true;

    }

    void lock(){
        if(!m_locked){
            m_locked = true;
            m_mutex.wrlock();
        }

    }

    void unlock(){
        if(m_locked){
            m_mutex.unlock();
            m_locked = false;

        }

    }
    bool islock(){
        return m_locked;
    }

     ~WriteScopedLockImpl(){
        m_mutex.unlock();
    }

protected:
    T& m_mutex;
    bool m_locked;

};
class Mutex :Nocopyable {
public:
    typedef ScopedLockImpl<Mutex> Lock;
    friend class Condition;
    Mutex(){
        pthread_mutex_init(&m_mutex,0);
    }
    /*
     * @brief 销毁锁
     */
    ~Mutex(){
        pthread_mutex_destroy(&m_mutex);

    }
    /*
     * @brief 加锁
     */
    void lock(){
        pthread_mutex_lock(&m_mutex);
    }
    /*
     * @brief 释放锁
     */
    void unlock(){
        pthread_mutex_unlock(&m_mutex);
    }
private:
    pthread_mutex_t m_mutex; //锁

};
class Condition : Nocopyable{
public:
    Condition(Mutex & mutex): m_mutex(mutex){
        pthread_cond_init(&m_cond,NULL);
    }

    ~Condition(){
        pthread_cond_destroy(&m_cond);
    }

    void wait(){
        pthread_cond_wait(&m_cond,&m_mutex.m_mutex);

    }

    void notify(){
        pthread_cond_signal(&m_cond);

    }

    void notifyAll(){
        pthread_cond_broadcast(&m_cond);
    }

private:
    pthread_cond_t  m_cond;
    Mutex& m_mutex;

};

class Semaphore : Nocopyable{
public:
    /*
     * @brief  初始化
     */
    Semaphore(unsigned int count = 1,const bool shared = false){
        if(count == 0)
            count=1;
        sem_init(&m_semaphore,(int)shared,count);

    }

    /*
     * @brief 销毁信号
     */
    ~Semaphore(){
        sem_destroy(&m_semaphore);
    }
    /*
     * @brief 等待信号
     */
    void wait(){
        sem_wait(&m_semaphore);
    }
    /*
     * @brief 等待毫秒信号
     * @param[in] timeoutMS: 毫秒
     * @ret false为超时
     *

    bool timewait(int timeoutMS = -1){


    }*/



    /*
     * @brief 尝试锁定信号
     */
    bool trywait(){
      return sem_trywait(&m_semaphore)==0;

    }
    /*
     * @brief 发送信号
     */
    void notify(){
        sem_post(&m_semaphore);

    }
private:
    sem_t m_semaphore;
};

/*
 *
 * @brief 读写锁
 */


class RWMutex :Nocopyable{

public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef WriteScopedLockImpl<RWMutex> WriteLock;
    RWMutex(){
        pthread_rwlock_init(&m_mutex,0);

    }
    ~RWMutex(){
        pthread_rwlock_destroy(&m_mutex);
    }
    /*
     * @brief 加读锁
     */
    void rdlock(){
        pthread_rwlock_rdlock(&m_mutex);


    }
    /*
     * @brief 加写锁
     */
    void wrlock(){
        pthread_rwlock_wrlock(&m_mutex);
    }
    /*
     * @brief 解除锁
     */
    void unlock(){
        pthread_rwlock_unlock(&m_mutex);
    }



private:
    pthread_rwlock_t m_mutex;

    
};
}
#endif
