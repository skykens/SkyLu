//
// Created by jimlu on 2020/7/24.
//

#ifndef SHAMQUEUE_H
#define SHAMQUEUE_H

#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include "log.h"
class ShamQueue {

public:
  ShamQueue(int key, int blksize, int blocks);
  ShamQueue();
  static ShamQueue *GetinterfaceRecv(int key,int blksize,int blocks);
  static ShamQueue *GetinterfaceSend(int key,int blksize,int blocks);
  virtual ~ShamQueue();

  //创建和销毁
  bool init(int key, int blksize, int blocks);
  void destroy(void);
  static void Destroy(int key); //静态删除共享内存方法

  // 打开和关闭
  bool open(int key, int blksize, int blocks);
  void close(void);

  //读取和存储
  void write(const void *buf);
  void read(void *buf);


protected:

  static  ShamQueue* m_recvFIFO;
  static  ShamQueue *m_sendFIFO;
  //进程控制信息块
  bool m_open;
  void *m_shmhead;		// 共享内存头部指针
  char *m_payload;			// 有效负载的起始地址

};

#endif // MQBUSD_SHAMQUEUE_H
