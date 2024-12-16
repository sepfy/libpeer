# ESP32
This guide demonstrates how to stream JPEG images over a WebRTC data channel using ESP32 hardware.

## Supported Devices
| Device |Image|
|---|---|
| [Freenove ESP32-S3-WROOM Board](https://store.freenove.com/products/fnk0085) |<img src="https://store.freenove.com/cdn/shop/files/FNK0085.MAIN.jpg" width="64">|
| [XIAO ESP32S3 Sense](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) |<img src="https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/img/xiaoesp32s3sense.jpg" width="64">|
| [ESP32-EYE](https://github.com/espressif/esp-who/blob/master/docs/en/get-started/ESP-EYE_Getting_Started_Guide.md) |<img src="https://www.espressif.com/sites/default/files/esp-eye-2-190116.png" width="64">|
| [M5Camera](https://github.com/m5stack/M5Stack-Camera) |<img src="https://static-cdn.m5stack.com/resource/docs/products/unit/m5camera/m5camera_01.webp" width="64">|

## Setup Instructions

### Install esp-idf (v5.2 or higher)
```bash
$ git clone -b v5.2.2 https://github.com/espressif/esp-idf.git
$ cd esp-idf
$ source install.sh
$ source export.sh
```

### Download Required Libraries
```bash
$ git clone https://github.com/sepfy/libpeer
$ cd libpeer/examples/esp32
```

### Get Your URL
Go to the test [website](https://sepfy.github.io/libpeer) and copy the URL

### Configure Your Build
```bash
$ idf.py menuconfig
# Peer Connection Configuration -> Signaling URL
# Example Connection Configuration -> WiFi SSID and WiFi Password
```

### Build the Project
```bash
$ idf.py build
```

### Flash and Test
```bash
$ idf.py flash
```
### Connect to Your Device
Back to the test website and click Connect button

