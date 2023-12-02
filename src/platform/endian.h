#ifndef PLATFORM_ENDIAN_H_
#define PLATFORM_ENDIAN_H_

#if defined(_WIN32) || defined(ESP32)
#define PLATFORM_BIG_ENDIAN 4321
#define PLATFORM_LITTLE_ENDIAN 1234
#define PLATFORM_BYTE_ORDER PLATFORM_LITTLE_ENDIAN
#else
#include <endian.h>
#define PLATFORM_BIG_ENDIAN __BIG_ENDIAN
#define PLATFORM_LITTLE_ENDIAN __LITTLE_ENDIAN
#define PLATFORM_BYTE_ORDER __BYTE_ORDER
#endif

#endif /* PLATFORM_ENDIAN_H_ */
