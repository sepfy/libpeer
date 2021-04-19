# Signal

A HTTP server based on libevent for exchange SDP. 

### Pre-requisite

Download dependencies
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
cp ../examples/signal/index.html .
./examples/signal/signal
```

Open google-chrome and go to 127.0.0.1:8080. You will see the streaming image from gstreamer.
