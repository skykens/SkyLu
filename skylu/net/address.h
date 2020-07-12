/*************************************************************************
	> File Name: address.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 18时52分51秒
    网络地址的封装
 ************************************************************************/

#ifndef _ADDRESS_H
#define _ADDRESS_H
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <sstream>
namespace skylu{


class Address{
public:
    typedef std::shared_ptr<Address> ptr;
    enum Family{
        IPv4 = AF_INET,
        IPv6 = AF_INET6
    };
    Address() = default;

    std::string toString() const{

        std::stringstream ss;
        insert(ss);
        return ss.str();
    }
    
    /*
     * @brief 可读性输出地址
     */
    virtual void insert(std::ostream& os)const = 0;
    virtual uint32_t getPort() const = 0;
    virtual  void setPort(uint32_t v) = 0;
    virtual const sockaddr * getAddr() const = 0;
    virtual socklen_t getAddrLen()const =0;
    virtual socklen_t getAddrLen() = 0;
    virtual sockaddr * getAddr() = 0;
    virtual std::string getAddrForString() = 0;
    virtual int getFamily()const = 0;
    virtual ~Address() = default;
};

class IPv4Address: public Address{
public:
    typedef std::shared_ptr<IPv4Address> ptr;

    /*
     * @brief 创建IPv4Address
     * @param[in] address 点分十进制 127.0.0.1
     * @param[in] port 
     * @ret 失败返回nullptr
     */
    static IPv4Address::ptr Create(const char * address,uint32_t port = 0){
      IPv4Address::ptr tmp(new IPv4Address());
      tmp->m_addr.sin_port = htons(port);
      if(address != nullptr){
        tmp->m_addr.sin_addr.s_addr = inet_addr(address);
      }
      tmp->m_addr.sin_family = AF_INET;
      return tmp;
    }

    /*
     * @brief 通过sockaddr_in构造
     */
    explicit IPv4Address(const sockaddr_in &addr){
        m_addr = addr;
    }

    /*
     * @brief 通过二进制地址构造
     */
    explicit IPv4Address(uint32_t address = INADDR_ANY , uint32_t port = 0);

    /*
     * @brief 获取sockaddr_in
     */
    const sockaddr * getAddr()const override;
    sockaddr* getAddr() override;
    socklen_t getAddrLen()const override {return sizeof(m_addr);}
    socklen_t getAddrLen() override  {return sizeof(m_addr);}

    /*
     * @brief 将地址重定向输出到os
     */
    void insert(std::ostream& os)const override;

    /*
     * @brief 获取端口
     */
    uint32_t getPort()const override;
    /*
     * @brief 设置端口
     */
    void setPort(uint32_t v) override;

    /*
     * @brief 获取协议族
     */
    int getFamily()const override;


    std::string getAddrForString(){return std::string(inet_ntoa(m_addr.sin_addr));}






private:
    sockaddr_in m_addr{};

        
};
}
#endif
