cmake_minimum_required (VERSION 3.5.1)
project (PSPAPP C CXX)

set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options chrono context system)
include_directories(${Boost_INCLUDE_DIR})

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(psp-app ${APP_DIR}/psp_app.cc)
target_link_libraries(psp-app psp-libos)
# Not sure why we have to specify the link flags here too (they are already in psp-libos)
target_add_dpdk(psp-app)
