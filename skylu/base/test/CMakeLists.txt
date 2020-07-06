cmake_minimum_required(VERSION 2.8.12)

project(TEST)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_CFLAGS} -g -std=c++11 -Wall")

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

include_directories(${GTEST_BOTH_LIBRARIES})

add_executable(test_threadpool test_threadpool.cc)
target_link_libraries(test_threadpool ${GTEST_BOTH_LIBRARIES})
target_link_libraries(test_threadpool ${GTEST_BOTH_LIBRARIES})

add_test(Test test_threadpool)
enable_testing()