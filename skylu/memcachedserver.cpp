//
// Created by jimlu on 2020/5/25.
//

#include "memcachedserver.h"
#include "log.h"
namespace skylu{
    MemcachedServer::MemcachedServer(Eventloop *loop, const Address::ptr &addr,const std::string & name)
            : server(loop,addr,name)
            ,m_name(name)
            ,m_loop(loop)
            ,m_threads(0)
            ,isStart(false){

        server.setConnectionCallback(std::bind(&MemcachedServer::NewSession,this,std::placeholders::_1));
        server.setCloseCallback([this](const TcpConnection::ptr & conne){
            m_sessions.erase(conne);
            SKYLU_LOG_INFO(G_LOGGER)<<"close conne :"<<conne->getName();
        });

    }
    void MemcachedServer::setThreadNums(int num) {
        m_threads = num;
        server.setThreadNum(num);

    }


    void MemcachedServer::run() {
        isStart = true;
        server.start();
        m_loop->loop();
        isStart = false;

    }

    bool MemcachedServer::del(const ByteString &key) {



            Item::ptr tmp(new Item(key));
            size_t n = m_maps[tmp->hash() % SHARED_NUM].erase(tmp);
            return n==1;

    }

    bool MemcachedServer::set(const ByteString &key, const ByteString &value, int flag) {

        size_t hashcode = Item::hash(key);
        auto it = m_maps[hashcode%SHARED_NUM].insert(Item::makeItem(key,value,flag,hashcode));
        if(!it.second){
            it.first->get()->setValue(value);
        }
        return true;

    }

    Item* MemcachedServer::get(const ByteString &key) {

        Item::ptr tmp(new Item(key));
        auto it = m_maps[tmp->hash()%SHARED_NUM].find(tmp);
        if(it != m_maps[tmp->hash()%SHARED_NUM].end()){
            return it->get();
        }else{
            return nullptr;
        }


    }


    void MemcachedServer::NewSession(const TcpConnection::ptr & conne) {
        Session * session = new Session(this,conne);
        SKYLU_LOG_INFO(G_LOGGER)<<"new Session create  from conne:"<<conne->getName();
        m_sessions.insert({conne,session});
    }




}