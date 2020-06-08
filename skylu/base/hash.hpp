//
// Created by jimlu on 2020/5/26.
//

#ifndef UNTITLED_HASH_H
#define UNTITLED_HASH_H

#include <vector>
#include <functional>
#include <sys/types.h>
namespace skylu {
    template<typename SizeT>
    inline void hash_combine_impl(SizeT &seed, SizeT value) {
        seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }


    template<class T>
    inline void hash_combine(std::size_t &seed, T const &v) {
        std::hash<T> hasher;
        return hash_combine_impl(seed, hasher(v));
    }

    inline std::size_t hash_range(
            std::vector<bool>::iterator first,
            std::vector<bool>::iterator last) {
        std::size_t seed = 0;

        for (; first != last; ++first) {
            hash_combine<bool>(seed, *first);
        }

        return seed;
    }

    inline std::size_t hash_range(
            std::vector<bool>::const_iterator first,
            std::vector<bool>::const_iterator last) {
        std::size_t seed = 0;

        for (; first != last; ++first) {
            hash_combine<bool>(seed, *first);
        }

        return seed;
    }

    inline void hash_range(
            std::size_t &seed,
            std::vector<bool>::iterator first,
            std::vector<bool>::iterator last) {
        for (; first != last; ++first) {
            hash_combine<bool>(seed, *first);
        }
    }

    inline void hash_range(
            std::size_t &seed,
            std::vector<bool>::const_iterator first,
            std::vector<bool>::const_iterator last) {
        for (; first != last; ++first) {
            hash_combine<bool>(seed, *first);
        }
    }

    template<class It>
    inline std::size_t hash_range(It first, It last) {
        std::size_t seed = 0;

        for (; first != last; ++first) {
            hash_combine(seed, *first);
        }

        return seed;
    }

    template<class It>
    inline void hash_range(std::size_t &seed, It first, It last) {
        for (; first != last; ++first) {
            hash_combine(seed, *first);
        }
    }
}




#endif //UNTITLED_HASH_H
