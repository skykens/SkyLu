/*************************************************************************
	> File Name: fdmanager.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 13时33分30秒
    管理打开的文件描符(线程安全)
 ************************************************************************/

#ifndef _FDMANAGER_H
#define _FDMANAGER_H

#include <vector>
#include "nocopyable.h"
#include "singleton.h"
#include "file.h"
#include "mutex.h"
namespace skylu {
    class Fdmanager : Nocopyable {

    public:
        /*
         * @brief 构造函数
         */
        Fdmanager() {
            m_fds.resize(64);
        }

        /*
         * @brief 默认析构函数
         * @descri 当智能指针的引用次数为0的时候会释放File
         */
        ~Fdmanager() = default;

        /*
         * @brief 从容器中取出fd
         * @param [in] fd: 文件描述符
         * @param [in] isCreate: 当没有的时候是否自动创建
         * @descri
         */
        File::ptr get(int fd, bool isCreate = false);

        /*
         * @brief 删除fd
         * @param[in] fd: 文件描述符
         */
        void del(int fd);

        /*
         * @brief 打开文件
         */
        File::ptr open(const std::string & path,int flags =0);

    private:
        std::vector<File::ptr> m_fds;
        Mutex m_mutex;

    public:
        /*
         * @brief 智能指针的单例模式
         */
        typedef SingletonPtr<Fdmanager> FdMagr;

    };
}
#endif
