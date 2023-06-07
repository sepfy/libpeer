# Surveillance

Build Raspberry Pi as a surveillance camera and watch the video remotely.

### Hardware Configuration

* Raspberry Pi Zero W
* Camera Module

### Run Example

```
cd pear && mkdir cmake && cd cmake
cmake -DENABLE_EXAMPLES=on ..
make
./examples/surveillance/surveillance
```

Open Google-Chrome and go to https://raspberrypi.local. You will see real-time of the camera video.

