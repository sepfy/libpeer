#!/usr/bin/env bash

sudo apt-get -y install libglib2.0-dev libssl-dev git cmake ninja-build
sudo pip3 install meson
./build-third-party.sh
mkdir cmake && cd cmake
cmake ..
make
