#include "file.h"

namespace skylu{

File::File(int fd)
    :m_isInit(false)
    ,m_isSocket(false)
    ,m_isNonblock(false)
    ,m_isClosed(false)
    ,m_isOpen(false)
    ,m_fd(fd)
    ,m_recvTimeout(-1)
    ,m_sendTimeout(-1)
{

    init();

}
File::~File() {
    if(m_isOpen&&m_isSocket == false){
        if(close(m_fd)){
            SKYLU_LOG_ERROR(G_LOGGER)<<" close fd"<<m_fd<<"  path="<<m_path<<" errno ="<< errno
                            <<"   strerror="<<strerror(errno);
        }
    }
}

bool File::init(){
    if(m_isInit)
        return true;
    m_recvTimeout = -1;
    m_sendTimeout = -1;


    struct stat fd_stat;
    if(fstat(m_fd,&fd_stat) == -1){
        m_isInit = false;
        m_isSocket = false;
    }else{
        m_isInit = true;
        m_isSocket = S_ISSOCK(fd_stat.st_mode);
    }

    if(m_isSocket){
        int flags = fcntl(m_fd,F_GETFL,0);
        if(!(flags & O_NONBLOCK)){
            fcntl(m_fd,F_SETFL,flags | O_NONBLOCK);

        }
        m_isNonblock = true;
    }else{
        m_isNonblock = false;
    }


    m_isClosed = false;
    return m_isInit;

    
}

void File::setNonblock(){
    if(m_isNonblock)
        return ;
    int flags = fcntl(m_fd,F_GETFL,0);
    if(!(flags & O_NONBLOCK)){
            fcntl(m_fd,F_SETFL,flags | O_NONBLOCK);

    }
    m_isNonblock = true;

}

void File::setTimeout(OPTION type,uint64_t v){
    if(type == RECVTIME){
        m_recvTimeout = v;

    }else{
        m_sendTimeout = v;
    }
}

uint64_t File::getTimeout(OPTION type){
    if(type == RECVTIME){
        return m_recvTimeout;
    }else{
        return m_sendTimeout;
    }
}



void File::readAllLine(std::vector<std::string> &data) {
    std::ifstream read(m_path.c_str());
    std::string tmp;
    while(getline(read,tmp));
}

size_t File::readLine(std::string &data,size_t n) {
    std::ifstream read(m_path.c_str());
    std::string tmp;
    for(size_t i=0;i<n;i++){
        getline(read,tmp);
    }
    data = tmp;

    return tmp.size();

}

size_t File::writeNewLine(std::string &data) {
    std::ofstream write(m_path.c_str(),std::ios::app);
    write<<data;
    return data.size();

}


bool File::isExits(const std::string &path) {
    if(access(path.c_str(),F_OK|W_OK|R_OK) == 0){
        return true;
    }else{
        SKYLU_LOG_ERROR(G_LOGGER)<<" access errno ="<<errno
        <<"    strerrno="<<strerror(errno);
        return false;
    }


}


size_t File::getFilesize(const std::string &path) {
    struct stat filestat;

    if(stat(path.c_str(),&filestat)<0){
        SKYLU_LOG_ERROR(G_LOGGER)<<" getFilesize errno ="<<errno
                                 <<"    strerrno="<<strerror(errno);
        return -1;
    }
    return filestat.st_size;



}

bool File::remove(const std::string &path) {

    if(::remove(path.c_str())){
        SKYLU_LOG_ERROR(G_LOGGER)<<" remove errno ="<<errno
                                 <<"    strerrno="<<strerror(errno);
        return false;
    }
    return true;

}


bool File::rename(const std::string &path, const std::string &name) {

    if(::rename(path.c_str(),name.c_str())){
        SKYLU_LOG_ERROR(G_LOGGER)<<" rename errno ="<<errno
                                 <<"    strerrno="<<strerror(errno);
        return false;

    }
    return true;


}



}
