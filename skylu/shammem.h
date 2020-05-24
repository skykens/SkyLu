//
// Created by jimlu on 2020/5/17.
//

#ifndef HASHTEST_SHAMMEM_H
#define HASHTEST_SHAMMEM_H


#include <errno.h>
#include <string>
#include <stdio.h>
#include <iostream>
#include <sys/shm.h>
#include "log.h"
namespace skylu{

class Shammem {

public:
    /**
     * @brief 初始化共享内存
     * @param key
     * @param size 共享内存大小
     * @param [out] exist 共享内存是否存在
     * @param dump 生成dump文件（暂时没用）
     * @ret 返回共享内存地址
     *
     */
    static void * Init(key_t key,size_t size,bool &exist, bool dump = true);
    /**
     * @brief 删除共享内存
     * @param key
     * @ret 成功true
     */
    static bool Delete(key_t key);

};
}


#endif //HASHTEST_SHAMMEM_H
