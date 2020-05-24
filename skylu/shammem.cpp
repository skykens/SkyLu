//
// Created by jimlu on 2020/5/17.
//

#include "shammem.h"
namespace skylu{
    void * Shammem::Init(key_t key, size_t size, bool &exist, bool dump) {

            exist = false;
            int shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0644);
            if(shmid < 0)
            {
                if(errno != EEXIST)
                {
                    SKYLU_LOG_FMT_ERROR(G_LOGGER,"shmget共享内存失败,error is not EEXIST,key=%d,size=%zu,errno=%d,err=%s",
                            key, size, errno, strerror(errno));
                    return NULL;
                }
                exist = true;
                shmid = shmget(key, size, 0644);
                if(shmid < 0)
                {
                    SKYLU_LOG_FMT_ERROR(G_LOGGER,"shmget 已存在共享内存失败,key=%d,size=%zu,errno=%d,err=%s",
                                        key, size, errno, strerror(errno));
                    return NULL;
                }
                SKYLU_LOG_FMT_INFO(G_LOGGER,"now the shm is reinitialized, key=%d, size=%zu", key, size);
            }
            void * p = shmat(shmid, NULL, 0);
            if(!p)
            {
                SKYLU_LOG_FMT_ERROR(G_LOGGER, "shmat共享内存失败,shmid=%d,key=%u,size=%zu,errno=%d,err=%s",
                        shmid, key, size, errno, strerror(errno));
                return NULL;
            }
            if(!exist)
            {
                bzero(p, size);
            }
            return p;
        }

        bool Shammem::Delete(key_t key) {

                int shmid = shmget(key, 0, 0644);
                if (shmid < 0) {
                    return false;
                }

                int ret = shmctl(shmid, IPC_RMID, NULL);

                return (-1 != ret);
            }
}

