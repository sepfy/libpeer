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
set(USRSCTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/usrsctplib)
include_directories(${GLIB_INCLUDE_DIRS} ${LIBNICE_INCLUDE_DIRS} ${LIBSRTP_INCLUDE_DIRS} ${LIBRTP_INCLUDE_DIRS} ${USRSCTP_INCLUDE_DIRS})

set(LIBNICE_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/)
set(LIBSRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/)
set(LIBRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux)
set(USRSCTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/build/usrsctplib)
link_directories(${LIBNICE_LINK_DIRS} ${LIBSRTP_LINK_DIRS} ${LIBRTP_LINK_DIRS} ${USRSCTP_LINK_DIRS})

set(TARGET_LIBS "ssl" "crypto" "nice" "srtp2" "rtp" "usrsctp" "pthread" "glib-2.0" "gobject-2.0" "gio-2.0")

if(ENABLE_SIGNALING)
  set(SIGNALING_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/httpserver.h ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  include_directories(${SIGNALING_INCLUDE_DIRS})
  set(SIGNALING_LIB_DIRS ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  link_directories(${SIGNALING_LIB_DIRS})
  LIST(APPEND TARGET_LIBS "cjson")
endif()

add_subdirectory(src)

if(ENABLE_EXAMPLES)
add_subdirectory(examples)
endif()
