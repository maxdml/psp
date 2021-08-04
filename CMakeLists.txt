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

set(FAKE_WORK_DIR $ENV{AE_DIR}/fake_work)
add_library(fake-work SHARED IMPORTED GLOBAL)
set_target_properties(fake-work PROPERTIES IMPORTED_LOCATION ${FAKE_WORK_DIR}/libfake.so)
function(target_add_fake_work TARGET)
    target_include_directories(${TARGET} PUBLIC ${FAKE_WORK_DIR})
    target_link_libraries(${TARGET} fake-work)
endfunction(target_add_fake_work)

# RocksDB flags
set(RDB_DIR ${CMAKE_SOURCE_DIR}/submodules/rocksdb)
set(RDB_LIBRARIES gflags snappy z lz4)
function(target_add_rocksdb TARGET)
    target_include_directories(${TARGET} PUBLIC ${RDB_DIR}/include/rocksdb)
    target_link_libraries(${TARGET} ${RDB_DIR}/librocksdb.a)
    target_link_libraries(${TARGET} ${RDB_LIBRARIES})
endfunction(target_add_rocksdb)

# default CFLAGS
set(COMMON_CFLAGS "-ggdb -fstack-protector -fPIC -Wall -Wextra -Wno-deprecated-declarations -Wno-uninitialized -Wno-unused-parameter -Wno-register -Wl,-zdefs -fvisibility=hidden -I/usr/include/boost -Wno-unused-variable -Wno-empty-body")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_CFLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_CFLAGS} -std=c++17 -D__STDC_LIMIT_MACROS")

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)
