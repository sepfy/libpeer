# Pear - WebRTC Toolkit for IoT/Embedded Device

![pear-ci](https://github.com/sepfy/pear/actions/workflows/pear-ci.yml/badge.svg)

Pear is a WebRTC SDK written in C. The SDK aims to integrate IoT/Embedded device video/audio streaming with WebRTC.

### Features

- Vdieo/Audio Codec
  - H264
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
- DataChannel
- STUN
- Signaling

### Dependencies

* [libsrtp](https://github.com/cisco/libsrtp)
* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [usrsctp](https://github.com/sctplab/usrsctp)
* [cJSON](https://github.com/DaveGamble/cJSON.git)
* [mosquitto](https://github.com/eclipse/mosquitto)

### Getting Started
Download test video/audio file from ...
```bash
$ sudo apt -y install git cmake
$ git clone --recursive https://github.com/sepfy/pear
$ ./build-third-party.sh
$ mkdir cmake
$ cd cmake
$ cmake ..
$ make
$ ./example
```

### Supported Platforms
- [Raspberry Pi](https://github.com/sepfy/pear-raspberrypi-example)
- [ESP32](https://github.com/sepfy/pear-esp32-examples)
- [M1s Dock](https://github.com/sepfy/pear-m1s-example)

