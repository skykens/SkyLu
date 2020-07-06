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
#include <assert.h>

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
                : m_capacity(capacity)
                  ,m_cond(m_mutex){
        }

        /*
         * @brief 返回队列大小
         */
        size_t size() {
            Mutex::Lock lock(m_mutex);
            return m_que.size();
        }
        /*
         * @brief 返回队列是否为空
         */
        bool empty() {
            Mutex::Lock lock(m_mutex);
            return m_que.empty();
        }

        bool push(T v){
          Mutex::Lock lock(m_mutex);
          if (m_capacity && m_que.size() == m_capacity)
            return false;
          m_que.push_back(v);
          m_cond.notify();
          return true;
        }

      /*
       * @brief 添加队列成员
       * @param[in] v 右值引用
       * @ret 队列为满时false
       */
        bool push(T &&v) {
          Mutex::Lock lock(m_mutex);
          if (m_capacity && m_que.size() == m_capacity)
                return false;
            m_que.push_back(std::move(v));
            m_cond.notify();
            return true;
        }
        /*
         * @brief 取队列头
         * @param[out] v
         * @ret 队列为空时返回false
         * @descri 非阻塞
         */
        bool pop(T *v) {
          Mutex::Lock lock(m_mutex);
          if (m_que.empty())
                return false;
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
          Mutex::Lock lock(m_mutex);
          while (m_que.empty())
                m_cond.wait();
            assert(!m_que.empty());
            T v = std::move(m_que.front());
            m_que.pop_front();
            return v;

        }


    private:
        size_t m_capacity;
        std::list<T> m_que;
        Mutex m_mutex;
        Condition m_cond;

    };

}
#endif
