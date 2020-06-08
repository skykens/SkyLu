/*************************************************************************
	> File Name: util.h
	> Author: 
	> Mail: 
	> Created Time: 2020年04月26日 星期日 14时12分47秒
    一些单独的API封装
 ************************************************************************/

#ifndef _UTIL_H
#define _UTIL_H
#include <iostream>
#include <execinfo.h>
#include <functional>
#include <sys/time.h>
#include <sched.h>
#include <sys/types.h>
#include <zconf.h>
#include <stdio.h>
#include <cstddef>
#include <dirent.h>
#include <syscall.h>
#include <vector>
#include <string.h>
namespace skylu {
/* 
 * @brief 调用syscall获取线程Id
 */
    pid_t getThreadId();

/*
 * @brief 获取协程Id
 */
    uint32_t getFiberId();

/*
 * @brief 获取当前毫秒级时间
 */
    uint64_t getCurrentMS();

/*
 * @brief 获取当前微妙级时间
 */
    uint64_t getCurrentUS();





/*
 * @brief 文件/文件夹相关操作
 *
 */
    struct FSUtil {
        /*
         * @brief 列出当前目录下的所有文件
         * @param[out] files :输出的文件名
         * @param[in] path:路径
         * @param[in] subfix: 后缀名
         */
        static void ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string &subfix);


    };


}
#endif
