#ifndef CONFIG_H_
#define CONFIG_H_

// uncomment this if you want to handshake with a aiortc
#define CONFIG_DTLS_USE_ECDSA 1

#define SCTP_MTU (1200)
#define CONFIG_MTU (1300)

#ifndef CONFIG_USE_LWIP
#define CONFIG_USE_LWIP 0
#endif

#ifndef CONFIG_MBEDTLS_DEBUG
#define CONFIG_MBEDTLS_DEBUG 0
#endif

#ifndef CONFIG_MBEDTLS_2_X
#define CONFIG_MBEDTLS_2_X 0
#endif

#if CONFIG_MBEDTLS_2_X
#define RSA_KEY_LENGTH 512
#else
#define RSA_KEY_LENGTH 1024
#endif

#ifndef CONFIG_DTLS_USE_ECDSA
#define CONFIG_DTLS_USE_ECDSA 0
#endif

#ifndef CONFIG_USE_USRSCTP
#define CONFIG_USE_USRSCTP 1
#endif

#ifndef CONFIG_VIDEO_BUFFER_SIZE
#define CONFIG_VIDEO_BUFFER_SIZE (CONFIG_MTU * 256)
#endif

#ifndef CONFIG_AUDIO_BUFFER_SIZE
#define CONFIG_AUDIO_BUFFER_SIZE (CONFIG_MTU * 256)
#endif

#ifndef CONFIG_DATA_BUFFER_SIZE
#define CONFIG_DATA_BUFFER_SIZE (SCTP_MTU * 128)
#endif

#ifndef CONFIG_SDP_BUFFER_SIZE
#define CONFIG_SDP_BUFFER_SIZE 8096
#endif

#ifndef CONFIG_MQTT_BUFFER_SIZE
#define CONFIG_MQTT_BUFFER_SIZE 4096
#endif

#ifndef CONFIG_HTTP_BUFFER_SIZE
#define CONFIG_HTTP_BUFFER_SIZE 4096
#endif

#define AUDIO_LATENCY 20  // ms
#define KEEPALIVE_CONNCHECK 10000
#define CONFIG_IPV6 0
// empty will use first active interface
#define CONFIG_IFACE_PREFIX ""

// #define LOG_LEVEL LEVEL_DEBUG
#define LOG_REDIRECT 0

// Disable MQTT and HTTP signaling
// #define DISABLE_PEER_SIGNALING 1

#endif  // CONFIG_H_
