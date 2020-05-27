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
#include "session.h"
#include <memory>
#include <set>
#include <malloc.h>
#include <unordered_set>
#include "bytestring.h"
#include "item.h"
#include "session.h"


#define SHARED_NUM 4096
namespace skylu {

    class MemcachedServer : Nocopyable {

    public:
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
        Item* get(const ByteString & key);
        bool del(const ByteString & key);
        bool set(const ByteString & key,const ByteString &value,int flag);


    private:
        struct Hash{
            size_t operator()(const Item::ptr & ptr)const{
                return ptr->hash();
            }
        };
        struct Equal{
            bool operator()(const Item::ptr& lh,const Item::ptr &rh)const{
                return lh->getKey() == rh->getKey();
            }
        };

    private:

        void NewSession(const TcpConnection::ptr &conne);

        typedef std::unordered_set<Item::ptr,Hash,Equal> MemcachedMap;
        typedef std::unordered_map<TcpConnection::ptr,Session *> SessionSet;
        typedef std::array<MemcachedMap,SHARED_NUM> SharedMap;
        TcpServer server;
        std::string m_name;
        Eventloop * m_loop;
        int m_threads;
        bool isStart;

        SharedMap m_maps;
        SessionSet m_sessions;



        Mutex m_mutex;


    };
}


#endif //HASHTEST_MEMCACHEDSERVER_H
