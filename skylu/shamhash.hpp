//
// Created by jimlu on 2020/5/17.
//

#ifndef HASHTEST_SHAMHASH_HPP
#define HASHTEST_SHAMHASH_HPP

#include "shampool.hpp"
#include "shammem.h"
#include <unordered_map>
#include "log.h"

namespace skylu {
    static Logger::ptr g_shamhash = SKYLU_LOG_NAME("system");

    template <typename KeyType, typename ValueType, bool OpenMProtect = false>
    class Shamhash
    {
    public:
        static const int32_t SUCCESS = 0;
        static const int32_t ERROR = -1;


        typedef struct HashNode
        {
            KeyType key;
            ValueType value;
        } HashNode;

    public:

        Shamhash() = default;

        virtual ~ Shamhash() = default;

        /**
         * @brief 获取将节点地址转换为下标
         * @param p   节点地址
         * @return  下标
         */

        inline size_t ref(void* p) const
        {
            return m_mem_pool.Ref(p);
        }


        /**
         * @brief  将下标转换为节点地址
         * @param pos  下标
         * @return  节点地址
         */
        inline void* deref(size_t pos) const
        {
            return m_mem_pool.Deref(pos);
        }


        /**
         * @brief 初始化 (使用之前必须调用），重建m_map
         * @param mem    共享内存地址
         * @param max_node  最大的节点个数
         * @param check_header  是否检查头部结构体
         * @return  0:成功
         *
         */
        int32_t init(void* mem, uint32_t max_node, bool check_header)
        {
            if(!mem)
            {
                SKYLU_LOG_ERROR(g_shamhash)<<"Init Fail.mem = NULL";
                return -1;
            }

            int ret = m_mem_pool.Init(mem, max_node, sizeof(HashNode), check_header);
            if(ret != 0)
            {
                SKYLU_LOG_FMT_ERROR(g_shamhash,"Init失败,m_mem_pool.Init失败,ret=%d,error=%s", ret, m_mem_pool.GetErrorMsg());
                return -2;
            }

            //重建索引
            HashNode * node = NULL;
            while(check_header && (node = (HashNode *)m_mem_pool.GetNextNode(node)))
            {
                if(GetValue(node->key))
                {
                  SKYLU_LOG_ERROR(g_shamhash)<<"重建索引失败, key is exit";
                    return -3;
                }

                m_map.insert(std::make_pair(node->key, &node->value));
            }

            return 0;
        }

        /**
         * @brief 在共享内存中建立一个索引
         * @param key
         * @return  返回的是值所存储的地址，通过这个地址在共享内存中存入value
         */
        ValueType* Alloc(const KeyType & key)
        {
            if(GetValue(key))
            {
                SKYLU_LOG_ERROR(g_shamhash)<<"error=key已经存在";
                return NULL;
            }

            HashNode * node = (HashNode *)m_mem_pool.Alloc();
            if(!node)
            {
                SKYLU_LOG_FMT_ERROR(g_shamhash, "m_mem_pool.Alloc失败,Alloc() return NULL,error=%s", m_mem_pool.GetErrorMsg());
                return NULL;
            }

            memcpy(&node->key, &key, sizeof(KeyType));

            //construct the hash map for quick search a node with a key
            m_map.insert(std::make_pair(key, &node->value));

            return &node->value;
        }

        /**
         * @brief  删除key
         * @param key
         * @return  0:成功 -1:key不存在
         */
        int32_t erase(const KeyType & key)
        {
            ValueType * value = GetValue(key);
            if(!value)
            {
                SKYLU_LOG_ERROR(g_shamhash)<<"Free 失败,error=key不存在";
                return -1;
            }
            if(m_mem_pool.Free(GetNode(value)) != 0)
            {
                 SKYLU_LOG_ERROR(g_shampool)<<"m_mem_pool.Free失败,error="<<m_mem_pool.GetErrorMsg();
                return -2;
            }

            m_map.erase(key);

            return 0;
        }

        /**
         * @brief 插入值
         * @param key
         * @param value
         * @return  返回值存放的地址  失败为NULL
         */
        ValueType* insert(const KeyType & key, const ValueType & value)
        {
            ValueType* alloc = Alloc(key);
            if(!alloc)
            {
                return NULL;
            }

            memcpy(alloc, &value, sizeof(ValueType));

            return alloc;
        }

        /**
         * 得到value
         * @param key
         * @return 值
         */
        ValueType* get(const KeyType & key)
        {
            return GetValue(key);
        }

        /**
         *
         * @param max_node  最大节点数
         * @return  存放max_node数量的节点需要多大的共享内存
         */
        static size_t getAllocSize(uint32_t max_node)
        {
            return ShammemPool<OpenMProtect>::GetAllocSize(max_node, sizeof(HashNode));
        }

    protected:


        /**
         *
         * @param value 值
         * @return  返回hash节点的时地址（实际就是偏移了一下）
         */
        HashNode * GetNode(const ValueType * value)
        {
            if(value)
                return (HashNode*)((char*)value - offsetof(HashNode, value));
            return NULL;
        }



        /**
         * 获取value
         * @param key
         * @return value
         */
        ValueType * GetValue(const KeyType & key)
        {
            typename std::unordered_map<KeyType, ValueType *>::iterator itr = m_map.find(key);
            if(itr != m_map.end())
                return itr->second;

            return NULL;
        }

    protected:
        ShammemPool<OpenMProtect> m_mem_pool; //共享内存池
        std::unordered_map<KeyType, ValueType *> m_map; //在init中会被重建
    };

}


#endif //HASHTEST_SHAMHASH_HPP
