cmake_minimum_required(VERSION 3.1)
project(peer LANGUAGES C)
set(CMAKE_C_STANDARD 99)

option(ENABLE_TESTS "Enable tests" OFF)

add_subdirectory(third_party dist)

set(DEP_LIBS
    external::cjson external::srtp2
    external::mbedtls external::mbedcrypto external::mbedx509
    external::usrsctp
    external::coreHTTP
    external::coreMQTT external::coreMQTT-serializer
)

add_definitions("-Wunused-variable -Werror=sequence-point -Werror=pointer-sign -Werror=return-type -Werror=sizeof-pointer-memaccess -Wincompatible-pointer-types")

add_subdirectory(src)

add_library(peer::shared ALIAS peer)
add_library(peer::static ALIAS peer-static)

add_subdirectory(examples)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
