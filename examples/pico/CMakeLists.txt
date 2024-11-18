cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(pico-peer)
pico_sdk_init()

include(FreeRTOS_Kernel_import.cmake)


include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/coreHTTP/httpFilePaths.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../third_party/coreMQTT/mqttFilePaths.cmake)
include_directories(
  ${CMAKE_CURRENT_LIST_DIR}/
  ${CMAKE_BINARY_DIR}/
  ${CMAKE_CURRENT_LIST_DIR}/../../third_party/libsrtp/include/
  ${CMAKE_CURRENT_LIST_DIR}/../../third_party/libsrtp/crypto/include/
  ${CMAKE_CURRENT_LIST_DIR}/../../third_party/cJSON/
  ${CMAKE_CURRENT_LIST_DIR}/../../src/
  ${HTTP_INCLUDE_PUBLIC_DIRS}
  ${MQTT_INCLUDE_PUBLIC_DIRS}
)

set(PICO_SDK_LIBS
  pico_stdlib
  pico_cyw43_arch_lwip_sys_freertos
  pico_mbedtls
  FreeRTOS-Kernel-Heap4
)

# Build libsrtp
file(GLOB LIBSRTP_SRC
  ../../third_party/libsrtp/srtp/srtp.c
  ../../third_party/libsrtp/crypto/cipher/cipher.c
  ../../third_party/libsrtp/crypto/cipher/null_cipher.c
  ../../third_party/libsrtp/crypto/cipher/aes.c
  ../../third_party/libsrtp/crypto/cipher/aes_icm.c
  ../../third_party/libsrtp/crypto/cipher/cipher_test_cases.c
  ../../third_party/libsrtp/crypto/hash/auth.c
  ../../third_party/libsrtp/crypto/hash/null_auth.c
  ../../third_party/libsrtp/crypto/hash/hmac.c
  ../../third_party/libsrtp/crypto/hash/sha1.c
  ../../third_party/libsrtp/crypto/hash/auth_test_cases.c
  ../../third_party/libsrtp/crypto/kernel/alloc.c
  ../../third_party/libsrtp/crypto/kernel/crypto_kernel.c
  ../../third_party/libsrtp/crypto/kernel/err.c
  ../../third_party/libsrtp/crypto/kernel/key.c
  ../../third_party/libsrtp/crypto/math/datatypes.c
  ../../third_party/libsrtp/crypto/math/stat.c
  ../../third_party/libsrtp/crypto/replay/rdb.c
  ../../third_party/libsrtp/crypto/replay/rdbx.c
  ../../third_party/libsrtp/crypto/replay/ut_sim.c
)

add_library(srtp STATIC
  ${LIBSRTP_SRC}
)

target_compile_definitions(srtp PRIVATE
  HAVE_CONFIG_H
  PRIx64=PRIx32
)

target_link_libraries(srtp
  ${PICO_SDK_LIBS}
)

add_custom_command(TARGET srtp
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/srtp2/
  COMMAND ${CMAKE_COMMAND} -E copy
  ../../../third_party/libsrtp/include/srtp.h
  ${CMAKE_BINARY_DIR}/srtp2/
)

# Build cJSON
add_library(cjson STATIC
  ../../third_party/cJSON/cJSON.c
)

# Build peer
file(GLOB LIBPEER_SRC "../../src/*.c")
add_library(peer STATIC
  ${LIBPEER_SRC}
  ${HTTP_SOURCES}
  ${MQTT_SOURCES}
  ${MQTT_SERIALIZER_SOURCES}
)

target_compile_definitions(peer PRIVATE
  __BYTE_ORDER=__LITTLE_ENDIAN
  CONFIG_USE_LWIP=1
  CONFIG_USE_USRSCTP=0
  CONFIG_MBEDTLS_2_X=1
  CONFIG_DATA_BUFFER_SIZE=512
  CONFIG_AUDIO_BUFFER_SIZE=2048
  CONFIG_HTTP_BUFFER_SIZE=1024
  CONFIG_SDP_BUFFER_SIZE=4096
  HTTP_DO_NOT_USE_CUSTOM_CONFIG
  MQTT_DO_NOT_USE_CUSTOM_CONFIG
)

target_link_libraries(peer
  ${PICO_SDK_LIBS}
  srtp
  cjson
)

pico_generate_pio_header(pico_peer ${CMAKE_CURRENT_LIST_DIR}/rp2040_i2s_example/i2s.pio)

# Build pico_peer
add_executable(pico_peer
  main.c
  rp2040_i2s_example/i2s.c
  pcm-g711/pcm-g711/g711.c
)

target_compile_definitions(pico_peer PRIVATE
  WIFI_SSID="$ENV{WIFI_SSID}"
  WIFI_PASSWORD="$ENV{WIFI_PASSWORD}"
  PICO_HEAP_SIZE=0x20000
  #PICO_DEBUG_MALLOC=1
)

target_link_libraries(pico_peer
  ${PICO_SDK_LIBS}
  peer
)

pico_enable_stdio_usb(pico_peer 1)
pico_enable_stdio_uart(pico_peer 0)
pico_add_extra_outputs(pico_peer)
