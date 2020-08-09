//
// Created by jimlu on 2020/8/4.
//

#ifndef MQBUSD_CONSISTENTHASH_HPP
#define MQBUSD_CONSISTENTHASH_HPP
#include <map>
#include <string>
#include <list>
#include <functional>
#include <algorithm>

namespace  skylu {
struct vnode_t{
  ///虚拟节点
  vnode_t() = default;
  vnode_t(const std::string &src,const std::string &name,int id)
      :realIp(src),virtualIp(src +":" + std::to_string(id)),conneName(name){}

  std::string realIp; ///实际主机节点地址
  std::string virtualIp;
  std::string conneName; /// optional
};

struct MurMurHash{

  uint32_t operator()(const std::string &str){
    const char * key = str.c_str();
    int len =str.size();
    const uint32_t m = 0x5bd1e995;
    const int32_t r = 24;
    const int32_t seed = 97;
    uint32_t h = seed ^len;
    // Mix 4 bytes at a time into the hash
    const auto *data = (const unsigned char *) key;
    while (len >= 4) {
      uint32_t k = *(uint32_t *) data;
      k *= m;
      k ^= k >> r;
      k *= m;
      h *= m;
      h ^= k;
      data += 4;
      len -= 4;
    }
    // Handle the last few bytes of the input array
    switch (len) {
    case 3:
      h ^= data[2] << 16;
    case 2:
      h ^= data[1] << 8;
    case 1:
      h ^= data[0];
      h *= m;
    };
    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;
    return h;
  }
  uint32_t operator() (const vnode_t & v) {
    return operator()(v.virtualIp);
  }
  typedef uint32_t result_type;
};
template <typename T = vnode_t , typename Hash  = MurMurHash,
          typename Alloc =
              std::allocator<std::pair<const typename Hash::result_type, T>>>
class consistent_hash_map {
public:
  typedef typename Hash::result_type size_type;
  typedef std::map<size_type, T, std::less<size_type>, Alloc> map_type;
  typedef typename map_type::value_type value_type;
  typedef value_type &reference;
  typedef const value_type &const_reference;
  typedef typename map_type::iterator iterator;
  typedef typename map_type::reverse_iterator reverse_iterator;
  typedef Alloc allocator_type;

public:
  consistent_hash_map() = default;

  ~consistent_hash_map() {}

public:
  std::size_t size() const { return nodes_.size(); }

  bool empty() const { return nodes_.empty(); }

  std::pair<iterator, bool> insert(const T &node) {
    size_type hash = hasher_(node);
    return nodes_.insert(value_type(hash, node));
  }

  void erase(iterator it) { nodes_.erase(it); }

  std::size_t erase(const T &node) {
    size_type hash = hasher_(node);
    return nodes_.erase(hash);
  }

  iterator find(size_type hash) {
    if (nodes_.empty()) {
      return nodes_.end();
    }

    iterator it = nodes_.lower_bound(hash);

    if (it == nodes_.end()) {
      it = nodes_.begin();
    }

    return it;
  }

  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }
  reverse_iterator rbegin() { return nodes_.rbegin(); }
  reverse_iterator rend() { return nodes_.rend(); }

private:
  Hash hasher_;
  map_type nodes_;
};

}


#endif // MQBUSD_CONSISTENTHASH_HPP
