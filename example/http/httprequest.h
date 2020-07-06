//
// Created by jimlu on 2020/5/23.
//

#ifndef HASHTEST_HTTPREQUEST_H
#define HASHTEST_HTTPREQUEST_H

#include <string>
#include <map>
#include <iostream>
#include "skylu/net/buffer.h"



namespace skylu{
    class HttpRequest {
    static  const char * kWWW_ROOT;
    public:
        enum HttpRequestParseState{
            RequestLine,
            Headers,
            Body,
            Finish
        };
        enum Method{
            INVALID,GET,POST,HEAD,PUT,DELETE
        };

        enum Version{
            UNKNOW,HTTP10,HTTP11
        };

        HttpRequest(Buffer *input);
        ~HttpRequest() = default;

        void appendOutBuffer(const Buffer & buf){ m_output_buffer.append(buf.curRead(),buf.readableBytes());}
        int writeableBytes(){return m_output_buffer.readableBytes();}

        bool isWorking()const {return m_isWorking;}

        bool parseRequest();
        bool isFinnish(){return m_state == Finish;}
        void resetParse();

        std::string getPath()const {return m_path;}
        std::string getArg()const {return m_arg;}
        std::string getHeader(const std::string &fild)const;
        std::string getMethod()const ;
        Buffer* getBody() {return &m_body;}
        int getTimeout()const;
        std::string getVersion() const {return getVersion(m_version);}
        static std::string getVersion(Version ver);
        bool isKeepAlive()const;
        bool isCGI()const;



    private:
        bool parseRequestLine(const char *begin,const char *end);
        bool setMethod(const char *begin,const char *end);
        void setPath(const char * begin,const char*end);
        void setArg(const char *begin,const char *end){m_arg.assign(begin,end);}
        void setVersion(Version version){m_version = version;}
        /**
         * 添加字段
         * @param start  字段开始位置
         * @param namelast  字段结束位置
         * @param end  crlf结束位置
         */
        void addHeader(const char *start,const char * namelast,const char *end);


    private:
        Buffer* m_input_buffer; //输入缓冲区 用来读出里面的数据
        Buffer m_output_buffer; //输出缓冲区 写入conn
        Buffer m_body;  //用来存放报文体

        Version m_version;
        Method m_method;
        bool m_isWorking;

        HttpRequestParseState m_state; //解析的状态

        std::string m_path;
        std::string m_arg;  //URL参数
        std::map<std::string,std::string> m_header; //报文头部



    };

}


#endif //HASHTEST_HTTPREQUEST_H
