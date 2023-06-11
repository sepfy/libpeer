#!/bin/bash

BASE_PATH=$(readlink -f $0)
BASE_DIR=$(dirname $BASE_PATH)

# Build libsrtp
cd $BASE_DIR/third_party/libsrtp
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" ..
make -j4

# Build libnice
cd $BASE_DIR/third_party/libnice
meson builddir -Ddefault_library=static
ninja -C builddir

# Build librtp
cd $BASE_DIR/third_party/media-server/librtp/
make -j4

# Build cJSON
cd $BASE_DIR/third_party/cJSON
make static

# Buld mosquitto
cd $BASE_DIR/third_party/mosquitto/lib
make WITH_STATIC_LIBRARIES=yes

# Build mbedTLS
cd $BASE_DIR/third_party/mbedtls
sed -i 's/\/\/#define MBEDTLS_SSL_DTLS_SRTP/#define MBEDTLS_SSL_DTLS_SRTP/g' include/mbedtls/mbedtls_config.h
make lib CFLAGS=-fPIC

cd $BASE_DIR/third_party/usrsctp
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" ..
make -j4
