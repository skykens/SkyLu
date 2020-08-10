cmake_minimum_required(VERSION 3.10.2)
project(mqbroker)
set(CMAKE_CXX_STANDARD 11)


#优化后的版本 如果不是发布版本的话需要加上-g 和-O0
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -g  -O2 -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -fno-omit-frame-pointer")
aux_source_directory(. DIR_SRCS)
add_subdirectory(../../skylu skylu.out)
include_directories(../../)
include_directories(/usr/local/include/yaml-cpp)
set(YAML "/usr/local/include/yaml-cpp")
#添加头文件搜索路径
include_directories(/usr/local/include)
#添加库文件搜索路径
link_directories(/usr/local/lib64)


add_executable(mqbroker main.cc ${DIR_SRCS})
target_link_libraries(mqbroker skylu pthread libyaml-cpp.a)
