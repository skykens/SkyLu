//
// Created by jimlu on 2020/7/22.
//

#include <iostream>
#include "dirserver.h"
#include <skylu/net/eventloop.h>

#include <skylu/net/tcpclient.h>
using namespace  skylu;
int main(int argc,char ** argv)
{


  EventLoop g_loop;
  Address::ptr addr = IPv4Address::Create("127.0.0.1",6001);
  DirServer server(&g_loop,addr,"dirserver");

  server.run();
  return 0;
}

