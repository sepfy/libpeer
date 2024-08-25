# ESP32
This guide demonstrates how to stream JPEG images over a WebRTC data channel using ESP32 hardware.

## Supported Devices
| Device |Image|
|---|---|
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

### Configure Your Build
```bash
$ idf.py menuconfig
# Choose Example Connection Configuration and change the SSID and password
```

### Build the Project
```bash
$ idf.py build
```

### Flash and Test
```bash
$ idf.py flash
```
Monitor the serial port output:
```
I (10813) Camera: Camera Task Started
I (10813) webrtc: peer_signaling_task started
I (10813) webrtc: peer_connection_task started
I (10823) webrtc: [APP] Free memory: 3882160 bytes
I (10823) webrtc: open https://sepfy.github.io/webrtc?deviceId=esp32-xxxxxxxxxxxx
```
Open your browser and navigate to the URL provided in the serial port output:
![image](https://github.com/sepfy/libpeer/assets/22016807/46df15b1-9e28-4a6b-bf0a-4f676778cf7d)
