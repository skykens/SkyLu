//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TIMER_H
#define HASHTEST_TIMER_H


#include <sys/types.h>
#include <time.h>
#include <functional>
#include <atomic>
#include "timestamp.h"

#include "nocopyable.h"
namespace skylu{
    class Timer :Nocopyable {
    public:
        typedef std::function<void()> TimerCallback;
        Timer(TimerCallback cb,Timestamp when,double interval);
        void run()const{m_callback();}
        bool repeat(){return isRepeat;}

        void restart(Timestamp now);
        Timestamp getExpiration() {return m_expiration;}
        int64_t  getSequence(){return m_sequence;}




    private:
        const TimerCallback m_callback; //回调函数
        Timestamp m_expiration; //到期时间
        const double m_interval; //间隔时间
        const bool isRepeat;
        const int64_t m_sequence;

        static std::atomic_long g_AutocreatemSequence;
    };

}


#endif //HASHTEST_TIMER_H
