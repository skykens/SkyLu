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
            ,isStart(false)
            ,m_maps(new MemcachedMap){
        server.setMessageCallback(std::bind(&MemcachedServer::doParseLine,this,std::placeholders::_1,std::placeholders::_2));

        server.setConnectionCallback([](const TcpConnection::ptr & conne){
            SKYLU_LOG_INFO(G_LOGGER)<<"new conne: "<<conne->getName();
        });

        server.setCloseCallback([](const TcpConnection::ptr & conne){
            SKYLU_LOG_INFO(G_LOGGER)<<"close conne :"<<conne->getName();
        });


    }
    void MemcachedServer::setThreadNums(int num) {
        m_threads = num;
        server.setThreadNum(num);

    }


    void MemcachedServer::doParseLine(const TcpConnection::ptr &conne, Buffer *buf) {
        const char * crlf =  buf->findCRLF();
        if(crlf){
            const char * space = std::find(buf->curRead(),crlf,' ');
            if(space == crlf){
                conne->send(CLIENT_ERROR("can't found space"));
                return ;
            }
            std::string cmd(buf->curRead(),space);
            if(cmd == "get" || cmd == "delete"){
                std::vector<std::string> keys;
                for(const char *start = space + 1;space != crlf;){
                    space = std::find(space + 1,crlf,' ');
                    keys.emplace_back(start,space);
                    if(space == crlf){
                        break;
                    }
                }
                if(keys.empty()){
                    conne->send(CLIENT_ERROR("can't found key"));
                    conne->shutdown();
                    return ;
                }
                if(cmd[0] == 'g')
                    get(conne,keys);
                else
                    del(conne,keys[0]);

                buf->updatePosUntil(crlf+2);

            }else if(cmd == "set"){
                int bytes = 0,flag =0;
                bool isFlag = false,isBytes = false,isTimeout = false;
                std::string key;
                for(const char * start = space + 1; space != crlf; start = space +1){
                    space = std::find(space + 1,crlf,' ');
                    if(key.empty())
                        key.assign(start,space);
                    else if(!isFlag){

                        flag = atoi(start);
                        isFlag = true;
                    }else if(!isTimeout){
                        //这里读取超时时间，但是定时器部分的内容还写做，之后再写
                        isTimeout = true;
                    }else if(!isBytes){
                        bytes = atoi(start);

                        isBytes = true;
                    }
                    if(space == crlf){
                        break;
                    }
                }
                if(!isBytes || !isFlag || !isTimeout){
                    conne->send(CLIENT_ERROR("flag 、exptime 、bytes is must."));
                    conne->shutdown();
                    return ;
                }
                assert(bytes);
                buf->updatePosUntil(crlf+2);
                auto it = m_maps->find(key);
                if(it == m_maps->end()){
                  Node *  value = new Node;
                    value->flag = flag;
                    value->val.assign(buf->curRead(),buf->curRead()+bytes);
                    assert(!value->val.empty());
                    set(conne,key,value);
                }else{
                    it->second->flag = flag;
                    it->second->val.assign(buf->curRead(),buf->curRead()+bytes);
                    assert(!it->second->val.empty());
                    buf->updatePos(bytes+2);
                    conne->send(STORED);
                }
                buf->updatePos(bytes+2);

            }else{
                conne->send(NOT_FOUND);
                conne->shutdown();
            }
        }else{
           conne->send(CLIENT_ERROR("can't found "));
           conne->shutdown();
        }


    }
    void MemcachedServer::run() {
        isStart = true;
        server.start();
        m_loop->loop();
        isStart = false;

    }

    void MemcachedServer::del(const TcpConnection::ptr &conne, const std::string &key) {


            size_t n = m_maps->erase(key.data());
            if(n == 1){
                conne->send(DELETED);
            }else{
                conne->send(NOT_FOUND);
            }

    }

    void MemcachedServer::set(const TcpConnection::ptr &conne, const std::string &key, Node * node) {

        m_maps->emplace(key.data(),node);
        conne->send(STORED);

    }

    void MemcachedServer::get(const TcpConnection::ptr &conne, const std::vector<std::string> &key) {

        Buffer buff;
        for(auto it : key){
                auto item = m_maps->find(it.data());
                if(item == m_maps->end()){
                    conne->send(NOT_FOUND);
                }else{
                 //   buff.append("VALUE " + it + " "+std::to_string(item->second->flag) + " " + std::to_string(item->second->val.size())+ "\r\n"
               //                 + item->second->val + "\r\n");
                  //  conne->send(&buff);
                    conne->send("VALUE " + it + " "+std::to_string(item->second->flag) + " " + std::to_string(item->second->val.size())+ "\r\n"
                                + item->second->val + "\r\n");
                    buff.resetAll();
                }
            }
            conne->send("END\r\n");


    }





}