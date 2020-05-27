//
// Created by jimlu on 2020/5/26.
//

#ifndef HASHTEST_SESSION_H
#define HASHTEST_SESSION_H
#include "nocopyable.h"
#include <memory>
#include "bytestring.h"
#include <iostream>
#include "bytestring.h"
#include "buffer.h"
#include "TcpConnection.h"
#include <string>

#define CLIENT_ERROR(message) "CLIENT_ERROR "#message"\r\n"
#define SERVER_ERROR(message) "SERVER_ERROR "#message"\r\n"
#define NOT_FOUND "NOT_FOUND\r\n"
#define STORED "STORED\r\n"
#define NOT_STORED "NOT_STORED\r\n"
#define EXISTS "EXISTS\r\n"
#define DELETED "DELETED\r\n"
#define END "END\r\n"
namespace skylu{
    class MemcachedServer;
class Session :Nocopyable{
    enum parseState{
        init,
        recvKey,
        recvCmd,
        recvFlag,
        recvTimeout,
        recvBytes,
        recvValue,
        recvAll
    };

public:

    Session(MemcachedServer* owner,const TcpConnection::ptr & conne);
    void onMessage(const TcpConnection::ptr & conne,Buffer *buf);

    void doUpdate(Buffer * buf);
    void doDelete();

    void doFind();
    void reply(const char *src,size_t len);
    void reply(const std::string &message);
    void reply();
    void closeSession();
    ~Session();

private:
    std::unique_ptr<MemcachedServer> m_owner;
    TcpConnection::ptr m_conne;
    parseState m_state;
    ByteString cmd;
    std::vector<ByteString> keys;
    ByteString key; //用于doUpdate
    ByteString value;
    int m_bytes;
    int m_flag;

    Buffer m_output;

};
}


#endif //HASHTEST_SESSION_H
