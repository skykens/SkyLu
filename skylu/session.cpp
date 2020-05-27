//
// Created by jimlu on 2020/5/26.
//

#include "session.h"
#include "memcachedserver.h"
#include <sstream>
namespace skylu{
    Session::Session(MemcachedServer *owner, const TcpConnection::ptr & conne)
            :m_owner(owner)
            ,m_conne(conne)
            ,m_state(init)
            ,m_bytes(0)
            ,m_flag(0)
            ,m_output(65536){
        TcpConnection::MessageCallback  cb = std::bind(&Session::onMessage,this,std::placeholders::_1,std::placeholders::_2);
        conne->setMessageCallback(cb);
    }
    void Session::onMessage(const TcpConnection::ptr &conne, Buffer *buf) {
        const char * crlf =  buf->findCRLF();
        m_state = init;
        if(crlf){
            const char * space = std::find(buf->curRead(),crlf,' ');
            if(space == crlf){
                reply(CLIENT_ERROR("can't found space"));
                return ;
            }

            cmd = ByteString(buf->curRead(),space);
            m_state = recvCmd;
            if(cmd == "get" || cmd == "delete"){
                for(const char *start = space + 1;space != crlf;){
                    space = std::find(space + 1,crlf,' ');
                    keys.emplace_back(start,space);
                    if(space == crlf){
                        m_state = recvKey;
                        break;
                    }
                }
                if(m_state != recvKey){
                    reply(CLIENT_ERROR("can't found key"));
                    closeSession();
                    return ;
                }
                if(cmd.getData()[0] == 'g')
                    doFind();
                else
                    doDelete();

                buf->updatePosUntil(crlf+2);
                static int count = 0;
                if(buf->getRPos() == buf->getWPos()){
                    count++;
                }else{
                    SKYLU_LOG_INFO(G_LOGGER)<<"error";
                }


                assert(buf->getRPos() == buf->getWPos());
                keys.clear();

            }else if(cmd == "set"){
                for(const char * start = space + 1; space != crlf; start = space +1){
                    space = std::find(space + 1,crlf,' ');
                    if(m_state == recvCmd) {

                        key= ByteString(start, space);
                        m_state = recvKey;
                    }else if(m_state == recvKey){

                        m_flag = atoi(start);
                        m_state = recvFlag;


                    }else if(m_state == recvFlag){
                        //这里读取超时时间，但是定时器部分的内容还写做，之后再写
                        m_state = recvTimeout;


                    }else if(m_state == recvTimeout){

                        m_bytes = atoi(start);
                        m_state = recvBytes;

                    }if(space == crlf){
                        break;
                    }
                }
                if(m_state != recvBytes){
                    reply(CLIENT_ERROR("flag 、exptime 、bytes is must."));
                    closeSession();
                    return ;
                }
                assert(m_state == recvBytes);
                assert(m_bytes);
                buf->updatePosUntil(crlf+2);
                doUpdate(buf);

            }else{
                reply(NOT_FOUND);
                closeSession();
            }
        }else{
            reply(CLIENT_ERROR("can't found "));
            closeSession();
        }



    }


    void Session::doDelete() {
        if(m_owner->del(keys[0])){
            reply(DELETED);

        }else{
            reply(NOT_FOUND);
        }
    }

    void Session::doFind() {
        assert(keys.size());
        Item * item = nullptr;
        for(auto it : keys){
            item = m_owner->get(it);
            if(item){


                    m_output.append("VALUE ");
                    m_output.append(it.getData(), it.getLen());
                    std::stringstream ss;
                    ss<<" "<<item->getFlag()<<" "<<item->getValueLen()<<"\r\n";
                    m_output.append(ss.str());
                    m_output.append(item->getValue(), item->getValueLen());
                    reply();

            }else{
                reply(NOT_FOUND);
            }
        }
        reply(END);

    }
    void Session::reply() {
        m_conne->send(&m_output);
    }

    void Session::reply(const std::string &message) {
        m_conne->send(message);
    }
    void Session::reply(const char *src,size_t len) {
        m_conne->send(src,len);
    }

    void Session::closeSession() {
        m_conne->shutdown();
    }

    Session::~Session() {
        closeSession();
        SKYLU_LOG_INFO(G_LOGGER)<<"session close";
    }
    void Session::doUpdate(Buffer *buf) {

            value = ByteString(buf->curRead(),buf->curRead()+m_bytes+2);
            if(!m_owner->set(key,value,m_flag)){
                reply(SERVER_ERROR("too larget"));
            }else{
                reply(STORED);
            }
        buf->updatePos(m_bytes+2);

        assert(buf->curRead() == buf->curWrite());
    }

}
