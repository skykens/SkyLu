//
// Created by jimlu on 2020/5/21.
//

#ifndef HASHTEST_CONNECTOR_H
#define HASHTEST_CONNECTOR_H

#include "../base/nocopyable.h"

#include "eventloop.h"

#include "socket.h"
#include "address.h"
#include <functional>
#include <memory>
#include <assert.h>

#include <algorithm>


namespace skylu{

/**
 * 连接器
 */
class Connector :public std::enable_shared_from_this<Connector>,Nocopyable{
  static const int kMaxRetryDelayMs = 30*1000;
  static const int kInitRetryDelayMs = 500;
    public:
  enum  State{
    kDisconnected,kConnecting,kConnected
  };
  typedef std::shared_ptr<Connector> ptr;
        typedef std::function<void(const Socket::ptr &)> NewConnectionCallback;
        Connector(EventLoop *loop,const Address::ptr& addr);
        ~Connector();
        void setNewConnectionCallback(const NewConnectionCallback &cb){m_newconnection_cb = cb;}
        const Address::ptr & getServerAddress()const {return m_server_addr;}
        void start();
        void restart();
        void stop();
        const State getState()const {return m_state;}

      private:
        void setState(State e ){
          m_state = e;
        }

        void startInLoop();
        void stopInLoop();
        void connect();
        void connecting();
        void handleWrite();
        void handleError();
        void retry();
        void removeAndResetChannel();
        void resetChannel();



    private:
  EventLoop * m_loop;
      Address::ptr m_server_addr;
      bool enableConnect; // 允许连接
      State m_state;
      std::unique_ptr<Channel> m_channel;
      Socket::ptr m_socket;

      NewConnectionCallback  m_newconnection_cb;
      int m_retryDelayMs;
    };
}


#endif //HASHTEST_CONNECTOR_H
