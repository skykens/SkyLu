//
// Created by jimlu on 2020/5/21.
//

#ifndef HASHTEST_CONNECTOR_H
#define HASHTEST_CONNECTOR_H

#include "nocopyable.h"

#include "eventloop.h"

#include "address.h"
#include <functional>
#include <algorithm>


namespace skylu{

    class Connector :Nocopyable{
    public:
        typedef std::function<void(int socket)> NewConnectionCallback;
        Connector(Eventloop *loop,const Address::ptr addr);
        ~Connector();
        void setNewConnectionCallback(NewConnectionCallback &cb){m_newconnection_cb = cb;}
        void start();
        void restart();
        void stop();

    private:
        NewConnectionCallback  m_newconnection_cb;
    };
}


#endif //HASHTEST_CONNECTOR_H
