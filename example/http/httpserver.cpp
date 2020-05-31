//
// Created by jimlu on 2020/5/24.
//

#include "httpserver.h"
#include "address.h"
namespace skylu{
    HttpServer::HttpServer(EventLoop *loop,Address::ptr address,const std::string & name) :
            m_loop(loop)
            ,m_server(loop,address,name)
            ,m_name(name)
            ,isStart(false){


        m_server.setConnectionCallback(std::bind(&HttpServer::doNewConnection,this,std::placeholders::_1));
        m_server.setMessageCallback(std::bind(&HttpServer::doRequest,this,std::placeholders::_1,std::placeholders::_2));


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


    void HttpServer::doRequest(const TcpConnection::ptr &conne, Buffer *buf) {
        HttpRequest request(buf);
        if(request.parseRequest()){

           // ++m_count_timers[conne.get()];
            HttpResponse response(request.getVersion(),200,request.getPath(),request.isKeepAlive());

            Buffer tmpbuf = std::move(response.initResponse());
            conne->send(&tmpbuf);
            int timeout = request.getTimeout();
            if(timeout == 0){
                conne->shutdown();
            }else if(timeout > 0){
                m_loop->runAfter(static_cast<double>(timeout),std::bind(&HttpServer::closeConnection,this,conne));
            }


        }else{
            HttpResponse response(request.getVersion(),400,"",request.isKeepAlive());

            Buffer tmpbuf = std::move(response.initResponse());
            conne->send(&tmpbuf);
            conne->shutdown();
           // m_count_timers.erase(conne.get());
        }







    }

    void HttpServer::doNewConnection(const TcpConnection::ptr &conne) {

     //   EventLoop * loop = conne->getLoop();
     //   loop->runAfter(5,std::bind(&HttpServer::closeConnection,this,conne));
 //       assert(m_count_timers.find(conne.get()) == m_count_timers.end());
    //    m_count_timers[conne.get()] = 0 ;

    }

    void HttpServer::closeConnection(const TcpConnection::ptr &conne) {
        conne->shutdown();

    }



}
