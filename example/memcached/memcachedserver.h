//
// Created by jimlu on 2020/5/25.
//

#ifndef HASHTEST_MEMCACHEDSERVER_H
#define HASHTEST_MEMCACHEDSERVER_H

#include <string>
#include <unordered_map>
#include "skylu/net/tcpserver.h"
#include "skylu/base/nocopyable.h"
#include "skylu/net/eventloop.h"
#include "skylu/net/buffer.h"
#include "skylu/net/timestamp.h"
#include "skylu/net/tcpconnection.h"
#include "skylu/net/address.h"
#include <memory>
#include <set>
#include <malloc.h>
#include <unordered_set>
#include "skylu/base/bytestring.h"
#include "item.h"
#include "session.h"


#define SHARED_NUM 4096
namespace skylu {
    class MemcachedServer : Nocopyable {
    public:
        enum aofState{
            Init,
            Append,
            Sync,
            ReWrittening,
            ReWrittened

        };
        enum Cmd{
            GET,
            SET,
            DELETE
        };

    public:
        MemcachedServer(EventLoop * loop,const Address::ptr &addr , const std::string & name);
        ~MemcachedServer() = default;
        void setThreadNums(int num);
        void run();

        Item* get(const ByteString & key);
        bool del(const ByteString & key);
        bool set(const ByteString & key,const ByteString &value,int flag);

        void appendAof(const ByteString &content);

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

        void init();
        void LoadAof(Timestamp &when);
        void setReWrittened();

        void NewSession(const TcpConnection::ptr &conne);
        void AofSync();

        void RewrittenAOF();

    private:
        static const char * aof_file_name;
        static const char * aof_written_file_name;
        static const size_t aof_file_length;


        typedef std::unordered_set<Item::ptr,Hash,Equal> MemcachedMap;
        typedef std::unordered_map<TcpConnection::ptr,Session *> SessionSet;
        typedef std::array<MemcachedMap,SHARED_NUM> SharedMap;
        TcpServer server;
        std::string m_name;
        EventLoop * m_loop;
        int m_threads;
        bool isStart;
        SharedMap m_maps;
        SessionSet m_sessions;
        Mutex m_mutex;
        aofState m_aof_state;
        void *m_aof_peek;
        size_t m_aof_seek;
        Buffer m_aof_rewritten_buff;
        std::unique_ptr<Channel> m_pipe;
        Session m_aof_session;



    };
}


#endif //HASHTEST_MEMCACHEDSERVER_H
