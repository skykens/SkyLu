//
// Created by jimlu on 2020/5/20.
//

#ifndef HASHTEST_TCPCONNECTION_H
#define HASHTEST_TCPCONNECTION_H

#include <memory>
#include "eventloop.h"
#include "address.h"
#include "socket.h"
#include "nocopyable.h"
#include "buffer.h"
#include "channel.h"
#include "Timestamp.h"
#include <string>
namespace skylu{
class TcpConnection :Nocopyable,public std::enable_shared_from_this<TcpConnection>{
        enum State{
            kConnceting, //初始状态
            kConnected,  //connectEstablished之后的
            kdisConnecting,  //调用shutdown之后的状态
            kdisConnected //调用了handleClose之后
        };
    public:
        typedef std::shared_ptr<TcpConnection> ptr;
        typedef std::function<void(const TcpConnection::ptr&)> ConnectionCallback;
        typedef std::function<void(const TcpConnection::ptr&,Buffer *,Timestamp)> MessageCallback;
        typedef std::function<void(const TcpConnection::ptr&)> CloseCallback;
        typedef std::function<void(const TcpConnection::ptr&)> WriteCompleteCallback;
        typedef std::function<void(const TcpConnection::ptr& ,size_t)> HighWaterMarkCallback;
        TcpConnection(Eventloop * loop,Socket::ptr socket,const std::string&name);
        ~TcpConnection() = default;

        /**
         * 设置各类回调函数
         * @param cb
         */
        void setConnectionCallback(ConnectionCallback &cb){m_connection_cb = cb;}
        void setMessageCallback(MessageCallback &cb){m_message_cb = cb;}
        void setCloseCallback(CloseCallback &cb){m_close_cb = cb;}
        void setWriteCompleteCallback(WriteCompleteCallback &cb){m_writecomplete_cb = cb;}
        void setHighWaterCallback(HighWaterMarkCallback &cb,size_t len){m_highwater_cb = cb;m_highMark = len;}


        /**
         * 获得输出输出缓冲区
         * @return
         */
        Buffer * getInputBuffer(){return &m_input_buffer;}
        Buffer* getOutputBuffer(){return &m_output_buffer;}



        const std::string getName()const {return m_name;}
        Eventloop * getLoop(){return m_loop;}
        const std::string getStateForString()const{
            switch (m_state) {
#define XX(state) \
            case State::state :\
                return #state; \
                break;

                XX(kdisConnected);
                XX(kdisConnecting);
                XX(kConnected);
                XX(kConnceting);
#undef XX
                default:
                    return "UNKNOW STATUS";

            }
        }


        void connectEstablished();
        void connectDestroyed();

        void send(const void *message,size_t len);
        void send(Buffer *buf);


        void shutdown();



    private:

        /**
         *  各类IO事件触发后的回调函数 （绑定在channel）
         * @param receiveTime
         */
        void handleRead(Timestamp receiveTime);
        void handleWrite();
        void handleClose();
        void handleError();

        void sendInLoop(const void *data,size_t len);
        void shutdownInLoop();

        void setState(State s){m_state = s;}


    private:
        State m_state;
        Eventloop *m_loop;
        Socket::ptr m_socket;
        Channel::ptr m_channel;
        ConnectionCallback m_connection_cb;
        MessageCallback m_message_cb;
        CloseCallback  m_close_cb;
        HighWaterMarkCallback m_highwater_cb; //只会在写入操作触发EAGIN时触发一次
        const std::string m_name;
        size_t m_highMark; //超出的部分
        WriteCompleteCallback m_writecomplete_cb;
        Buffer m_input_buffer;   //存放从socket读进来的内容
        Buffer m_output_buffer;  //存放向socket写入超出来的内容(即触发EAGAIN后面的数据)








    };

}



#endif //HASHTEST_TCPCONNECTION_H