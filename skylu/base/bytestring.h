//
// Created by jimlu on 2020/5/26.
//

#ifndef HASHTEST_BYTESTRING_H
#define HASHTEST_BYTESTRING_H
/**
 * C语言风格实现的string  仅仅保存char*地址和长度 ，容易出现野指针
 */
#include <sys/types.h>
#include <iostream>
#include <string>
#include <string.h>
class ByteString {
public:
    ByteString():m_data(NULL),m_len(0){}
    ~ByteString() = default;
    ByteString(const char *begin,const char *end):m_data(begin),m_len(end-begin){}
    ByteString(const char *src) : m_data(src),m_len(strlen(src)){}
    ByteString(std::string &src): m_data(src.data()),m_len(strlen(src.data())){}
    ByteString(const char *src,size_t len):m_data(src),m_len(len){}

    inline size_t getLen()const {return m_len;}
    inline const char * getData()const{return m_data;}
    inline const char *getBegin()const{return m_data;}
    inline const char * getEnd()const{return m_data + m_len;}
    inline void reset(){m_data = nullptr; m_len = 0;}
    inline void set(const char * src){m_data = src; m_len = strlen(src);}
    inline void set(const char * src,int len){m_data = src ; m_len = static_cast<size_t>(len);}

    bool operator == (const ByteString& rh)const{
        return (m_len == rh.m_len) && (memcmp(m_data,rh.m_data,m_len) == 0);
    }

    bool operator !=(const ByteString & rh)const{
        return !(*this == rh);
    }

#define STRINGPIECE_BINARY_PREDICATE(cmp,auxcmp)                             \
  bool operator cmp (const ByteString& x) const {                           \
    int r = memcmp(m_data, x.m_data, m_len < x.m_len ? m_len : x.m_len); \
    return ((r auxcmp 0) || ((r == 0) && (m_len cmp x.m_len)));          \
  }
    STRINGPIECE_BINARY_PREDICATE(<,  <);
    STRINGPIECE_BINARY_PREDICATE(<=, <);
    STRINGPIECE_BINARY_PREDICATE(>=, >);
    STRINGPIECE_BINARY_PREDICATE(>,  >);
#undef STRINGPIECE_BINARY_PREDICATE
    int compare(const ByteString& rh)const {
        int n = memcmp(m_data,rh.m_data,m_len < rh.m_len ? m_len : rh.m_len);
        if(n == 0){
            if(m_len < rh.m_len)
                n = -1;
            else if(m_len > rh.m_len)
                n = 1;

        }

        return n;
    }

private:
    const char * m_data;
    size_t m_len;

};


#endif //HASHTEST_BYTESTRING_H
