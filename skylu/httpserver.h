//
// Created by jimlu on 2020/5/24.
//

#ifndef HASHTEST_HTTPSERVER_H
#define HASHTEST_HTTPSERVER_H

#include "httpresponse.h"
#include "httprequest.h"
#include "tcpserver.h"
#include "eventloop.h"
#include <unordered_map>
#include "timerid.h"
namespace skylu{
class HttpServer {
public:

    HttpServer(Eventloop *loop,Address::ptr address,const std::string & name);
    ~HttpServer() = default;
    void setThreadNum(int num);
    void run();

private:
    void doRequest(const TcpConnection::ptr& conne,Buffer * buf);
    void doNewConnection(const TcpConnection::ptr &conne);
    void closeConnection(const TcpConnection::ptr &conne);

private:
    Eventloop * m_loop;
    TcpServer m_server;
    std::string m_name;
    bool isStart;
    std::unordered_map<TcpConnection *,int64_t> m_count_timers;




};
}


#endif //HASHTEST_HTTPSERVER_H
