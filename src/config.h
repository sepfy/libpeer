#ifndef CONFIG_H_
#define CONFIG_H_

#include "esp_idf_version.h"

#define SCTP_MTU (1200)
#define CONFIG_MTU (1300)


#ifdef ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 2)
    #define RSA_KEY_LENGTH 1024
#else
    #define RSA_KEY_LENGTH 512
#endif
#define VIDEO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define AUDIO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define DATA_RB_DATA_LENGTH (SCTP_MTU * 128)
#define AUDIO_LATENCY 40 // ms
#else
#define HAVE_USRSCTP
#define RSA_KEY_LENGTH 2048
#define VIDEO_RB_DATA_LENGTH (CONFIG_MTU * 256)
#define AUDIO_RB_DATA_LENGTH (CONFIG_MTU * 256)
#define DATA_RB_DATA_LENGTH (SCTP_MTU * 128)
#define AUDIO_LATENCY 20 // ms
#endif

#ifndef CONFIG_MQTT
#define CONFIG_MQTT 1
#endif

#ifndef CONFIG_WHIP
#define CONFIG_WHIP 0
#endif

// siganling
#define MQTT_HOST "test.mosquitto.org"
#define MQTT_PORT 8883

#define WHIP_HOST "192.168.1.110"
#define WHIP_PATH "/index/api/whip?app=live&stream=test"
#define WHIP_PORT 443

#define KEEPALIVE_CONNCHECK 0

// default use wifi interface
#define IFR_NAME "w"

//#define LOG_LEVEL LEVEL_DEBUG

#endif // CONFIG_H_
