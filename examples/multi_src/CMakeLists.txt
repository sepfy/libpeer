cmake_minimum_required(VERSION 3.1)
project(multi-src)

find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

file(GLOB SRCS "*.c")

include_directories(${GST_INCLUDE_DIRS})

add_executable(multi-src ${SRCS})

target_link_libraries(multi-src pear glib-2.0 ssl crypto nice srtp2 rtp pthread event cjson ${GST_LIBRARIES})
