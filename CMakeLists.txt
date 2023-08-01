cmake_minimum_required(VERSION 3.1)
project(peer)

option(ENABLE_TESTS "Enable tests" OFF)

include_directories(dist/include)

link_directories(dist/lib)

set(DEP_LIBS "srtp2" "usrsctp" "mbedtls" "mbedcrypto" "mbedx509" "cjson" "mqttc")

add_subdirectory(src)
add_subdirectory(examples)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()

