//
// Created by jimlu on 2020/5/24.
//

#include "httpserver.h"
#include "address.h"
namespace skylu{
    HttpServer::HttpServer(Eventloop *loop,Address::ptr address,const std::string & name) :
            m_loop(loop)
            ,m_server(loop,address,name)
            ,m_name(name)
            ,isStart(false){
        m_server.setMessageCallback(std::bind(&HttpServer::doRequest,this
                            ,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
        m_server.setConnectionCallback(std::bind(&HttpServer::doNewConnection,this,std::placeholders::_1));


    }

    void HttpServer::setThreadNum(int num) {
        m_server.setThreadNum(num);
    }

    void HttpServer::run() {
        isStart = true;
        m_server.start();
        m_loop->loop();
        isStart = false;
    }


    void HttpServer::doRequest(const TcpConnection::ptr &conne, Buffer *buf, Timestamp time) {
        HttpRequest requese(buf);
        if(requese.parseRequest()){

            ++m_count_timers[conne.get()];
            HttpResponse response(200,requese.getPath(),requese.isKeepAlive());

            Buffer tmpbuf = std::move(response.initResponse());
            conne->send(&tmpbuf);


        }else{
            HttpResponse response(400,"",requese.isKeepAlive());

            Buffer tmpbuf = std::move(response.initResponse());
            conne->send(&tmpbuf);
            conne->shutdown();
            m_count_timers.erase(conne.get());
        }







    }

    void HttpServer::doNewConnection(const TcpConnection::ptr &conne) {
     //   Eventloop * loop = conne->getLoop();
     //   loop->runAfter(5,std::bind(&HttpServer::closeConnection,this,conne));
 //       assert(m_count_timers.find(conne.get()) == m_count_timers.end());
    //    m_count_timers[conne.get()] = 0 ;

    }

    void HttpServer::closeConnection(const TcpConnection::ptr &conne) {
        if(m_count_timers.find(conne.get()) == m_count_timers.end())
            return ;
        if(--m_count_timers[conne.get()] <= 0){
            SKYLU_LOG_INFO(G_LOGGER)<<"connection destroy";

        }
    }



}
