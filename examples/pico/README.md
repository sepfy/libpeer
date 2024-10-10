# RP2040 PICO W
Establish a DataChannel connection between PICO and the web

## Supported Devices
| Device |Image|
|---|---|
| [PICO W](https://www.raspberrypi.com/products/raspberry-pi-pico/) |<img src="https://piepie.s3.ap-northeast-1.amazonaws.com/wp-content/uploads/2023/02/04114837/Raspberry-Pi-Pico-W-1.png" width="64">|

## Setup Instructions
### Download Required Libraries
```
$ git clone --recursive https://github.com/sepfy/libpeer
$ git clone --recurisve https://github.com/raspberrypi/pico-sdk
$ git clone --recursive https://github.com/FreeRTOS/FreeRTOS-Kernel
```
### Apply a patch
Modify `libpeer/third_party/libsrtp/include/srtp.h`

```patch
@@ -614,7 +614,7 @@ srtp_err_status_t srtp_add_stream(srtp_t session, const srtp_policy_t *policy);
  *    - [other]           otherwise.
  *
  */
-srtp_err_status_t srtp_remove_stream(srtp_t session, unsigned int ssrc);
+srtp_err_status_t srtp_remove_stream(srtp_t session, uint32_t ssrc);

 /**
  * @brief srtp_update() updates all streams in the session.
```

### Configure Your Build
```
$ export PICO_SDK_PATH=<your PICO SDK path>/pico-sdk
$ export FREERTOS_KERNEL_PATH=<your FreeRTOS kernel path>/FreeRTOS-Kernel
$ export WIFI_SSID=<your WIFI SSID>
$ export WIFI_PASSWORD=<your WIFI password>
```

### Build the Project
```
$ cd libpeer/examples/pico
$ mkdir build
$ cd build
$ cmake ..
```

### Flash and Test
```
$ sudo picotool load -f pico_peer.uf2
```
