cmake_minimum_required(VERSION 3.1)
project(peer)

option(ENABLE_TESTS "Enable tests" OFF)

include(third_party/coreHTTP/httpFilePaths.cmake)
include(third_party/coreMQTT/mqttFilePaths.cmake)

include_directories(dist/include)

link_directories(dist/lib)

set(DEP_LIBS "srtp2" "usrsctp" "mbedtls" "mbedcrypto" "mbedx509" "cjson")

add_definitions("-Wunused-variable -Werror=sequence-point -Werror=pointer-sign -Werror=return-type -Werror=sizeof-pointer-memaccess -Wincompatible-pointer-types -DHTTP_DO_NOT_USE_CUSTOM_CONFIG -DMQTT_DO_NOT_USE_CUSTOM_CONFIG")

add_subdirectory(src)
add_subdirectory(examples)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()

