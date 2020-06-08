//
// Created by jimlu on 2020/5/20.
//

#include "tcpconnection.h"
#include "../base/log.h"
#include <assert.h>
#include <functional>
#include <algorithm>
namespace skylu{
    TcpConnection::TcpConnection(EventLoop *loop, Socket::ptr socket,const std::string & name)
            :m_state(kConnceting)
            ,m_loop(loop)
            ,m_socket(socket)
            ,m_channel(new Channel(m_loop,m_socket->getSocket()))
            ,m_name(name),
            m_highMark(64*1024*1024){  //高水位的位置应该是64K
        m_channel->setReadCallback(std::bind(&TcpConnection::handleRead,this));
        m_channel->setErrorCallback(std::bind(&TcpConnection::handleError,this));
        m_channel->setCloseCallback(std::bind(&TcpConnection::handleClose,this));
        m_channel->setWriteCallback(std::bind(&TcpConnection::handleWrite,this));
    }

    void TcpConnection::handleWrite() {

        assert(m_loop->isInLoopThread());
        if(m_channel->isWriting()){
            ssize_t n = ::write(m_channel->getFd(),m_output_buffer.curRead(),m_output_buffer.readableBytes());
            if(n > 0){
                m_output_buffer.updatePos(n);
                if(m_output_buffer.readableBytes() == 0){
                    m_channel->disableWriting();
                    if(m_writecomplete_cb){
                        m_loop->queueInLoop(std::bind(m_writecomplete_cb,shared_from_this()));
                    }
                    if(m_state == kdisConnecting){
                        shutdownInLoop();

                    }
                }else{
                    SKYLU_LOG_INFO(G_LOGGER)<<"write("<<m_channel->getFd()<<") more data";
                }
            }else{
                SKYLU_LOG_ERROR(G_LOGGER)<<"errno ="<<errno<<"  strerror="<<strerror(errno);
            }
        }else{
            SKYLU_LOG_INFO(G_LOGGER)<<"Connection is down ,no more writing.";
        }

    }
    void TcpConnection::handleRead() {

        assert(m_loop->isInLoopThread());
        int savedError;
        ssize_t  n = m_input_buffer.readFd(m_channel->getFd(),&savedError);
        if(n > 0){
            if(m_message_cb){
                m_message_cb(shared_from_this(),&m_input_buffer);
            }

        }else if(n == 0){
            handleClose();
        }else{
            errno = savedError;
            handleError();
        }
    }

    void TcpConnection::handleError() {
        SKYLU_LOG_ERROR(G_LOGGER)<<"TcpConnection("<<m_channel->getFd()<<") error:"<<errno<<"   strerror:"<<strerror(errno);
    }
    void TcpConnection::handleClose() {
        assert(m_loop->isInLoopThread());
        assert(m_state == kConnected || m_state == kdisConnecting);
        setState(kdisConnected);
        m_channel->disableAll();
        TcpConnection::ptr conne(shared_from_this());
      //  m_connection_cb(conne);
        m_close_cb(shared_from_this());
    }

    void TcpConnection::connectDestroyed() {
        assert(m_loop->isInLoopThread());

        if(m_state == kConnected){
            setState(kdisConnected);
            m_channel->disableAll();
            if(m_connection_cb)
                m_connection_cb(shared_from_this());
        }
        m_channel->remove();
    }
    void TcpConnection::connectEstablished() {
        assert(m_loop->isInLoopThread());
        assert(m_state == kConnceting);
        setState(kConnected);
        m_channel->enableReading();
        if(m_connection_cb)
            m_connection_cb(shared_from_this());

    }
    void TcpConnection::shutdown() {
        if(m_state == kConnected){
            setState(kdisConnecting);
            m_loop->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));
        }
    }

    void TcpConnection::shutdownInLoop() {
        assert(m_loop->isInLoopThread());
        if(!m_channel->isWriting()){
            m_socket->shoutdownWriting();
        }
    }

    void TcpConnection::send(const std::string & message){
        send(message.data(),message.size());
    }
    void TcpConnection::send(const void *message, size_t len) {
        if(m_state == kConnected){
            if(m_loop->isInLoopThread()){
                sendInLoop(message,len);
            } else{
                m_loop->runInLoop(std::bind(&TcpConnection::sendInLoop,this,message,len));
            }
        }

    }

    void TcpConnection::send(Buffer *buf) {
        if(m_state == kConnected){
            if(m_loop->isInLoopThread()){
                sendInLoop(buf->curRead(),buf->readableBytes());
                buf->resetAll();
            }else{
                m_loop->runInLoop(std::bind(&TcpConnection::sendInLoop,
                        this,buf->curRead(),buf->readableBytes()));
                buf->resetAll();
            }
        }
    }


    void TcpConnection::sendInLoop(const void *data, size_t len) {
        assert(m_loop->isInLoopThread());
        ssize_t nwrite = 0;
        size_t remaning = len;
        bool Error=false;
        if(m_state == kdisConnected){
            SKYLU_LOG_WARN(G_LOGGER)<<"disconnected ,give up writing";
            return ;
        }
        if(!m_channel->isWriting() && m_output_buffer.readableBytes() == 0){
            nwrite = ::write(m_socket->getSocket(),data,len);

            if(nwrite >= 0){
                    //注册handleWrite事件
                    remaning = len - nwrite;
                    if(remaning == 0 && m_writecomplete_cb){
                        m_loop->queueInLoop(std::bind(m_writecomplete_cb,shared_from_this()));

                    }

            }else{
                nwrite = 0;
                if(errno != EWOULDBLOCK){
                SKYLU_LOG_ERROR(G_LOGGER)<<"TcpConnection::sendInLoop errno="<<errno
                            << "  strerrno="<<strerror(errno);
                Error = true;

                }
            }
        }

        assert(nwrite>=0);
        if(!Error && remaning > 0){
            size_t oldlen = m_output_buffer.readableBytes();
            if(oldlen + remaning >= m_highMark
                    && oldlen < m_highMark
                    && m_highwater_cb){
                m_loop->queueInLoop(std::bind(m_highwater_cb,shared_from_this(),oldlen+remaning));
            }
            m_output_buffer.append(static_cast<const char*>(data)+nwrite,remaning);
            if(!m_channel->isWriting()){
                m_channel->enableWriting();
                SKYLU_LOG_INFO(G_LOGGER)<<"write too large";
            }
        }

    }
}