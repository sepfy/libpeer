#include <stdio.h>
#include <stdlib.h>

#define LEVEL_ERROR 0x00
#define LEVEL_WARN 0x01
#define LEVEL_INFO 0x02
#define LEVEL_DEBUG 0x03

#define ERROR_TAG "ERROR"
#define WARN_TAG "WARN"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"

#ifndef LOG_LEVEL
#define LOG_LEVEL LEVEL_INFO
#endif

#define LOG_PRINT(level_tag, fmt, ...) \
  fprintf(stdout, "[%s %s:%d] " fmt"\n", level_tag, __FILE__, __LINE__, ##__VA_ARGS__)

#if LOG_LEVEL >= LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_INFO
#define LOG_INFO(fmt, ...) LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_WARN
#define LOG_WARN(fmt, ...) LOG_PRINT(WARN_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL >= LEVEL_ERROR
#define LOG_ERROR(fmt, ...) LOG_PRINT(ERROR_TAG, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...)
#endif

int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size);

int utils_is_valid_ip_address(char *ip_address);
