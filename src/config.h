#ifndef CONFIG_H_
#define CONFIG_H_

#define SCTP_MTU (1200)
#define CONFIG_MTU (1434)

#ifdef ESP32
#define RSA_KEY_LENGTH 512
#else
#define HAVE_USRSCTP
#define RSA_KEY_LENGTH 2048
#endif

#define VIDEO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define AUDIO_RB_DATA_LENGTH (CONFIG_MTU * 64)
#define DATA_RB_DATA_LENGTH (SCTP_MTU * 128)

#endif // CONFIG_H_
