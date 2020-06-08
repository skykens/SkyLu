cmake_minimum_required(VERSION 3.10)

aux_source_directory(base SRC_BASE)
aux_source_directory(net SRC_NET)

add_library(skylu ${SRC_BASE} ${SRC_NET})