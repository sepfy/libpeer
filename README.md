# libpeer - Portable WebRTC Library for IoT/Embedded Device

![pear-ci](https://github.com/sepfy/pear/actions/workflows/pear-ci.yml/badge.svg)

libpeer is a WebRTC implementation written in C, developed with BSD socket. The library aims to integrate IoT/Embedded device video/audio streaming with WebRTC, such as ESP32 and Raspberry Pi

### Features

- Vdieo/Audio Codec
  - H264
  - G.711 PCM (A-law)
  - G.711 PCM (µ-law)
  - OPUS
- DataChannel
- STUN/TURN
- Signaling

### Dependencies

* [mbedtls](https://github.com/Mbed-TLS/mbedtls)
* [libsrtp](https://github.com/cisco/libsrtp)
* [usrsctp](https://github.com/sctplab/usrsctp)
* [cJSON](https://github.com/DaveGamble/cJSON.git)
* [MQTT-C](https://github.com/LiamBindle/MQTT-C)

### Getting Started
```bash
$ sudo apt -y install git cmake
$ git clone --recursive https://github.com/sepfy/libpeer
$ cd libpeer
$ ./build-third-party.sh
$ mkdir cmake
$ cd cmake
$ cmake ..
$ make
$ echo "" | openssl s_client -showcerts -connect mqtt.eclipseprojects.io:8883 | sed -n "1,/Root/d; /BEGIN/,/END/p" | openssl x509 -outform PEM >mqtt_eclipse_org.pem # Download certificate for signaling
$ wget http://www.live555.com/liveMedia/public/264/test.264 # Download test video file
$ wget https://mauvecloud.net/sounds/alaw08m.wav # Download test audio file
$ ./examples/sample/sample
```

### Supported Platforms
- [ESP32](https://github.com/sepfy/libpeer/tree/main/examples/esp32)
- [Raspberry Pi](https://github.com/sepfy/libpeer/tree/main/examples/raspberrypi)
