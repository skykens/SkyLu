/*************************************************************************
	> File Name: fd_manager.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月24日 星期五 10时40分24秒
    对文件状态、操作的封装
 ************************************************************************/

#ifndef _FD_MANAGER_H
#define _FD_MANAGER_H
#include <memory>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <fcntl.h>
#include "log.h"
namespace skylu {

    class File : public std::enable_shared_from_this<File> {
    public:
        enum OPTION {
            RECVTIME = 0,
            SENDTIME
        };
        typedef std::shared_ptr<File> ptr;

        File() = default;
        /*
         * @brief 会在这里构造成员变量
         */
        File(int fd);


        ~File();

        int getFd() const {return m_fd;}

        bool isInit() const { return m_isInit; }

        bool isSocket() const { return m_isSocket; }

        bool isClose() const { return m_isClosed; }

        /*
         * @brief 设置FD为非阻塞IO
         */
        void setNonblock();

        /*
         * @brief 是否为阻塞IO
         */
        bool getNonblock() const { return m_isNonblock; }

        /*
         * @brief 设置超时时间 目前仅仅是改变成员变量中的值
         * @type 接收or发送
         * @v 超时时间
         */
        void setTimeout(OPTION type, uint64_t v);

        /*
         * @brief 获取超时时间
         * @type 接收or发送
         * @ret 对应成员变量的值
         */
        uint64_t getTimeout(OPTION type);



        /*
         * @brief 按照行来读取文件
         */
        void readAllLine(std::vector<std::string>& data);

        /*
         * @brief 从文件中读取一行
         */
        size_t readLine(std::string & data,size_t n);

        /*
         * @brief 向文件中追加一行
         */
        size_t writeNewLine(std::string & data);

        /*
         * @brief 判断文件是否存在
         */
        static bool isExits(const std::string & path);

        /*
         * @brief 查看文件大小
         */
        static size_t getFilesize(const std::string &path);

        /*
         * @brief 重命名
         */
        static bool rename(const std::string &path,const std::string &name);

        /*
         * @brief 删除文件
         */
        static bool remove(const std::string &path);






    private:
        /*
         * @brief 初始化相关成员变量的值 同时把fd设置为非阻塞IO
         * @desrc 在 File(int fd)中被调用
         */
        bool init();

        friend class Fdmanager;


    private:
        bool m_isInit;
        bool m_isSocket;
        bool m_isNonblock;
        bool m_isClosed;
        bool m_isOpen;
        int m_fd;
        uint64_t m_recvTimeout;
        uint64_t m_sendTimeout;
        struct stat m_stat;
        std::string m_path;


    };
}
#endif