cmake_minimum_required(VERSION 3.1)
project(pear)

option(ENABLE_TEST "Enable unit test" OFF)
option(ENABLE_EXAMPLES "Build examples" OFF)
option(ENABLE_SIGNALING "Enable signaling service" OFF)

if(ENABLE_EXAMPLES)
  set(ENABLE_SIGNALING ON)
endif()

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)

set(LIBNICE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/libnice/agent)
set(LIBSRTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include)
set(LIBRTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/include)
include_directories(${GLIB_INCLUDE_DIRS} ${LIBNICE_INCLUDE_DIRS} ${LIBSRTP_INCLUDE_DIRS} ${LIBRTP_INCLUDE_DIRS})

set(LIBNICE_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/)
set(LIBSRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/)
set(LIBRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux)
link_directories(${LIBNICE_LINK_DIRS} ${LIBSRTP_LINK_DIRS} ${LIBRTP_LINK_DIRS})

set(TARGET_LIBS "glib-2.0" "ssl" "crypto" "nice" "srtp2" "rtp" "pthread")

if(ENABLE_SIGNALING)
  set(SIGNALING_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/httpserver.h ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.c/src/ ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  include_directories(${SIGNALING_INCLUDE_DIRS})
  set(SIGNALING_LIB_DIRS ${CMAKE_SOURCE_DIR}/third_party/cJSON ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.c/build/src)
  link_directories(${SIGNALING_LIB_DIRS})
  LIST(APPEND TARGET_LIBS "cjson" "paho-mqtt3cs")
endif()

add_subdirectory(src)

if(ENABLE_EXAMPLES)
add_subdirectory(examples)
endif()
