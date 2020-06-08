//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TIMESTAMP_H
#define HASHTEST_TIMESTAMP_H

#include <inttypes.h>
#include <iostream>
#include <string>
#include <algorithm>
#include "../base/nocopyable.h"
namespace skylu{
    class Timestamp {
    public:
        Timestamp();
        Timestamp(int64_t microSeconds);
        inline void swap(Timestamp &ti){
            std::swap(m_microSeconds,ti.m_microSeconds);
        }
        std::string toString()const;
        bool isVaild()const{return m_microSeconds>0;}
        int64_t getMicroSeconds()const {return m_microSeconds;}
        time_t seconds()const{return static_cast<time_t>(m_microSeconds/kMicroSecondsPerSecond);}

        static Timestamp now();
        static Timestamp invalid(){return Timestamp();}

        static Timestamp fromUnixTime(time_t t,int microseconds){
            return Timestamp(static_cast<int64_t>(t)*kMicroSecondsPerSecond + microseconds);
        }

    public:
        static const int kMicroSecondsPerSecond = 1000 * 1000;
    private:
        int64_t m_microSeconds;  // 开始的时间(微秒


    };

    inline bool operator<(Timestamp lhs,Timestamp rhs){
        return lhs.getMicroSeconds() < rhs.getMicroSeconds();
    }

    inline bool operator==(Timestamp lhs,Timestamp rhs){
        return lhs.getMicroSeconds() == rhs.getMicroSeconds();
    }

    inline double timeDifference(Timestamp high,Timestamp low){
        int64_t  diff = high.getMicroSeconds() - low.getMicroSeconds();
        return static_cast<double>(diff / Timestamp::kMicroSecondsPerSecond);
    }
    inline Timestamp addSecond(Timestamp timestamp ,double seconds){
        int64_t  ret = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
        return Timestamp(timestamp.getMicroSeconds() + ret);
    }

}


#endif //HASHTEST_TIMESTAMP_H
