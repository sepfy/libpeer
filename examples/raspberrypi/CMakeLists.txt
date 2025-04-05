cmake_minimum_required(VERSION 3.1)

project(raspberrypi)

file(GLOB SRCS "*.c")

find_package(PkgConfig)

pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

include(ExternalProject)

ExternalProject_Add(libpeer
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/dist
)
include_directories(${CMAKE_BINARY_DIR}/dist/include ${GST_INCLUDE_DIRS})

link_directories(${CMAKE_BINARY_DIR}/dist/lib)

add_executable(raspberrypi ${SRCS})

target_link_libraries(raspberrypi peer pthread ${GST_LIBRARIES})

