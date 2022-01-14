# Home Camera

Build Raspberry Pi as a home camera. Support video camera, 2-way audio.

### Hardware Configuration

* [Raspberry Pi 3A+](https://www.raspberrypi.com/products/raspberry-pi-3-model-a-plus/)
* [Camera Module](https://www.raspberrypi.com/products/camera-module-v2/)
* [ReSpeaker 2-Mics Pi HAT](https://wiki.seeedstudio.com/ReSpeaker_2_Mics_Pi_HAT_Raspberry/)
* Speaker

### Run Example

```
cd pear && mkdir cmake && cd cmake
cmake -DENABLE_EXAMPLES=on ..
make
./examples/home_camera/home_camera
```

Open Google-Chrome and go to https://raspberrypi.local. You can communication with Raspberry Pi and watch the camera video remotely.


