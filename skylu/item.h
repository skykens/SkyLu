//
// Created by jimlu on 2020/5/26.
//

#ifndef HASHTEST_ITEM_H
#define HASHTEST_ITEM_H

#include <memory>
#include "nocopyable.h"
#include "bytestring.h"

#include "hash.hpp"
namespace skylu{

class Item : Nocopyable{
public:
    typedef std::shared_ptr<Item> ptr;
    static Item::ptr makeItem(const ByteString & key,const ByteString & value,int flag,size_t hash);
    Item(const ByteString & key);
    void setValue(const ByteString & value);
    inline void setFlag(int flag){ m_flag = flag;}
    void clear();
    inline ByteString getKey(){return m_key;}
    inline char * getValue(){return m_data;}
    inline int getFlag(){return m_flag;}
    inline size_t getValueLen(){return m_data_len;}
    inline size_t hash()const{ return hashCode;}
   static inline size_t hash(const ByteString & key) {
        return hash_range(key.getBegin(), key.getEnd());
    }

    ~Item();
private:
    Item(const ByteString & key,const ByteString &value,int falg,size_t hashcode);


private:
    ByteString m_key;
    char* m_data;
    int m_flag;
    size_t m_data_len;
    bool isMalloc;
    size_t hashCode;





};

}

#endif //HASHTEST_ITEM_H
