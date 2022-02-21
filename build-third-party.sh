#!/bin/bash

BASE_PATH=$(readlink -f $0)
BASE_DIR=$(dirname $BASE_PATH)

# Build libsrtp
cd $BASE_DIR/third_party/libsrtp
mkdir -p build && cd build
cmake ..
make -j4

# Build libnice
cd $BASE_DIR/third_party/libnice
meson builddir
ninja -C builddir

# Build librtp
cd $BASE_DIR/third_party/media-server/librtp/
make -j4

# Build cJSON
cd $BASE_DIR/third_party/cJSON
make static

# Build paho.mqtt.c
cd $BASE_PATH/third_party/paho.mqtt.c
mkdir -p build && cd build
cmake -DPAHO_WITH_SSL=on -DPAHO_BUILD_STATIC=on ..
make -j4

