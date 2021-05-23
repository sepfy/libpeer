# GStreamer

A HTTP server based on libevent for SDP exchange and streaming GStreamer video pipeline to browser.

### Pre-requisite

Install dependencies
```
sudo apt-get install -y gstreamer1.0-tools gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-plugins-good libgstreamer1.0-dev libglib2.0-dev libgstreamer-plugins-bad1.0-dev libevent-dev
```

Install JSON parser library
```
git clone https://github.com/DaveGamble/cJSON.git
cd cJSON
make
sudo make install
sudo cp libcjson* /usr/lib/
```

### Build example
```
cd pear && mkdir cmake && cd cmake
cmake -DENABLE_EXAMPLES=on ..
make
./examples/gstreamer/gstreamer
```

Open google-chrome and go to \<ip address\>:8080. You will see the streaming video from GStreamer.

### With Raspberry Pi
```
sudo modprobe -r bcm2835_v4l2
sudo modprobe bcm2835_v4l2 gst_v4l2src_is_broken=1
sudo apt-get install gstreamer1.0-omx-rpi gstreamer1.0-omx
```

Modify GStreamer pipeline in examples/gstreamer/main.c to
```
v4l2src device=/dev/video0 ! videorate ! video/x-raw,width=1280,height=720,framerate=30/1 ! omxh264enc target-bitrate=1000000 control-rate=variable ! video/x-h264,profile=high ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink
```
The latency of Raspberry Pi Zero W with 720P video
![rpi0](https://raw.githubusercontent.com/sepfy/readme-image/main/pear-rpi0.jpg)

### With RTSP source
Run RTSP server testProgs/testH264VideoStreamer in live555 library

Modify GStreamer pipeline in examples/gstreamer/main.c to
```
rtspsrc location=rtsp://127.0.0.1:8554/testStream ! rtph264depay ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink
```
