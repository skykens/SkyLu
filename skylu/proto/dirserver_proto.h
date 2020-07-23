//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_DIRSERVER_PROTO_H
#define DIRSERVER_DIRSERVER_PROTO_H
enum{
  COMMAND_REGISTER = 0,
  COMMAND_REQUEST,
  COMMAND_ACK,
  COMMAND_KEEPALIVE,


};

struct DirServerPacket{
  char command;
};
#endif // DIRSERVER_DIRSERVER_PROTO_H
