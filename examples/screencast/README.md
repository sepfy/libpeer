# Screencast

Cast your entire screen or chrome tab to Raspberry Pi and display on the monitor.

### Hardware Configuration

* Raspberry Pi 4B
* HDMI Monitor

### Run Example

```
cd pear && mkdir cmake && cd cmake
cmake -DENABLE_EXAMPLES=on ..
make
./examples/screencast/screencast
```

Open Google-Chrome and go to https://raspberrypi.local. Choose entire screen or Chrome tab. Raspberry Pi will display the content on the HDMI

