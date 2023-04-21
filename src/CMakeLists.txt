project(lib)

file(GLOB SRCS "*.c")
file(GLOB TEST_SRCS "*test.c")

list(REMOVE_ITEM SRCS ${TEST_SRCS})

if(NOT ENABLE_SIGNALING)
  list(REMOVE_ITEM SRCS ${SIGNALING_SRCS})
endif()

if(ENABLE_TEST)
  foreach(sourcefile ${TEST_SRCS})
    string(REPLACE ".c" "" appname ${sourcefile})
    string(REPLACE "${PROJECT_SOURCE_DIR}/" "" appname ${appname})
    set_source_files_properties(${sourcefile} PROPERTIES LANGUAGE CXX)
    add_executable(${appname} ${sourcefile})
    target_link_libraries(${appname} pear gtest ${TARGET_LIBS})
  endforeach(sourcefile ${TEST_SRCS})
endif(ENABLE_TEST)

file(GLOB HEADERS "*.h")

option(ENABLE_TEST "Enable unit test" OFF)
option(ENABLE_GSTREAMER "Enable GStreamer" ON)

if(ENABLE_EXAMPLES)
  set(ENABLE_SIGNALING ON)
endif()

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GST REQUIRED gstreamer-1.0>=1.4 gstreamer-base-1.0>=1.4)

set(LIBNICE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/libnice/agent)
set(LIBSRTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include)
set(LIBRTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/include)
set(USRSCTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/usrsctplib)
include_directories(${GLIB_INCLUDE_DIRS} ${LIBNICE_INCLUDE_DIRS} ${LIBSRTP_INCLUDE_DIRS} ${LIBRTP_INCLUDE_DIRS} ${USRSCTP_INCLUDE_DIRS})

include_directories(${CMAKE_SOURCE_DIR}/src ${GST_INCLUDE_DIRS})

set(LIBNICE_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/)
set(LIBSRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/)
set(LIBRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux)
set(USRSCTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/build/usrsctplib)
link_directories(${LIBNICE_LINK_DIRS} ${LIBSRTP_LINK_DIRS} ${LIBRTP_LINK_DIRS} ${USRSCTP_LINK_DIRS})

if(ENABLE_SIGNALING)
  set(SIGNALING_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/httpserver.h ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  include_directories(${SIGNALING_INCLUDE_DIRS})
  set(SIGNALING_LIB_DIRS ${CMAKE_SOURCE_DIR}/third_party/cJSON)
  link_directories(${SIGNALING_LIB_DIRS})
  LIST(APPEND TARGET_LIBS "cjson")
endif()

add_library(pear SHARED ${SRCS})

target_link_libraries(pear ssl crypto nice srtp2 rtp usrsctp pthread glib-2.0 gobject-2.0 gio-2.0 ${GST_LIBRARIES})

if(ENABLE_AEC)
  target_link_libraries(pear speexdsp)
endif()

set_target_properties(pear PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS pear
  LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)

