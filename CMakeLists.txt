cmake_minimum_required(VERSION 3.1)
project(pear)

option(ENABLE_TESTS "Enable tests" OFF)
option(ENABLE_EXAMPLES "Build examples" OFF)
option(ENABLE_AEC "Enable acoustic echo cancellation" OFF)

set(LIBSRTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/include)
set(USRSCTP_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/usrsctplib)
set(MBEDTLS_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/mbedtls/include)
include_directories(${LIBSRTP_INCLUDE_DIRS} ${USRSCTP_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS})

set(LIBSRTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/libsrtp/build/)
set(USRSCTP_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/usrsctp/build/usrsctplib)
set(MBEDTLS_LINK_DIRS ${CMAKE_SOURCE_DIR}/third_party/mbedtls/library)
link_directories(${LIBSRTP_LINK_DIRS} ${USRSCTP_LINK_DIRS} ${MBEDTLS_LINK_DIRS})

set(TARGET_LIBS "srtp2" "usrsctp" "mbedtls" "mbedcrypto" "mbedx509")

if(ENABLE_EXAMPLES)
  set(TARGET_LIBS ${TARGET_LIBS} mosquitto pthread cjson)
  add_subdirectory(examples)
endif()

if(ENABLE_AEC)
  set(TARGET_LIBS ${TARGET_LIBS} speexdsp)
endif()
 
if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(src)

