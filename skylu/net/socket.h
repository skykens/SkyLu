/*************************************************************************
	> File Name: socket.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 17时48分41秒
    socket封装类 包含socket的操作
 ************************************************************************/

#ifndef _SOCKET_H
#define _SOCKET_H
#include <memory>
#include <stdio.h>
#include <iostream>
#include <netinet/tcp.h>
#include <errno.h>
#include <netinet/in.h>
#include <functional>
#include <algorithm>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "address.h"
#include "../base/file.h"
#include "../base/fdmanager.h"
#include "../base/nocopyable.h"
#include "../base/log.h"

namespace skylu{
class Socket : public std::enable_shared_from_this<Socket>,Nocopyable{
public:



    typedef std::shared_ptr<Socket> ptr;
    enum Type{
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM
    };

    enum Family{
        IPv4 = AF_INET,
        IPv6 = AF_INET6
    };

    /**
     * @brief 创建TCP连接 IPv4
     */
    static Socket::ptr CreateTCP(Address::ptr address);
    /*
     * @brief 创建UDP连接 IPv4
     */
    static Socket::ptr CreateUDP(Address::ptr address);

    /**
     *
     * @brief 创建Socket
     * @param[in] family 协议族
     * @param[in] type 类型： TCP/UDP
     * @param[in] protocol 默认为0
     */
     Socket(int family,int type,int protocol = 0);
    /**
     * @brief 关闭文件套接字
     */
    virtual ~Socket();

    /**
     * @brief 接收connect连接
     * @ret 返回客户端的socket 失败则nullptr
     */
    virtual Socket::ptr accept();
    /**
     * @brief 绑定端口
     */
    virtual bool bind(const Address::ptr addr);
    /**
     * @brief 连接
     * @param[in] addr: 目的地址
     * @param[in] timeoutMS: 超时时间
     *
     */
    virtual bool connect(const Address::ptr addr,uint64_t timeoutMS = -1);

    /**
     * @brief 监听socket
     * @param[in] backlog 队列长度 默认即可
     */
    virtual bool listen(int backlog = SOMAXCONN);

    /**
     * @brief 关闭文件套接字（在析构中调用）
     */
    virtual bool close();




    /**
     * @brief 获取socket配置
     * @param[in] level:设置的网络层
     * @param[in] optname: 目标选项
     * @param[out] optval:  返回值
     * @param[in] size: 存放返回值的空间大小
     * @ret 成功true
     */
    bool getoption(int optname,void *optval,size_t size,int level = SOL_SOCKET);

    /**
     * @brief 设置socket配置
     * @param[in] level: 设置的网络层
     * @param[in] optname: 目标选项
     * @param[in] optval: 欲设置的值 
     * @param[in] size: 欲设置的值的大小
     * @ret 成功true
     */
    bool setoption(int optname,const void *optval,size_t size,int level = SOL_SOCKET);

    /**
     * @brief 发送
     */
    virtual ssize_t send(const void * buff, size_t size);

    /**
     * @brief 发送到目的地址
     */
    virtual ssize_t sendTo(const void *buff,size_t size,const Address::ptr target,int flags = 0);
    /**
     * @brief 接收 
     */
    virtual ssize_t recv(void *buff,size_t size);

    /**
     * @brief 接收某个地址的内容
     */

    virtual ssize_t recvFrom(void* buff,size_t size,Address::ptr from,int flags = 0);


    /**
     * @brief 返回当前socket本端地址和对端地址. 无法使用于bind后的套接字
     *
     */
    Address::ptr getLocalAddress();
    Address::ptr getRemoteAddress();


    /**
     * @brief 是否连接
     */
    inline bool isConnected() const {return m_isConnected;}

    /**
     * @brief 当前socket是否有效
     */
    inline bool isValid() const {return m_isVaild;}


    /**
     * @brief 返回socket
     */
    inline int getSocket()const {return m_fd;}

    /**
     * @brief 设置为非阻塞
     */
    void setNonblock();

    /**
     * 关闭读取
     */
    void shoutdownWriting();



    /**
     * 设置是否发送心跳包
     * @param on 开启
     */
    void setKeepAlive(bool on);


    /**
     * 设置是否关闭Nagle算法
     * @param on
     */
    void setTcpNoDelay(bool on);

    bool isTcpSocket() const{return m_type == TCP;}

private:


    /**
     * @brief 创建新的socket
     */
     void newSocket();

    /**
     *
     * @brief 初始化socket 的选项
     */
     void initOption();

     /**
      * @brief 根据已经创建的socket 进行初始化
      */
      bool init(int fd);



private:
    int m_fd;
    int m_family;
    int m_type;
    int m_protocol;
    bool m_isVaild;
    bool m_isConnected;
    bool m_isWriting;

    Address::ptr m_LocalAddress; //本地地址 
    Address::ptr m_RemoteAddress;//远程地址 用于连接

};
}
#endif
