//
// Created by jimlu on 2020/5/26.
//

#include "item.h"
#include <assert.h>
#include <malloc.h>
namespace skylu{
    Item::ptr Item::makeItem(const ByteString &key, const ByteString &value, int flag,size_t hash) {

        return Item::ptr(new Item(key,value,flag,hash));



    }
    Item::Item(const ByteString &key)
        :m_key(key)
        ,isMalloc(false){
        hashCode = hash_range(m_key.getBegin(),m_key.getEnd());
    }

    Item::Item(const ByteString &key, const ByteString &value, int flag,size_t hashcode)
        :m_data(static_cast<char*>(::malloc(value.getLen())))
        ,m_flag(flag)
        ,m_data_len(value.getLen())
        ,isMalloc(true)
        ,hashCode(hashcode){

            memcpy(m_data,value.getData(),value.getLen());

            char * src = static_cast<char *>(::malloc(key.getLen()));
            memcpy(src,key.getData(),key.getLen());
            m_key.set(src,key.getLen());
    }
    Item::~Item() {
        if(isMalloc)
            clear();
        }

        void Item::clear() {
            assert(isMalloc);

            if(m_key.getData()){
                ::free(const_cast<char*>(m_key.getData()));
            }
            m_key.reset();
            if(m_data){
                ::free(m_data);
            }
            m_data = nullptr;
            m_data_len = 0;

        }


    void Item::setValue(const ByteString &value) {
        if(m_data){
            ::free(m_data);
        }
        m_data = static_cast<char*>(::malloc(value.getLen()));
        assert(m_data != NULL);
        m_data_len = value.getLen();
        memcpy(m_data,value.getData(),m_data_len);
    }





}
