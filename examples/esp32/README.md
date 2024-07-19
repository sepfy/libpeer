# ESP32
Stream JPEG over WebRTC datachannel with ESP32.

## Support List

| Hardware ||
|---|---|
| [ESP32-EYE](https://github.com/espressif/esp-who/blob/master/docs/en/get-started/ESP-EYE_Getting_Started_Guide.md) |<img src="https://www.espressif.com/sites/default/files/esp-eye-2-190116.png" width="64">|
| [ESP32 M5Camera](https://github.com/m5stack/M5Stack-Camera) |<img src="https://static-cdn.m5stack.com/resource/docs/products/unit/m5camera/m5camera_01.webp" width="64">|

## Instructions

### Install esp-idf
at lease v5.2
```bash
$ git clone -b v5.2.2 https://github.com/espressif/esp-idf.git
$ cd esp-idf
$ source install.sh
$ source export.sh
```

### Download
```bash
$ git clone https://github.com/sepfy/libpeer
$ cd libpeer/examples/esp32
$ idf.py add-dependency "espressif/esp32-camera^2.0.4"
$ idf.py add-dependency "mdns"
$ git clone --recursive https://github.com/sepfy/esp_ports.git components/srtp
```

### Configure
```bash
$ idf.py menuconfig
# Choose Example Connection Configuration and change the SSID and password
```

### Build 
```bash
$ idf.py build
```

### Test
```bash
$ idf.py flash
```
Check the serial port message:
```
I (10813) Camera: Camera Task Started
I (10813) webrtc: peer_signaling_task started
I (10813) webrtc: peer_connection_task started
I (10823) webrtc: [APP] Free memory: 3882160 bytes
I (10823) webrtc: open https://sepfy.github.io/webrtc?deviceId=esp32-xxxxxxxxxxxx
```
Open the browser and visit the website showing by serial port message
![image](https://github.com/sepfy/libpeer/assets/22016807/46df15b1-9e28-4a6b-bf0a-4f676778cf7d)

