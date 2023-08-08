cmake_minimum_required(VERSION 3.1)

project(raspberrypi)

file(GLOB SRCS "*.c")

find_package(PkgConfig)

pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

include_directories(../../cmake/include ${GST_INCLUDE_DIRS})

link_directories(../../cmake/lib)

add_executable(raspberrypi ${SRCS})

target_link_libraries(raspberrypi peer pthread ${GST_LIBRARIES})

