cmake_minimum_required (VERSION 3.5.1)
project (CLIENT C CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options chrono context system)
include_directories(${Boost_INCLUDE_DIR})

add_executable(client ${CMAKE_CURRENT_SOURCE_DIR}/client.cc)
target_link_libraries(client psp-libos)
target_add_dpdk(client)

add_executable(nc ${CMAKE_CURRENT_SOURCE_DIR}/nc.cc)
target_link_libraries(nc psp-libos)
target_add_dpdk(nc)
