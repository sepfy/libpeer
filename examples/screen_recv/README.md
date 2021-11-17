# Recording

A HTTP server based on libevent for SDP exchange and record screen to file.

### Pre-requisite

Install dependencies
```
sudo apt-get install -y libglib2.0-dev libevent-dev
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
./examples/recording
```

Open google-chrome and go to \<ip address\>:8080. Choose entire screen. It will save your screen to record.h264.

Play the video with the following command:
```
ffplay record.h264
```
