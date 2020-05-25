//
// Created by jimlu on 2020/5/25.
//

#ifndef HASHTEST_MEMCACHEDSERVER_H
#define HASHTEST_MEMCACHEDSERVER_H

#include <string>
#include <unordered_map>

#include "tcpserver.h"
#include "nocopyable.h"
#include "eventloop.h"
#include "buffer.h"
#include "Timestamp.h"
#include "TcpConnection.h"
#include "address.h"
#include "mutex.h"
#include <memory>
#include <hash_map>
#define CLIENT_ERROR(message) "CLIENT_ERROR "#message"\r\n"
#define SERVER_ERROR(message) "SERVER_ERROR "#message"\r\n"
#define NOT_FOUND "NOT_FOUND\r\n"
#define STORED "STORED\r\n"
#define NOT_STORED "NOT_STORED\r\n"
#define EXISTS "EXISTS\r\n"
#define DELETED "DELETED\r\n"
#define align 1024
namespace skylu {

    class MemcachedServer : Nocopyable {
        struct Node{
            Node():flag(0){}
            int flag;
            std::string val;
        };
        enum Cmd{
            GET,
            SET,
            DELETE
        };

    public:
        MemcachedServer(Eventloop * loop,const Address::ptr &addr , const std::string & name);
        ~MemcachedServer() = default;
        void setThreadNums(int num);


        void run();

    private:
        void doParseLine(const TcpConnection::ptr & conne,Buffer * buf);




        void get(const TcpConnection::ptr &conne,const std::vector<std::string>  & key);
        void del(const TcpConnection::ptr &conne,const std::string &key);
        void set(const TcpConnection::ptr &conne,const std::string &key,Node * node);


    private:

        typedef std::unordered_map<std::string,Node*> MemcachedMap;
        TcpServer server;
        std::string m_name;
        Eventloop * m_loop;
        int m_threads;
        bool isStart;

        std::shared_ptr<MemcachedMap> m_maps;

        Mutex m_mutex;


    };
}


#endif //HASHTEST_MEMCACHEDSERVER_H
