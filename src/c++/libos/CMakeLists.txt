set(LIBOS_MAIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBOS_SU "${LIBOS_MAIN_DIR}/su/")
set(LIBOS_COMMON "${LIBOS_MAIN_DIR}/common/")
set(LIBOS_IO_UDP "${LIBOS_MAIN_DIR}/io/udp_dpdk/")
set(LIBOS_IO_LRPC "${LIBOS_MAIN_DIR}/io/lrpc/")
file(GLOB LIBOS_SRC
    "${LIBOS_SU}/*.cc" "${LIBOS_SU}/*.c"
    "${LIBOS_COMMON}/*.cc"
    "${LIBOS_IO_UDP}/*.cc"
    "${LIBOS_IO_LRPC}/*.cc"
)

message(STATUS ${LIBOS_SRC})

add_library(psp-libos STATIC ${LIBOS_SRC})
target_link_libraries(psp-libos psp-common boost_program_options boost_context boost_system boost_chrono -lnuma yaml-cpp psp-time pthread)
target_add_dpdk(psp-libos)
