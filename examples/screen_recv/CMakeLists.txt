cmake_minimum_required(VERSION 3.1)
project(screen_recv)

find_package(PkgConfig)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

file(GLOB SRCS "*.c")

include_directories(${GST_INCLUDE_DIRS})

add_executable(screen_recv ${SRCS})

target_link_libraries(screen_recv pear glib-2.0 ssl crypto nice srtp2 rtp pthread event cjson ${GST_LIBRARIES})
