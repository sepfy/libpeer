#!/bin/bash

BASE_PATH=$(readlink -f $0)
BASE_DIR=$(dirname $BASE_PATH)

mkdir -p $BASE_DIR/dist/

# Build libsrtp
cd $BASE_DIR/third_party/libsrtp
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" -DTEST_APPS=off -DCMAKE_INSTALL_PREFIX=$BASE_DIR/dist ..
make -j4
make install

# Build cJSON
cd $BASE_DIR/third_party/cJSON
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" -DBUILD_SHARED_LIBS=off -DENABLE_CJSON_TEST=off -DCMAKE_INSTALL_PREFIX=$BASE_DIR/dist ..
make -j4
make install
# keep the path consistent with esp-idf
ln -s $BASE_DIR/dist/include/cjson/cJSON.h $BASE_DIR/dist/include/cJSON.h

# Build mbedTLS
cd $BASE_DIR/third_party/mbedtls
sed -i 's/\/\/#define MBEDTLS_SSL_DTLS_SRTP/#define MBEDTLS_SSL_DTLS_SRTP/g' include/mbedtls/mbedtls_config.h
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" -DENABLE_TESTING=off -DENABLE_PROGRAMS=off -DCMAKE_INSTALL_PREFIX=$BASE_DIR/dist ..
make install

cd $BASE_DIR/third_party/MQTT-C
mkdir -p build && cd build
cmake -DMQTT_C_EXAMPLES=off -DCMAKE_PREFIX_PATH=$BASE_DIR/dist -DCMAKE_INSTALL_LIBDIR=$BASE_DIR/dist/lib -DCMAKE_INSTALL_INCLUDEDIR=$BASE_DIR/dist/include/ -DMQTT_C_MbedTLS_SUPPORT=on ..
make -j4
make install
cp ../examples/templates/mbedtls_sockets.h $BASE_DIR/dist/include/

cd $BASE_DIR/third_party/usrsctp
mkdir -p build && cd build
cmake -DCMAKE_C_FLAGS="-fPIC" -Dsctp_build_programs=off -DCMAKE_INSTALL_PREFIX=$BASE_DIR/dist ..
make -j4
make install

