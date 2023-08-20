#ifndef CONFIG_H_
#define CONFIG_H_

#define SCTP_MTU (1200)
#define CONFIG_MTU (1300)

#ifdef ESP32
#define RSA_KEY_LENGTH 512
#define VIDEO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define AUDIO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define DATA_RB_DATA_LENGTH (SCTP_MTU * 128)
#else
#define HAVE_USRSCTP
#define RSA_KEY_LENGTH 2048
#define VIDEO_RB_DATA_LENGTH (CONFIG_MTU * 256)
#define AUDIO_RB_DATA_LENGTH (CONFIG_MTU * 256)
#define DATA_RB_DATA_LENGTH (SCTP_MTU * 128)
#endif

// siganling
#define MQTT_HOST "mqtt.eclipseprojects.io"
#define MQTT_PORT "8883"

#define KEEPALIVE_CONNCHECK 10000

//#define LOG_LEVEL LEVEL_DEBUG

#endif // CONFIG_H_
