//
// Created by jimlu on 2020/7/22.
//

#ifndef DIRSERVER_DIRSERVER_H
#define DIRSERVER_DIRSERVER_H
#include <unordered_set>
#include <set>
#include <unordered_map>
#include <skylu/net/eventloop.h>
#include <skylu/net/tcpserver.h>
#include <skylu/proto/dirserver_proto.h>

using namespace skylu;
class DirServer{
  static const char * kRegisterFileName;
  static const char * kRegisterBackup;
public:


  DirServer(EventLoop *owner, const Address::ptr &addr,
            const std::string &name);
  ~DirServer() = default;
  void run();





private:
  /**
   *
   */
  void init();
  /**
   * 加载注册表
   */
  void loadFromRegisterFile();
  /**
   * 更新注册表（ 增删 时调用）
   */
  void updateRegisterFile();
  void onMessage(const TcpConnection::ptr&,Buffer *buff);
  /**
   * 注册
   * @param conne
   */
  void handleRegister(const TcpConnection::ptr &conne);

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
  void sendAck(const TcpConnection::ptr & conne);
  /**
   * 心跳监听 (定时器
   */
  void listenClientWithMs();


private:
  typedef std::pair<std::string,int> HostPair;
  typedef std::pair<Timestamp,int> Entry;
  EventLoop *m_owner;
  TcpServer m_server;
  std::unique_ptr<File> m_register_conf;
  std::unordered_map<int,TcpConnection::ptr> m_clients;

  std::set<HostPair> m_conf;

};

#endif // DIRSERVER_DIRSERVER_H
