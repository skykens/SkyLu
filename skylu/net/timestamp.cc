//
// Created by jimlu on 2020/5/19.
//

#include "timestamp.h"
#include <time.h>
#include <sys/time.h>


namespace skylu{
    static_assert(sizeof(Timestamp) == sizeof(int64_t),"Timestamp is same size as int64_t");
    Timestamp::Timestamp() :m_microSeconds(0){

    }
    Timestamp::Timestamp(int64_t microSeconds) :m_microSeconds(microSeconds){

    }
    std::string Timestamp::toString() const
    {
        char buf[32] = {0};
        int64_t seconds = m_microSeconds / kMicroSecondsPerSecond;
        int64_t microseconds = m_microSeconds % kMicroSecondsPerSecond;
        snprintf(buf, sizeof(buf)-1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);
        return buf;
    }


    Timestamp Timestamp::now() {
        struct timeval tv;
        gettimeofday(&tv,NULL);

        int64_t  seconds = tv.tv_sec;
        return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);

    }
}