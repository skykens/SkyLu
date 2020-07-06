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

#include "skylu/base/threadpool.h"
using namespace  skylu;
EventLoop * g_loop = new EventLoop();
int main(){
    Address::ptr addr = IPv4Address::Create(nullptr,3015);
    Signal::hook(SIGPIPE,[](){});

    MemcachedServer server(g_loop,addr,"memcached server");

    server.setThreadNums(0);
    server.run();

    return 0;

}