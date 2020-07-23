cmake_minimum_required(VERSION 2.8.12)

aux_source_directory(base SRC_BASE)
aux_source_directory(net SRC_NET)
aux_source_directory(proto SRC_PROTO)

add_library(skylu ${SRC_BASE} ${SRC_NET} ${SRC_PROTO})