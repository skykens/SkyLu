//
// Created by jimlu on 2020/5/19.
//

#include "tcpserver.h"
#include "eventloopthreadpool.h"
#include <assert.h>

namespace skylu{

    TcpServer::Acceptor::Acceptor(Eventloop *loop, const Address::ptr listenAddr)
        :m_loop(loop)
        ,m_socket(Socket::CreateTCP(listenAddr))
        ,m_acceptChannel(loop,m_socket->getSocket())
        ,m_isListening(false){
        m_socket->bind(listenAddr);
        m_acceptChannel.setReadCallback(std::bind(&TcpServer::Acceptor::handleRead,this));


    }

    void TcpServer::Acceptor::listen() {
        assert(m_loop->isInLoopThread());
        m_isListening = true;
        m_socket->listen();
        m_acceptChannel.enableReading();
    }

    void TcpServer::Acceptor::handleRead() {
        assert(m_loop->isInLoopThread());
        //这里没有考虑文件描述符耗尽情况，可以改进： 在拿到大于0的fd后阻塞一下查看是否可读(epoll)
        Socket::ptr fd = m_socket->accept();
        if(fd->isValid()){
            m_connection_cb(fd);
        }else{
            fd->close();
        }

    }


    TcpServer::TcpServer(Eventloop *loop, const Address::ptr address,const std::string &name)
            :m_loop(loop)
            ,m_name(name)
            ,m_acceptor(new TcpServer::Acceptor(loop,address))
            ,isStart(false)
            ,m_threadpool(new EventLoopThreadPool(m_loop,name+"LoopPool")){
        m_acceptor->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,std::placeholders::_1));

    }

    void TcpServer::start() {
        isStart = true;
        m_threadpool->start();
        assert(!m_acceptor->isListening());
        m_loop->runInLoop(std::bind(&Acceptor::listen,m_acceptor.get()));
    }

    void TcpServer::removeConnection(const TcpConnection::ptr &conn) {
        m_loop->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
    }
    void TcpServer::removeConnectionInLoop(const TcpConnection::ptr &conn) {
        assert(m_loop->isInLoopThread());
        size_t n = m_connections.erase(conn->getName());
        assert(n == 1);
        Eventloop * ioloop = conn->getLoop();
        ioloop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));

    }

    void TcpServer::newConnection(Socket::ptr socket) {

        static int64_t  count = 1;
        std::string name = m_name + "#" +std::to_string(count++);
        Eventloop * ioloop = m_threadpool->getNextLoop();
        TcpConnection::ptr conne(new TcpConnection(ioloop,socket,name));
        m_connections[name] = conne;
        conne->setConnectionCallback(m_connection_cb);
        conne->setMessageCallback(m_message_cb);
        //这里需要绑定一下关闭连接的回调函数
        TcpConnection::CloseCallback tmpcb = std::bind(&TcpServer::removeConnection,this,std::placeholders::_1);
        conne->setCloseCallback(tmpcb);
        ioloop->runInLoop(std::bind(&TcpConnection::connectEstablished,conne));


    }

    void TcpServer::setThreadNum(int numThreads) {
        assert(0 <= numThreads);
        m_threadpool->setThreadNum(numThreads);
    }
}