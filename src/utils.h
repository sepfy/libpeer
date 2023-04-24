#ifndef UTILS_H_
#define UTILS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LEVEL_ERROR 0x00
#define LEVEL_WARN 0x01
#define LEVEL_INFO 0x02
#define LEVEL_DEBUG 0x03

#define ERROR_TAG "ERROR"
#define WARN_TAG "WARN"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"

#ifndef LOG_LEVEL
#define LOG_LEVEL LEVEL_DEBUG
#endif

#define LOG_PRINT(level_tag, fmt, ...) \
  fprintf(stdout, "[%s %s:%d] " fmt"\n", level_tag, __FILE__, __LINE__, ##__VA_ARGS__)

#if LOG_LEVEL >= LEVEL_DEBUG
#define LOGD(fmt, ...) LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)
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

typedef struct Buffer {

  uint8_t *data;
  int size;
  int head;
  int tail;

} Buffer;

int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size);

int utils_is_valid_ip_address(char *ip_address);

Buffer* utils_buffer_new(int size);

int utils_buffer_push(Buffer *rb, uint8_t *data, int size);
   
int utils_buffer_pop(Buffer *rb, uint8_t *data, int size);

#endif // UTILS_H_
