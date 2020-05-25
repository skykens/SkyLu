//
// Created by jimlu on 2020/5/19.
//

#ifndef HASHTEST_TCPSERVER_H
#define HASHTEST_TCPSERVER_H


#include "socket.h"
#include "nocopyable.h"
#include "TcpConnection.h"
#include "eventloopthreadpool.h"
#include "eventloop.h"
#include <memory>
#include <functional>
namespace skylu{
    class EventLoopThreadPool;
class TcpServer :Nocopyable , public std::enable_shared_from_this<TcpServer> {
        class Acceptor: Nocopyable{
        public:
            typedef std::function<void(Socket::ptr sock)>NewConnectionCallback;
            Acceptor(Eventloop *loop,const Address::ptr & listenAddr);
            ~Acceptor() = default;


            void setNewConnectionCallback(const NewConnectionCallback &cb){
                m_connection_cb = cb;
            }
            bool isListening()const {return m_isListening;}
            void listen();

            Socket::ptr getSocket(){return m_socket;}

        private:
            void handleRead();

        private:
            Eventloop * m_loop;
            Socket::ptr m_socket;
            Channel m_acceptChannel;
            NewConnectionCallback  m_connection_cb;
            bool m_isListening;


        };
    public:

        TcpServer(Eventloop * loop,const Address::ptr& address,const std::string &name);
        ~TcpServer() {}
        void start();
        void setConnectionCallback(const TcpConnection::ConnectionCallback &cb){m_connection_cb = cb;}
        void setMessageCallback(const TcpConnection::MessageCallback &cb){m_message_cb = cb;}
        void setCloseCallback(const TcpConnection::CloseCallback& cb){m_close_cb = cb;}
        void setThreadNum(int numThreads);

    private:
        typedef std::map<std::string,TcpConnection::ptr> ConnectionMap;
        void newConnection(Socket::ptr socket);
        // 线程安全
        void removeConnection(const TcpConnection::ptr& conn);

        //非线程安全
        void removeConnectionInLoop(const TcpConnection::ptr & conn);



    private:
        Eventloop *m_loop;
        const std::string m_name;
        std::unique_ptr<Acceptor> m_acceptor;
        TcpConnection::ConnectionCallback m_connection_cb;
        TcpConnection::CloseCallback  m_close_cb;
        TcpConnection::MessageCallback m_message_cb;
        std::atomic_bool isStart;
        ConnectionMap m_connections;

        std::unique_ptr<EventLoopThreadPool> m_threadpool;







    };

}


#endif //HASHTEST_TCPSERVER_H
