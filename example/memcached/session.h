//
// Created by jimlu on 2020/5/26.
//

#ifndef HASHTEST_SESSION_H
#define HASHTEST_SESSION_H
#include <iostream>
#include <memory>
#include "skylu/base/nocopyable.h"
#include "skylu/base/bytestring.h"
#include "skylu/net/buffer.h"
#include "skylu/net/tcpconnection.h"
#include <string>

namespace skylu{
    class MemcachedServer;
class Session :Nocopyable{
  inline const std::string serverError(const std::string & message){return "SERVER_ERROR"+message+"\r\n";}
  inline const std::string  clientError(const std::string & message){return "CLIENT_ERROR"+message+"\r\n";}
  static const char * kNOT_FOUND ;
  static  const char * kSTORED ;
  static const char * kNOT_STORED;
  static const char * kEXISTS;
  static const char * kDELETED;

  const char * kEND = "END\r\n";
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
    Session(MemcachedServer * owner);
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
    MemcachedServer* m_owner;
    TcpConnection::ptr m_conne;
    parseState m_state;
    ByteString cmd;
    std::vector<ByteString> keys;
    ByteString key; //用于doUpdate
    ByteString value;
    int m_bytes;
    int m_flag;
    Buffer m_output;
    char * recvBegin;
    char *recvEnd;

};
}


#endif //HASHTEST_SESSION_H
