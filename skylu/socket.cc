#include "socket.h"
#include <iostream>
namespace skylu{


Socket::ptr Socket::CreateTCP(skylu::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), TCP));
    sock->newSocket();
    return sock;
}

Socket::ptr Socket::CreateUDP(skylu::Address::ptr address) {
    Socket::ptr sock(new Socket(address->getFamily(), UDP));
    sock->newSocket();
    sock->m_isConnected = true;
    return sock;
}


Socket::Socket(int family,int type,int protocol)
    :m_fd(-1)
    ,m_family(family)
    ,m_type(type)
    ,m_protocol(protocol)
    ,m_isVaild(false)
    ,m_isConnected(false)
{

}
bool Socket::init(int fd){


    File::ptr file = Fdmanager::FdMagr::GetInstance()->get(fd,true);
    if(file&&file->isSocket()&&!file->isClose()){

        m_fd = fd;
        m_isVaild = true;
        m_isConnected = true;
        initOption();
        getLocalAddress();
        getRemoteAddress();
        return true;
    }
    return false;
    
    
}
void Socket::initOption(){
    int ret =1;
    setoption(SO_REUSEADDR,(void*)&ret,sizeof(ret));
    if(m_type == SOCK_STREAM) {
        setoption(TCP_NODELAY,(void*)&ret,sizeof(ret),IPPROTO_TCP);

    }
    
}




void Socket::newSocket(){
    m_fd = socket(m_family,m_type,m_protocol);
    if(m_fd < 0){
        SKYLU_LOG_ERROR(G_LOGGER)<< "socket("<< m_fd<<") error ="
            << errno<<"strerr ="<<strerror(errno);
        return ;
    }
    initOption();
    m_isVaild = true;
}

Socket::ptr Socket::accept(){
    Socket::ptr val(new Socket(m_family,m_type,m_protocol));
    int fd = ::accept(m_fd,nullptr ,nullptr);
    if(val->init(fd))
        return val;
    SKYLU_LOG_ERROR(G_LOGGER)<<"accept ("<<fd << ")  error ="
        << errno <<" strerr ="<<strerror(errno);
    return nullptr;
}

bool Socket::bind(const Address::ptr addr){
    if(!m_isVaild)
        newSocket();
    if(addr->getFamily()!=m_family){

        SKYLU_LOG_ERROR(G_LOGGER)<<"addr->getFamily("
            <<addr->getFamily()<<")  != m_family("
            <<m_family<<")";
        return false;
    }
    if(::bind(m_fd,(struct sockaddr *)addr->getAddr(),addr->getAddrLen())){
        SKYLU_LOG_ERROR(G_LOGGER)<<"bind("<<m_fd<<")"
            <<" error ="<<errno
            <<"  strerr ="<<strerror(errno);
        return false;
    }
    getLocalAddress();
    return true;
}

bool Socket::listen(int backlog){
    if(!m_isVaild){
        SKYLU_LOG_ERROR(G_LOGGER)<<"listen("<<m_fd<<")";
        return false;
    }
    
    if(::listen(m_fd,backlog)){
        SKYLU_LOG_ERROR(G_LOGGER)<<"listen("<<m_fd<<")"
            <<"  error ="<<errno
            <<"  strerr ="<<strerror(errno);
        return false;

    }
    return true;
}

bool Socket::connect(const Address::ptr addr,uint64_t timeoutMS){
    
    if(!m_isVaild){
        newSocket();
        if(!m_isVaild){
            SKYLU_LOG_ERROR(G_LOGGER)<<"connect("<<m_fd<<")";
            return false;
            
        }

    }
    if(::connect(m_fd,addr->getAddr(),addr->getAddrLen())){
        SKYLU_LOG_ERROR(G_LOGGER)<<"connect( "<<m_fd
            <<") error = "<<errno
            <<"  strerror="<<strerror(errno);
        return false;
    }

    getLocalAddress();
    getRemoteAddress();
    m_isConnected = true;
    return true;

    
}
bool Socket::close(){
    if(::close(m_fd)== 0)
    {
        m_isVaild = false;
        m_isConnected = false;
        return true;
    }
    SKYLU_LOG_ERROR(G_LOGGER)<<"close("<<m_fd
        <<")  error="<<errno
        <<"   strerror="<<strerror(errno);
    return false;
}



bool Socket::getoption(int optname,void *optval,size_t size,int level){
    
    if(::getsockopt(m_fd, level, optname, optval, (socklen_t*)&size)){
        SKYLU_LOG_DEBUG(G_LOGGER) << "getoption sock=" << m_fd
            << " level=" << level << " option=" << optname
            << " errno=" << errno << " strerror=" << strerror(errno);
        return false;
    }
    return true;


}

bool Socket::setoption(int optname,const void *optval,size_t size,int level){
    if(::setsockopt(m_fd, level, optname, optval, (socklen_t)size)) {
        SKYLU_LOG_DEBUG(G_LOGGER) << "setoption sock=" << m_fd
           << " level=" << level << " option=" << optname
            << " errno=" << errno << " strerror=" << strerror(errno);
        return false;
    }
    return true;
}



void Socket::setNonblock(){
    if(!m_isVaild)
    {
        newSocket();
        if(!m_isVaild){
            SKYLU_LOG_ERROR(G_LOGGER)<<"setNonblock("<<m_fd<<")";
            return ;

        }
    }
    Fdmanager::FdMagr::GetInstance()->get(m_fd,true)->setNonblock();
}


size_t Socket::send(const void *buff,size_t size){
    if(m_isConnected && m_isVaild)
    {
        return ::send(m_fd,buff,size,0);
    }
    SKYLU_LOG_DEBUG(G_LOGGER)<<"m_isConnected="<<m_isConnected
        <<"   m_isVaild="<<m_isVaild;
    return -1;
}



size_t Socket::sendTo(const void *buff,size_t size,const Address::ptr target,int flags){

    if(m_isConnected && m_isVaild){
        return ::sendto(m_fd,buff,size,flags,target->getAddr(),target->getAddrLen());
    }
    SKYLU_LOG_DEBUG(G_LOGGER)<<"m_isConnected="<<m_isConnected
        <<"   m_isVaild="<<m_isVaild;
    return -1;
}

size_t Socket::recv(void *buff,size_t size) {
    if (m_isConnected && m_isVaild) {
        return ::recv(m_fd, buff, size, 0);
    }

    SKYLU_LOG_DEBUG(G_LOGGER) << "m_isConnected=" << m_isConnected
                              << "   m_isVaild=" << m_isVaild;
    return -1;


}

size_t Socket::recvFrom(void *buff,size_t size,Address::ptr from,int flags){

    if(m_isConnected && m_isVaild){
        socklen_t  len = from->getAddrLen();
        return ::recvfrom(m_fd,buff,size,flags,from->getAddr(),&len);
    }

    SKYLU_LOG_DEBUG(G_LOGGER)<<"m_isConnected="<<m_isConnected
        <<"   m_isVaild="<<m_isVaild;
    return -1;
}



    Address::ptr Socket::getRemoteAddress() {
        if(m_RemoteAddress) {
            return m_RemoteAddress;
        }

        Address::ptr result;
        switch(m_family) {
            case AF_INET:
                result.reset(new IPv4Address());
                break;
        }
        socklen_t addrlen = result->getAddrLen();
        if(getpeername(m_fd, result->getAddr(), &addrlen)) {
            SKYLU_LOG_ERROR(G_LOGGER) << "getpeername error sock=" << m_fd
                << " errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        m_RemoteAddress = result;
        return m_RemoteAddress;
    }

    Address::ptr Socket::getLocalAddress() {
        if(m_LocalAddress) {
            return m_LocalAddress;
        }

        Address::ptr result;
        switch(m_family) {
            case AF_INET:
                result.reset(new IPv4Address());
                break;
        }
        socklen_t addrlen = result->getAddrLen();
        if(getsockname(m_fd, result->getAddr(), &addrlen)) {
            SKYLU_LOG_ERROR(G_LOGGER) << "getsockname error sock=" << m_fd
                                      << " errno=" << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        m_LocalAddress = result;
        return m_LocalAddress;
    }

    void Socket::setKeepAlive(bool on) {
    int opt = on ? 1:0;
    ::setsockopt(m_fd,SOL_SOCKET,SO_KEEPALIVE,&opt, static_cast<socklen_t>(sizeof(opt)));

    }

    void Socket::setTcpNoDelay(bool on) {

        int optval = on ? 1 : 0;
        ::setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY,
                     &optval, static_cast<socklen_t>(sizeof optval));
    }

    void Socket::shoutdownWriting() {
    if(::shutdown(m_fd,SHUT_WR)<0){
        SKYLU_LOG_ERROR(G_LOGGER)<<"socket("<<m_fd<<") shut downWrite";
    }


    }






Socket::~Socket() {
    close();
}








}
