//
// Created by jimlu on 2020/5/14.
//环形队列
//

#ifndef KV_CIRCLEQUEUE_H
#define KV_CIRCLEQUEUE_H

#include <functional>
#include <vector>
#include <iostream>
#include <string.h>
#include <memory>
#include "algorithm"
#include "nocopyable.h"

#include "log.h"
#include <assert.h>
namespace skylu {

    static Logger::ptr cG_LOGGER = SKYLU_LOG_NAME("system");

    class Buffer {
    public:
        static const size_t kInitialSize = 1024;
        static const size_t kInitPos = 0;
        typedef std::shared_ptr<Buffer> ptr;
        Buffer(size_t size = kInitialSize)
            :m_wpos(0)
            ,m_rpos(0)
            ,m_queue(size)
            {
        }
        ~Buffer() = default;

        size_t readFd(int fd,int *saveError);

        /**
         *可读字节数
         * @return
         */
        size_t readabelBytes() const { return m_wpos - m_rpos;}

        /**
         * 可写字节数
         * @return
         */
        size_t writeableBytes()const {return m_queue.size() - m_wpos;}

        /****
         ** @brief 得到队列的空间大小
         **/
        inline const size_t capacity()const{return m_queue.size();}


        /**
         ** @brief  获得写坐标
         **/
        inline const size_t getWPos()const {return m_wpos;}

        /**
         ** @brief 获得读坐标
         **/
        inline const size_t getRPos() const{return m_rpos;}


        const char * findCRLF()const {
            const char CRLF[] = "\r\n";
            const char * ret = std::search(curRead(),curWrite(),CRLF,CRLF+2);
            return ret == curWrite()? nullptr : ret;
        }



        char * curWrite(){
            return reinterpret_cast<char*>(m_queue.data()) + m_wpos;
        }

        const char* curWrite() const{
            return reinterpret_cast<const char *>(m_queue.data()) + m_wpos;

        }

     //   char *curRead(){
       //     return reinterpret_cast<char *>(m_queue.data()) + m_rpos;
        //}

        const char *curRead() const {

            return reinterpret_cast<const char *>(m_queue.data()) + m_rpos;

        }

        void resetAll(){
            m_rpos = kInitPos;
            m_wpos = kInitPos;

        }

        void swap(Buffer &rhs){
            m_queue.swap(rhs.m_queue);
            std::swap(m_wpos,rhs.m_wpos);
            std::swap(m_rpos,rhs.m_rpos);
        }

        void updatePos(size_t len){
            assert(len <= readabelBytes());
            if(len < readabelBytes()){
                m_rpos += len;

            }else{
                resetAll();
            }
        }

        void updatePosUntil(const char *end){  //取出数据直到END
            updatePos(end - curRead());
        }


        void hasWriteten(size_t len){
            assert(len <= writeableBytes());
            m_wpos +=len;
        }



        void ensureWriteableBytes(size_t len){
            if(writeableBytes() < len){
                resize(len);

            }
            assert(writeableBytes() >= len);

        }

        void append(const std::string & data){
            append(data.data(),data.size());

        }
        void append(const char * data,size_t len){
            ensureWriteableBytes(len);
            std::copy(data,data+len,curWrite());
            hasWriteten(len);
        }


    private:

        char *begin(){
            return reinterpret_cast<char*>(m_queue.data());
        }
        void resize(size_t len){
            if(writeableBytes() + m_rpos < len + kInitPos){

                m_queue.resize(m_wpos + len);

            }else{
                assert(kInitPos < m_rpos);
                size_t readable = readabelBytes();
                std::copy(begin()+m_rpos,begin()+m_wpos,begin()+kInitPos);
                m_rpos = kInitPos;
                m_wpos = m_rpos + readable;
                assert(readable == readabelBytes());
            }

        }

    private:
        size_t m_wpos; //写坐标
        size_t m_rpos; //读坐标
        std::vector<char> m_queue;


    };
}

#endif //KV_CIRCLEQUEUE_H
