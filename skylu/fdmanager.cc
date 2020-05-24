#include "fdmanager.h"
namespace skylu {


    //暂时去掉锁了
    File::ptr Fdmanager::get(int fd, bool isCreate) {
        if (fd < 0){
            return nullptr;
        }

      //  Mutex::Lock lock(m_mutex);
        if (fd < (int) m_fds.size()) {
                if (m_fds[fd])
                    return m_fds[fd];
                if (!isCreate)
                    return nullptr;
            }

        if (fd >= (int)m_fds.size()) {
            m_fds.resize(m_fds.size() * 1.5);
        }
        if (m_fds[fd])
            return m_fds[fd];
        if (isCreate) {
            m_fds[fd].reset(new File(fd));
            return m_fds[fd];
        } else {
            return nullptr;
        }

    }

    void Fdmanager::del(int fd) {
        if (fd < (int)m_fds.size()) {
            m_fds[fd].reset();
        }
    }
    File::ptr Fdmanager::open(const std::string& path,int flags) {
        int fd = ::open(path.c_str(),flags);
        if(fd < 0){
            SKYLU_LOG_ERROR(G_LOGGER) << "open fd("<<fd<<") errno="
                                      <<errno<<"   strerror="<<strerror(errno);
            return nullptr;
        }
        File::ptr ret = get(fd,true);
        ret->m_path = path;
        ret->m_isOpen = true;

        return ret;


    }
}
