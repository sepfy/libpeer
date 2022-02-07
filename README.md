# Pear - WebRTC Toolkit for IoT/Embedded Device

![pear-ci](https://github.com/sepfy/pear/actions/workflows/pear-ci.yml/badge.svg)

Pear is a WebRTC SDK written in C. The SDK aims to integrate IoT/Embedded device video/audio streaming with WebRTC.

### Features

- Vdieo/Audio Codec Support
  - H264
  - G.711 PCM (A-law)
  - Opus
- Browser Tested
  - Chrome
  - Safari
  - Edge

### Dependencies

* [libsrtp](https://github.com/cisco/libsrtp)
* [libnice](https://github.com/libnice/libnice)
* [librtp](https://github.com/ireader/media-server)
* [httpserver.h](https://github.com/jeremycw/httpserver.h.git) (Option)
* [cJSON](https://github.com/DaveGamble/cJSON.git) (Option)

### Getting Started

```
# sudo apt -y install libglib2.0-dev libssl-dev git cmake ninja-build
# sudo pip3 install meson
# git clone --recursive https://github.com/sepfy/pear
# ./build-third-party.sh
# mkdir cmake
# cd cmake
# cmake ..
# make
```

### Examples
- [Video On Demand](https://github.com/sepfy/pear/tree/main/examples/video_on_demand): Stream sigle video file from device to browser.
- [Surveillance](https://github.com/sepfy/pear/tree/main/examples/surveillance): Stream single camera from device to browser.
- [Home Camera](https://github.com/sepfy/pear/tree/main/examples/home_camera): Stream camera video from devcie and two way audio.
- [Screencast](https://github.com/sepfy/pear/tree/main/examples/screencast): Stream single video content from Browser to device.
