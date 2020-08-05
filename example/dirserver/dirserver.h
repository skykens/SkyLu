//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_DIRSERVER_H
#define DIRSERVER_DIRSERVER_H
#include <unordered_set>
#include <list>
#include <unordered_map>
#include <skylu/net/eventloop.h>
#include <skylu/net/tcpserver.h>
#include <skylu/proto/dirserver_proto.h>

using namespace skylu;
class DirServer {
public:
  DirServer(EventLoop *owner, const Address::ptr &addr,
            const std::string &name,int HeartBeatMs = kCheckKeepAliveMs);
  ~DirServer() = default;
  void run();

private:
  /**
   *
   */
  void init();
  void onMessage(const TcpConnection::ptr &, Buffer *buff);
  /**
   * 注册 (心跳包 )
   * @param conne
   */
  void handleRegister(const TcpConnection::ptr &conne,
                      Buffer * buff);

  /**
   * mqbusd 请求注册表
   * @param conne
   */
  void handleRequest(const TcpConnection::ptr &conne);
  /**
   * 心跳包处理 (废弃)
   * @param conne
   */
  void handleKeepAlive(const TcpConnection::ptr &conne,Buffer * buff);
  /**
   * 发送确认包
   * @param conne
   */
  void sendAck(const TcpConnection::ptr &conne);
  /**
   * 心跳监听 (定时器
   * 非线程安全
   */
  void listenClientWithMs();

private:
  EventLoop *m_owner;
  TcpServer m_server;
  HostAndTopicsMap m_clients_info; // 维护clients的info (broker 下有什么主题 )
  std::unordered_map<int,std::string > m_link_info_to_clients; /// fd- host:port (m_clients_info 的key)
  std::unordered_map<int, TcpConnection::ptr> m_clients; // fd-tcpconnect =  MqServers
  std::unordered_map<int,Timestamp> m_clients_heartBeat;  //维护对应fd的上次的心跳时间
  const int m_ClientHeartBeatMs; ///客户端心跳时间
};


#endif // DIRSERVER_DIRSERVER_H
