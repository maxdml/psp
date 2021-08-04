cmake_minimum_required (VERSION 3.5.1)
project (Persephone C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/scripts/cmake")

include(dpdk)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(RELEASE_CFLAGS "-Wno-register -O3 -mtune=intel -DNDEBUG" CACHE STRING "C compiler options for release builds")
set(DEBUG_CFLAGS "-ggdb -O0 -fno-omit-frame-pointer -frecord-gcc-switches -lasan -fsanitize=address " CACHE STRING "C compiler options for debug builds")
set(CMAKE_C_FLAGS_DEBUG ${DEBUG_CFLAGS})
set(CMAKE_CXX_FLAGS_DEBUG ${DEBUG_CFLAGS})
set(CMAKE_C_FLAGS_RELEASE ${RELEASE_CFLAGS})
set(CMAKE_CXX_FLAGS_RELEASE ${RELEASE_CFLAGS})

set(CMAKE_VERBOSE_MAKEFILE ON)

# default CFLAGS
set(COMMON_CFLAGS "-ggdb -fstack-protector -fPIC -Wall -Wextra -Wno-deprecated-declarations -Wno-uninitialized -Wno-unused-parameter -Wno-register -Wl,-zdefs -fvisibility=hidden -I/usr/include/boost -Wno-unused-variable -Wno-empty-body")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CFLAGS} -std=c++17 -D__STDC_LIMIT_MACROS")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)
