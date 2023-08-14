# Raspberry Pi

Build a home camera with Raspberry Pi. Support camera video streaming and two-way audio communication.

## Hardware

* [Raspberry Pi 4B](https://www.raspberrypi.com/products/raspberry-pi-4-model-b/)
* [Camera Module](https://www.raspberrypi.com/products/camera-module-v2/)
* [ReSpeaker 2-Mics Pi HAT](https://wiki.seeedstudio.com/ReSpeaker_2_Mics_Pi_HAT_Raspberry/)
* Speaker

## Instructions
### Install
* Install [Raspberry Pi OS Lite Image](https://www.raspberrypi.com/software/operating-systems/)
* Install dependencies
```bash
$ sudo apt update
$ sudo apt install -y gstreamer1.0-alsa gstreamer1.0-tools gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-plugins-good libgstreamer1.0-dev git
```

### Build
```bash
$ git clone --recursive https://github.com/sepfy/libpeer
$ cd libpeer
$ ./build-third-party.sh
$ mkdir cmake && cd camke
$ cmake -DCMAKE_INSTALL_PREFIX=. ..
$ make && make install
$ cd ../examples/raspberrypi
$ mkdir build && cd build
$ make
```

### Test
```bash
$ echo "" | openssl s_client -showcerts -connect mqtt.eclipseprojects.io:8883 | sed -n "1,/Root/d; /BEGIN/,/END/p" | openssl x509 -outform PEM >mqtt_eclipse_org.pem
$ ./raspberrypi mqtt_eclipse_org.pem
```

