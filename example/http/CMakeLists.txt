cmake_minimum_required(VERSION 2.8.12.2)
set(CMAKE_CXX_STANDARD 11)
aux_source_directory(. DIR_LIB_SRCS)

add_library(skylu ${DIR_LIB_SRCS})