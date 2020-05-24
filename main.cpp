//
// Created by jimlu on 2020/5/17.
//

#include "skylu/timerqueue.h"
#include "skylu/Daemon.h"
#include "skylu/address.h"
#include "skylu/httpserver.h"

using namespace  skylu;
Eventloop * g_loop = new Eventloop();

int main(){
    Address::ptr addr = IPv4Address::Create("127.0.0.1",80);
    Signal::hook(SIGPIPE,[](){});
    HttpServer server(g_loop,addr,"http server");
    server.setThreadNum(5);
    server.run();




    return 0;

}