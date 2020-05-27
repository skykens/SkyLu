cmake_minimum_required(VERSION 3.10)
project(hashtest)
set(CMAKE_CXX_STANDARD 11)


set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -g -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -fno-omit-frame-pointer")
aux_source_directory(. DIR_SRCS)
add_subdirectory(skylu)

add_executable(hashtest main.cpp)
target_link_libraries(hashtest skylu pthread)
