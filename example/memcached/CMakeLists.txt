cmake_minimum_required(VERSION 3.10)
project(skylu_memcached)
set(CMAKE_CXX_STANDARD 11)


#优化后的版本 如果不是发布版本的话需要加上-g 和-O0
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O2  -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -fno-omit-frame-pointer")
aux_source_directory(. DIR_SRCS)
add_subdirectory(../../skylu skylu.out)
include_directories(../../)

add_executable(skylu_memcached main.cpp ${DIR_SRCS})
target_link_libraries(skylu_memcached skylu pthread)
