# libpeer - Portable WebRTC Library for IoT/Embedded Device

![build](https://github.com/sepfy/pear/actions/workflows/build.yml/badge.svg)

libpeer is a WebRTC implementation written in C, developed with BSD socket. The library aims to integrate IoT/Embedded device video/audio streaming with WebRTC, such as ESP32 and Raspberry Pi

### Features

- Vdieo/Audio Codec
  - H264
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
  - OPUS
- DataChannel
- STUN/TURN
- IPV4/IPV6
- Signaling
  - [WHIP](https://www.ietf.org/archive/id/draft-ietf-wish-whip-01.html)
  - MQTT

### Dependencies

* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [libsrtp](https://github.com/cisco/libsrtp)
* [usrsctp](https://github.com/sctplab/usrsctp)
* [cJSON](https://github.com/DaveGamble/cJSON.git)
* [coreHTTP](https://github.com/FreeRTOS/coreHTTP)
* [coreMQTT](https://github.com/FreeRTOS/coreMQTT)

### Getting Started with Generic Example
```bash
$ sudo apt -y install git cmake
$ git clone --recursive https://github.com/sepfy/libpeer
$ cd libpeer
$ cmake -S . -B build && cmake --build build
$ wget http://www.live555.com/liveMedia/public/264/test.264 # Download test video file
$ wget https://mauvecloud.net/sounds/alaw08m.wav # Download test audio file
$ ./generic/sample/sample
```

### Examples for Platforms
- [ESP32](https://github.com/sepfy/libpeer/tree/main/examples/esp32): MJPEG over datachannel
- [Raspberry Pi](https://github.com/sepfy/libpeer/tree/main/examples/raspberrypi): Video and two-way audio stream
