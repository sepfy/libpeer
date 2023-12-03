cmake_policy(SET CMP0077 NEW)   # option() honors normal variables.

function(replace_in_file file pattern replacement)
    file(STRINGS "${file}" old_file_lines NO_HEX_CONVERSION)

    set(new_file_lines)
    set(modified FALSE)
    foreach(old_line IN LISTS old_file_lines)
        string(REGEX REPLACE "${pattern}" "${replacement}" new_line "${old_line}")
        list(APPEND new_file_lines "${new_line}")
        if(NOT ("${old_line}" STREQUAL "${new_line}"))
            set(modified TRUE)
        endif()
    endforeach()

    if(modified)    # write is conditional to prevent constantly updating its write timestamp
        list(JOIN new_file_lines "\n" new_file_contents)
        file(WRITE "${file}" "${new_file_contents}")
    endif()
endfunction()

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}")

# cJSON
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(cJSON)

target_include_directories(cjson INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cJSON>
    $<INSTALL_INTERFACE:include/cjson>)

add_library(external::cjson ALIAS cjson)


# libsrtp
replace_in_file("libsrtp/CMakeLists.txt" [[CMAKE_(PROJECT_[A-Z_]*)]] [[\1]])
set(TEST_APPS OFF)
add_subdirectory(libsrtp)

# since we're not running install anymore, manually copy over required files
# (TODO: remove the need for this and *somehow* install libsrtp during build)
file(COPY libsrtp/include/srtp.h libsrtp/crypto/include/auth.h
    libsrtp/crypto/include/cipher.h
    libsrtp/crypto/include/crypto_types.h
    DESTINATION include/srtp2
)
target_include_directories(srtp2 INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")

add_library(external::srtp2 ALIAS srtp2)


# mbedTLS
replace_in_file("mbedtls/include/mbedtls/mbedtls_config.h" [[^//(#define MBEDTLS_SSL_DTLS_SRTP)]] [[\1]])
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(MBEDTLS_TARGET_PREFIX mbed- CACHE STRING "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE) # MBedTLS runs into warnings with some compilers (e.g. clang)
add_subdirectory(mbedtls)
add_library(external::mbedcrypto ALIAS ${MBEDTLS_TARGET_PREFIX}mbedcrypto)
add_library(external::mbedtls ALIAS ${MBEDTLS_TARGET_PREFIX}mbedtls)
add_library(external::mbedx509 ALIAS ${MBEDTLS_TARGET_PREFIX}mbedx509)


# usrsctp
set(sctp_build_programs OFF CACHE BOOL "" FORCE)
set(sctp_werror OFF CACHE BOOL "" FORCE)    # usrsctp runs into warnings with some compilers (e.g. clang)
set(CMAKE_C_FLAGS -Wno-gnu-offsetof-extensions)
add_subdirectory(usrsctp)
add_library(external::usrsctp ALIAS usrsctp)


# coreHTTP
include(coreHTTP/httpFilePaths.cmake)
add_library(coreHTTP STATIC EXCLUDE_FROM_ALL ${HTTP_SOURCES})
target_compile_definitions(coreHTTP PUBLIC HTTP_DO_NOT_USE_CUSTOM_CONFIG)
target_include_directories(coreHTTP PUBLIC ${HTTP_INCLUDE_PUBLIC_DIRS})

add_library(external::coreHTTP ALIAS coreHTTP)


# coreMQTT
include(coreMQTT/mqttFilePaths.cmake)
add_library(coreMQTT STATIC EXCLUDE_FROM_ALL ${MQTT_SOURCES})
target_compile_definitions(coreMQTT PUBLIC MQTT_DO_NOT_USE_CUSTOM_CONFIG)
target_include_directories(coreMQTT PUBLIC ${MQTT_INCLUDE_PUBLIC_DIRS})

add_library(coreMQTT-serializer STATIC ${MQTT_SERIALIZER_SOURCES})
target_link_libraries(coreMQTT-serializer coreMQTT)

add_library(external::coreMQTT ALIAS coreMQTT)
add_library(external::coreMQTT-serializer ALIAS coreMQTT-serializer)
