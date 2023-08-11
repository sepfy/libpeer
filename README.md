# libpeer - Portable WebRTC Library for IoT/Embedded Device

![pear-ci](https://github.com/sepfy/pear/actions/workflows/pear-ci.yml/badge.svg)

libpeer is a WebRTC implementation written in C, developed with BSD socket. The library aims to integrate IoT/Embedded device video/audio streaming with WebRTC, such as ESP32 and Raspberry Pi

### Features

- Vdieo/Audio Codec
  - H264
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
- DataChannel
- STUN
- Signaling

### Dependencies

* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [libsrtp](https://github.com/cisco/libsrtp)
* [usrsctp](https://github.com/sctplab/usrsctp)
* [cJSON](https://github.com/DaveGamble/cJSON.git)
* [MQTT-C](https://github.com/LiamBindle/MQTT-C)

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
- [Raspberry Pi](https://github.com/sepfy/pear/tree/main/examples/raspberrypi)
- [ESP32](https://github.com/sepfy/pear/tree/main/examples/esp32)


