cmake_minimum_required(VERSION 3.1)
project(src)

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)

file(GLOB SRCS "*.c")
file(GLOB TEST_SRCS "*test.c")
list(REMOVE_ITEM SRCS ${TEST_SRCS})

include_directories(${GLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include/ ${CMAKE_SOURCE_DIR}/third_party/libnice/agent ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/include ${CMAKE_SOURCE_DIR}/third_party/httpserver.h)

link_directories(${CMAKE_SOURCE_DIR}/third_party/libnice/builddir/nice/ ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/ ${CMAKE_SOURCE_DIR}/third_party/media-server/librtp/debug.linux)

add_library(pear SHARED ${SRCS})

if(ENABLE_TEST)
  foreach(sourcefile ${TEST_SRCS})
    string(REPLACE ".c" "" appname ${sourcefile})
    string(REPLACE "${PROJECT_SOURCE_DIR}/" "" appname ${appname})
    set_source_files_properties(${sourcefile} PROPERTIES LANGUAGE CXX )
    add_executable(${appname} ${sourcefile})
    target_link_libraries(${appname} pear gtest pthread glib-2.0 nice srtp2 ssl crypto)
  endforeach(sourcefile ${TEST_SRCS})
endif(ENABLE_TEST)

target_link_libraries(pear cjson)

file(GLOB HEADERS "*.h")
set_target_properties(pear PROPERTIES PUBLIC_HEADER "${HEADERS}")

install(TARGETS pear
    LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_PREFIX}/include/
)
