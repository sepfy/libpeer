#include <stdio.h>
#include <stdlib.h>

#define LEVEL_ERROR 0x00
#define LEVEL_WARNING 0x01
#define LEVEL_INFO 0x02
#define LEVEL_DEBUG 0x03

#define ERROR_TAG "ERROR"
#define WARNING_TAG "WARNING"
#define INFO_TAG "INFO"

#ifndef LOG_LEVEL
#define LOG_LEVEL LEVEL_INFO
#endif

#define LOG_PRINT(level_tag, fmt, arg...) \
  fprintf(stdout, "[%s %s:%d] " fmt"\n", level_tag, __FILE__, __LINE__, ##arg)

#if LOG_LEVEL >= LEVEL_INFO
#define LOG_INFO(fmt, arg...) LOG_PRINT(INFO_TAG, fmt, ##arg)
#else
#define LOG_INFO(fmt, arg...)
#endif

#if LOG_LEVEL >= LEVEL_WARNING
#define LOG_WARNING(fmt, arg...) LOG_PRINT(WARNING_TAG, fmt, ##arg)
#else
#define LOG_WARNING(fmt, arg...)
#endif

#if LOG_LEVEL >= LEVEL_ERROR
#define LOG_ERROR(fmt, arg...) LOG_PRINT(ERROR_TAG, fmt, ##arg)
#else
#define LOG_ERROR(fmt, arg...)
#endif

int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size);

int utils_is_valid_ip_address(char *ip_address);
