cmake_minimum_required(VERSION 3.1)
project(gstreamer)

find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

file(GLOB SRCS "*.c")

add_executable(gstreamer ${SRCS})

include_directories(${GST_INCLUDE_DIRS})

target_link_libraries(gstreamer pear glib-2.0 ssl crypto nice srtp2 rtp ${GST_LIBRARIES})
