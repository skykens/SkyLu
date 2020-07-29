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
  static const int kInitHeartBeatMs = 5000;
public:
  DirServer(EventLoop *owner, const Address::ptr &addr,
            const std::string &name,int HeartBeatMs = kInitHeartBeatMs);
  ~DirServer() = default;
  void run();

private:
  /**
   *
   */
  void init();
  /**
   * 更新注册表（ 增删 时调用）
   */
  void updateRegisterFile();
  void onMessage(const TcpConnection::ptr &, Buffer *buff);
  /**
   * 注册
   * @param conne
   */
  void handleRegister(const TcpConnection::ptr &conne,
                      const DirServerPacket *msg);

  /**
   * mqbusd 请求注册表
   * @param conne
   */
  void handleRequest(const TcpConnection::ptr &conne);
  /**
   * 心跳包处理
   * @param conne
   */
  void handleKeepAlive(const TcpConnection::ptr &conne);
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
  std::unique_ptr<File> m_register_conf; // 落地到文件
  std::unordered_map<std::string,std::string> m_conf; // 从文件中读出来的conneName- host:port地址
  std::unordered_map<int, TcpConnection::ptr> m_clients; // fd-tcpconnect =  MqServers
  std::unordered_map<int,Timestamp> m_clients_heartBeat;  //维护对应fd的上次的心跳时间
  const std::string kRegisterFileName;
  const std::string kRegisterBackup;
  const int m_ClientHeartBeatMs; ///客户端心跳时间
};


#endif // DIRSERVER_DIRSERVER_H
