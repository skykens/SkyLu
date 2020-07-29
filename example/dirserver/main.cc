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
  if(argc < 3){
    SKYLU_LOG_FMT_DEBUG(G_LOGGER,"your argc = %d,usgae host port",argc);
    return -1;
  }
  EventLoop g_loop;
  Address::ptr addr = IPv4Address::Create(argv[1],atoi(argv[2]));
  DirServer server(&g_loop,addr,"dirserver" + std::string(argv[1])+":"+std::string(argv[2]));

  server.run();
  return 0;
}

