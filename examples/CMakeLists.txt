cmake_minimum_required(VERSION 3.1)
project(examples)

option(ENABLE_EXAMPLES "Build examples" OFF)

include_directories(${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include/ ${CMAKE_SOURCE_DIR}/third_party/libnice/agent ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/include/ ${CMAKE_SOURCE_DIR}/src/)

link_directories(${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/ ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/ ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux)

add_subdirectory(local_file)
if(ENABLE_EXAMPLES)
  add_subdirectory(gstreamer)
endif(ENABLE_EXAMPLES)
