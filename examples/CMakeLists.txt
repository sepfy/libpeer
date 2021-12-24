cmake_minimum_required(VERSION 3.1)
project(examples)

option(ENABLE_EXAMPLES "Build examples" OFF)

include_directories(${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include/ ${CMAKE_SOURCE_DIR}/third_party/libnice/agent ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/include/ ${CMAKE_SOURCE_DIR}/src/ ${CMAKE_SOURCE_DIR}/third_party/cJSON/)

link_directories(${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/ ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/ ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux ${CMAKE_SOURCE_DIR}/third_party/cJSON/)

if(ENABLE_EXAMPLES)
  add_subdirectory(file_send)
  add_subdirectory(camera_send)
  add_subdirectory(screen_recv)
  add_subdirectory(home_camera)
  add_subdirectory(bidirection)
endif(ENABLE_EXAMPLES)
