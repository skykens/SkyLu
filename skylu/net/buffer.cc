//
// Created by jimlu on 2020/5/21.
//


#include "buffer.h"
#include <sys/uio.h>
#include <errno.h>

namespace skylu{


    ssize_t Buffer::readFd(int fd, int *saveError) {
        char extrabuf[65536];  //64k 的空间，已经能够容纳千兆网在50微妙下的全速收到的数据了
        struct iovec vec[2];

        const size_t writeable = writeableBytes();
        vec[0].iov_base = begin() + m_wpos;
        vec[0].iov_len = writeable;
        vec[1].iov_base = extrabuf;
        vec[1].iov_len = sizeof(extrabuf);


        //当容量总购大的时候就不需要extrabuf
        const int count = (writeable < sizeof(extrabuf)) ? 2: 1;

        const  ssize_t n = ::readv(fd,vec,count);

        if(n<0){
            *saveError = errno;
        }else if(static_cast<size_t>(n)<= writeable){
            hasWriteten(n);
        }else{
            hasWriteten(writeable);
            append(extrabuf,n-writeable);
        }
        return n;



    }
    ssize_t Buffer::readFd(int fd, int *saveError, const Address::ptr& addr) {
      // TODO 后续可以看看UDP下面有没有类似于TCP的readv这样多个缓冲区同时拷贝的
      const size_t writeable = writeableBytes();
      char extrabuf[65536 + writeable];  //64k 的空间，已经能够容纳千兆网在50微妙下的全速收到的数据了

      socklen_t  len = addr->getAddrLen();
      const ssize_t  n = ::recvfrom(fd,extrabuf,sizeof(extrabuf),0,addr->getAddr(),&len);

      if(n<0){
        *saveError = errno;
      }else{
        append(extrabuf,n);
      }
      return n;;
    }

    }