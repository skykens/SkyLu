//
// Created by jimlu on 2020/5/17.
//

#include "skylu/net/timerqueue.h"
#include "skylu/base/daemon.h"
#include "skylu/net/address.h"
#include "skylu/net/eventloop.h"
#include "memcachedserver.h"
#include "skylu/base/tree.h"

#include "skylu/base/btree.h"
using namespace  skylu;
EventLoop * g_loop = new EventLoop();
int main(){
    Address::ptr addr = IPv4Address::Create("127.0.0.1",3015);
    Signal::hook(SIGPIPE,[](){});
    //HttpServer server(g_loop,addr,"http server");
    //server.setThreadNum(5);
    //server.run();
    BTree tree;

    MemcachedServer server(g_loop,addr,"memcached server");

    server.setThreadNums(0);
    server.run();




    return 0;

}