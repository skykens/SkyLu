#include "address.h"
namespace skylu{


IPv4Address::IPv4Address(uint32_t address,uint32_t port){
    memset(&m_addr, 0,sizeof(m_addr));
    m_addr.sin_family = Address::IPv4;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = htonl(address);

}
sockaddr* IPv4Address::getAddr() {
    return (sockaddr*)&m_addr;
}

const sockaddr* IPv4Address::getAddr() const {
    return (sockaddr*)&m_addr;
}


void IPv4Address::insert(std::ostream& os) const {
    uint32_t addr = ntohl(m_addr.sin_addr.s_addr);
    os<< ((addr >> 24) & 0xff) << "."
       << ((addr >> 16) & 0xff) << "."
       << ((addr >> 8) & 0xff) << "."
       << (addr & 0xff);
    os << ":" << ntohs(m_addr.sin_port);
}

uint32_t IPv4Address::getPort() const {
    return ntohs(m_addr.sin_port);
}

void IPv4Address::setPort(uint32_t v) {
    m_addr.sin_port = htons(v);
}

int IPv4Address::getFamily() const {
    return m_addr.sin_family;
}
}
