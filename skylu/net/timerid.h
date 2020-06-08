//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TIMERID_H
#define HASHTEST_TIMERID_H

#include <iostream>
#include "timer.h"
#include <sys/types.h>
namespace skylu{
    class Timerid {
    public:
        Timerid():m_sequence(0),m_timer(NULL){}
        Timerid(Timer* timer,uint64_t seq):m_sequence(seq),m_timer(NULL){}

        uint64_t  getSequence()const {return m_sequence;}

        friend class TimerQueue;
    private:
        uint64_t m_sequence;
        Timer * m_timer;


    };

}


#endif //HASHTEST_TIMERID_H
