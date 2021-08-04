file(GLOB PSP_TIME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
add_library(psp-time STATIC ${PSP_TIME_SOURCES})
target_link_libraries(psp-time boost_chrono boost_system)
