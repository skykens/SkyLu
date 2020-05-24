//
// Created by jimlu on 2020/5/16.
//

#ifndef KV_SHAMPOOL_H
#define KV_SHAMPOOL_H


#include <stddef.h>
#include <string.h>
#include <type_traits>
#include <sys/mman.h>
#include <errno.h>
#include <iostream>
#include "log.h"

namespace skylu
{
    static Logger::ptr g_shampool = SKYLU_LOG_NAME("system");
    static const size_t FENCE_SIZE = 4096;
    static const size_t MEM_PAGE_SIZE = 4096; //内存页大小
    static const size_t MEM_PAGE_ALIGN_CALC_SIZE = MEM_PAGE_SIZE - 1; // 对齐到4096， 用4095来计算
    static const size_t MP_MEM_POOL_BLOCK_INDEX_START = 10000; // block idx 开始

    template<bool OpenMProtect = true>
    class ShammemPool
    {

    public:
        static const size_t INVALID_POINTER = static_cast<size_t>(-1);

        typedef struct AllocBlock
        {
            /*
             * next说明：
             * 0 则表示为已分配使用（被使用指已经分配并且未在空闲链中）的Block
             * INVALID_POINTER 表示空闲链结尾
             * 否则 next 值用来链接空闲链，表示下一个空闲节点
             */
            size_t next;
            char node[0];
        } AllocBlock;

        static const size_t NODE_OFFSET = offsetof(AllocBlock, node);

    public:
        typedef struct MemHeader
        {
            size_t max_size; // 最大可分配大小
            uint32_t max_node; // 最大节点数
            size_t alloc_size; // 已分配大小
            uint32_t alloc_node; // 已分配节点数
            size_t node_size; // 节点实际大小
            size_t block_size; // 分配块大小
            time_t create_time; // 创建的时间
            size_t free_list; // 空闲链
        } MemHeader;

        ShammemPool() :
                m_header(NULL), m_data(NULL)
        {
            memset(m_error_msg_, 0, sizeof(m_error_msg_));
            static_assert(sizeof(MemHeader) % 8 == 0, "MemHeader size should be 8*N");
        }

        virtual ~ShammemPool()
        {
        }

        inline size_t Ref(void* p) const
        {
            return (size_t)((intptr_t)p - (intptr_t)(m_header));
        }

        inline size_t Ref2BlockIdx(void* p) const
        {
            size_t mem_offset = Ref(p);
            return MP_MEM_POOL_BLOCK_INDEX_START + ((mem_offset - GetHeaderSize()) / m_header->block_size);
        }

        inline void* Deref(size_t pos) const
        {
            return ((char*)m_header + pos);
        }

        inline void* DerefByBlockIdx(size_t block_idx) const
        {
            return ((char*)m_header + GetHeaderSize() + m_header->block_size * (block_idx - MP_MEM_POOL_BLOCK_INDEX_START));
        }

        /**
         * 计算分配max_node个节点至少需要多少内存
         *
         * @param max_node 最多可能同时分配多少个节点
         * @param node_size 每个节点多大
         * @return 需要的字节数
         */
        static size_t GetAllocSize(uint32_t max_node, size_t node_size)
        {
            return GetHeaderSize() + max_node * GetBlockSize(node_size);
        }


        inline const MemHeader* Header() const
        {
            return m_header;
        }

        inline const char* GetErrorMsg() const
        {
            return m_error_msg_;
        }

        int32_t Init(void* mem, uint32_t max_node, size_t node_size, bool check_header = false)
        {
            if(!mem)
            {
                snprintf(m_error_msg_, sizeof(m_error_msg_), "mem argument invalid, it is nullptr");
                return -1;
            }
            m_header = (MemHeader*)mem;
            if(!check_header)
            {
                // 初始化内存头
                m_header->max_size = GetAllocSize(max_node, node_size);
                m_header->max_node = max_node;
                m_header->alloc_size = GetHeaderSize();
                m_header->alloc_node = 0;
                m_header->node_size = node_size;
                m_header->block_size = GetBlockSize(node_size);
                m_header->create_time = time(NULL);
                m_header->free_list = INVALID_POINTER;
                m_data = (char*)m_header + GetHeaderSize();

                SKYLU_LOG_FMT_INFO(g_shampool,"MPROTECT : m_data :%p",m_data);
            }
            else
            {
                if(!CheckHeader(max_node, node_size, m_header))
                {
                    snprintf(m_error_msg_, sizeof(m_error_msg_), "mem pool header invalid");
                    std::cout << "shm pool header invalid" << std::endl;
                    return -1;
                }

                m_data = (char*)m_header + GetHeaderSize();

                SKYLU_LOG_FMT_INFO(g_shampool,"MPROTECT : m_data :%p",m_data);
            }

            // 调用mprotect设置栅栏，将内存块最后的4k设置为不可访问
            if(OpenMProtect)
            {
                char * block = (char *)m_data;
                while(block + m_header->block_size <= (char*)m_header + m_header->max_size)
                {
                    char * protect_addr = (char*)block + m_header->block_size - MEM_PAGE_SIZE;
                    int32_t ret = mprotect(protect_addr, FENCE_SIZE, PROT_NONE);
                    if(0 != ret)
                    {
                        snprintf(m_error_msg_, sizeof(m_error_msg_), "init mprotect failed: p=%p, errno:%d, err_msg:%s",
                                 protect_addr, errno, strerror(errno));
                        return -2;
                    }

                    block += m_header->block_size;
                }
            }


            return 0;
        }

        void* Alloc(bool zero = true)
        {
            AllocBlock* p = GetFreeBlock();
            if(!p){
                // 没有找到空闲块，需要从池中分配内存
                if(m_header->alloc_size + m_header->block_size <= m_header->max_size){
                    p = (AllocBlock*)Deref(m_header->alloc_size);
                    m_header->alloc_size += m_header->block_size;
                    ++m_header->alloc_node;
                }else{
                    snprintf(m_error_msg_, sizeof(m_error_msg_), "Alloc错误,空间已满");
                }
            }
            else if(!IsValidBlock(p)){
                snprintf(m_error_msg_, sizeof(m_error_msg_), "Alloc错误,空闲块非法%p,"
                                                         "header=%p,max_size=%zu,alloc_size=%zu,block_size=%zu", p, m_header,
                         m_header->max_size, m_header->alloc_size, m_header->block_size);
                p = NULL;
            }

            if(!p)
                return NULL;

            if(zero)
            {
                memset(p, 0, OpenMProtect ? m_header->block_size - MEM_PAGE_SIZE : m_header->block_size);
            }

            p->next = 0;

            return p->node;
        }

        int32_t Free(void* node)
        {
            AllocBlock* block = GetBlock(node);

            if(IsUsedBlock(block))
            {
                LinkFreeBlock(block);
            }
            else
            {
                snprintf(m_error_msg_, sizeof(m_error_msg_), "Free释放指针错误,指针%p为非法指针,"
                                                         "header=%p,max_size=%zu,alloc_size=%zu,block_size=%zu", node, m_header,
                         m_header->max_size, m_header->alloc_size, m_header->block_size);
                return -1;
            }

            return 0;
        }

    protected:
        static size_t GetHeaderSize()
        {
            static size_t m_headersize = OpenMProtect ?
                                        ((sizeof(MemHeader) + MEM_PAGE_ALIGN_CALC_SIZE) & ~MEM_PAGE_ALIGN_CALC_SIZE) :
                                        ((sizeof(MemHeader) + 7) & ~7);

            return m_headersize;
        }

        /**
         *
         * @param node_size
         * @return  返回块大小
         */
        static size_t GetBlockSize(size_t node_size)
        {
            static size_t block_size = OpenMProtect ?
                                       ((node_size + NODE_OFFSET + MEM_PAGE_SIZE + MEM_PAGE_ALIGN_CALC_SIZE) & ~ MEM_PAGE_ALIGN_CALC_SIZE) :
                                       ((node_size + NODE_OFFSET + 7) & ~7);

            return block_size;
        }

        size_t GetBlockSize()
        {
            return GetBlockSize(m_header->node_size);
        }

        static bool CheckHeader(uint32_t max_node, size_t node_size, MemHeader* header)
        {
            size_t max_size = GetAllocSize(max_node, node_size);
            // 验证内存头部信息是否正确
            if((header->node_size != node_size) || (header->block_size != GetBlockSize(node_size))
               || (header->max_node != max_node) || (header->max_size != max_size)
               || (GetHeaderSize() + header->alloc_node * header->block_size
                   != header->alloc_size) || (header->alloc_size > max_size))
            {
//            std::cout<<"check header: "<<header->node_size<<", "<<node_size<<";"<<header->block_size<<", "<<GetBlockSize(node_size)<<";"
//                    <<header->max_node<<", "<<max_node<<";"<<header->max_size<<", "<<max_size<<";"
//                    <<GetHeaderSize() + header->alloc_node * header->block_size<<", "<<header->alloc_size<<";"
//                    <<header->alloc_size<<", "<<max_size<<std::endl;
                return false;
            }

            return true;
        }

        bool IsValidBlock(const AllocBlock* block) const
        {
//        return !(block < m_data
//                || (size_t)block + m_header->block_size > (size_t)m_header + m_header->alloc_size
//                || Ref(const_cast<AllocBlock*>(block)) % m_header->block_size
//                        != sizeof(MemHeader) % m_header->block_size);
//        TRACE_LOG(0, 0, 0, "", "block:%p, ref:%zu, block_size:%zu, sizeof header:%zu",
//                block, Ref(const_cast<AllocBlock*>(block)), m_header->block_size, sizeof(MemHeader));
            if (block < m_data)
            {
                //DEBUG_LOG(0, 0, 0, "", "block:%p, date_:%p", block, m_data);
                return false;
            }

            if ((size_t)block + m_header->block_size > (size_t)m_header + m_header->alloc_size)
            {
                //DEBUG_LOG(0, 0, 0, "", "block_end_size:%zu, alloc_size:%zu",
                  //        (size_t)block + m_header->block_size, (size_t)m_header + m_header->alloc_size);
                return false;
            }

            if ( Ref(const_cast<AllocBlock*>(block)) % m_header->block_size
                 != GetHeaderSize() % m_header->block_size)
            {
                //DEBUG_LOG(0, 0, 0, "", "mod:%zu, block_size:%zu",
                  //        Ref(const_cast<AllocBlock*>(block)) % m_header->block_size,
                    //      GetHeaderSize() % m_header->block_size);

                return false;
            }

            return true;
        }

        bool IsUsedBlock(const AllocBlock* block) const
        {
            return (IsValidBlock(block) && block->next == 0);
        }

        /**
         * @brief  连接数据块到链表头部（头插）
         * @param p
         */
        void LinkFreeBlock(AllocBlock* p)
        {
            p->next = m_header->free_list;
            m_header->free_list = Ref(p);
        }

        AllocBlock* GetFreeBlock()
        {
            AllocBlock* p = NULL;
            if(m_header->free_list == INVALID_POINTER)
            {
                p = NULL;
            }
            else
            {
                p = (AllocBlock*)Deref(m_header->free_list);
                m_header->free_list = p->next;
            }

            return p;
        }

    public:
        static AllocBlock* GetBlock(const void* node)
        {
            return (AllocBlock*)((char*)node - NODE_OFFSET);
        }

        static void * GetNode(const AllocBlock * block)
        {
            return (char *)block + NODE_OFFSET;
        }

        const AllocBlock* GetFirstBlock() const
        {
            const AllocBlock* block = (AllocBlock*)m_data;
            return (IsValidBlock(block) ? block : NULL);
        }

        const AllocBlock* GetNextBlock(const AllocBlock* block) const
        {
            if(!block)
                return GetFirstBlock();

            const AllocBlock* next = (AllocBlock*)((char*)block + m_header->block_size);
            return (IsValidBlock(next) ? next : NULL);
        }

        const AllocBlock* GetNextUsedBlock(const AllocBlock* block = NULL) const
        {
            while((block = GetNextBlock(block)))
            {
                if(block->next == 0)
                {
                    return block;
                }
            }

            return NULL;
        }

        const void* GetNextNode(const void* node = NULL)
        {
            const AllocBlock* block = NULL;
            if(node)
            {
                block = GetBlock(node);
            }

            block = GetNextUsedBlock(block);

            if(!block)
                return NULL;
            else
                return block->node;
        }

        bool IsValidNode(const void* node)
        {
            return IsValidBlock((const AllocBlock*)node);
        }

    protected:
        MemHeader* m_header;
        void* m_data;
        char m_error_msg_[256];
    };

}



#endif //KV_SHAMPOOL_H
