//
// Created by jimlu on 2020/5/19.
//

#include "timer.h"
namespace skylu{

    std::atomic_long Timer::g_AutocreatemSequence;
    Timer::Timer(TimerCallback cb,Timestamp when, double interval)
        :m_callback(cb)
        ,m_expiration(when)
        ,m_interval(interval)
        ,isRepeat(interval > 0)
        ,m_sequence(g_AutocreatemSequence++){
    }
    void Timer::restart(Timestamp now) {
        if(isRepeat){
            m_expiration = addSecond(now,m_interval);

        }else{
            m_expiration = Timestamp::invalid(); //设为无效参数
        }
    }

}
