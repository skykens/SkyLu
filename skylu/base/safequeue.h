/*************************************************************************
	> File Name: safequeue.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 11时34分08秒
    线程安全队列
 ************************************************************************/

#ifndef _SAFEQUEUE_H
#define _SAFEQUEUE_H
#include "mutex.h"
#include <atomic>
#include <functional>
#include <list>

#include "nocopyable.h"
namespace skylu {

    template<class T>
    class SafeQueue : Nocopyable {

    public:
        /*
         * @brief 构造函数
         * @param[in] capacity 0:无限制大小
         */
        SafeQueue(size_t capacity = 0)
                : m_capacity(capacity) {
        }

        /*
         * @brief 返回队列大小
         */
        size_t size() {
            RWMutex::ReadLock lock(m_mutex);
            return m_que.size();
        }
        /*
         * @brief 返回队列是否为空
         */
        bool empty() {
            RWMutex::ReadLock lock(m_mutex);
            return m_que.empty();
        }

        /*
         * @brief 添加队列成员
         * @param[in] v 右值引用
         * @ret 队列为满时false
         */
        bool push(T &&v) {
            if (m_capacity && size() == m_capacity)
                return false;
            RWMutex::WriteLock lock(m_mutex);
            m_que.push_back(std::move(v));
            m_semaphore.notify();
            return true;
        }
        /*
         * @brief 取队列头
         * @param[out] v
         * @ret 队列为空时返回false
         * @descri 非阻塞
         */
        bool pop(T *v) {
            if (empty())
                return false;
            RWMutex::WriteLock lock(m_mutex);
            *v = std::move(m_que.front());
            m_que.pop_front();
            return true;
        }


        /*
         * @brief 取队列头
         * @ret T
         * @descri 阻塞
         */

        T pop_wait() {
            while (empty())
                m_semaphore.wait();
            RWMutex::WriteLock lock(m_mutex);
            T v = std::move(m_que.front());
            m_que.pop_front();
            return v;

        }


    private:
        size_t m_capacity;
        std::list<T> m_que;
        RWMutex m_mutex;
        Semaphore m_semaphore;

    };

}
#endif
