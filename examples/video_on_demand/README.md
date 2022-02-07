# Video On Demand

Playback the video file in the Raspberry Pi.

### Hardware Configuration

* [Raspberry Pi 3B](https://www.raspberrypi.com/products/raspberry-pi-3-model-b/)

### Run Example

Download test [video file](http://www.live555.com/liveMedia/public/264/test.264)

```
cd pear && mkdir cmake && cd cmake
cmake -DENABLE_EXAMPLES=on ..
make
./examples/video_on_demand/video_on_demand
```

Open Google-Chrome and go to https://raspberrypi.local. You can see the video file.
