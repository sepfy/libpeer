#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"

#define LIBPEER_LOG_LEVEL_ERROR 0x00
#define LIBPEER_LOG_LEVEL_WARN 0x01
#define LIBPEER_LOG_LEVEL_INFO 0x02
#define LIBPEER_LOG_LEVEL_DEBUG 0x03

#define ERROR_TAG "ERROR"
#define WARN_TAG "WARN"
#define INFO_TAG "INFO"
#define DEBUG_TAG "DEBUG"

#ifndef LIBPEER_LOG_LEVEL
#define LIBPEER_LOG_LEVEL LIBPEER_LOG_LEVEL_INFO
#endif

#ifndef LIBPEER_LOG_PRINT
#define LIBPEER_LOG_PRINT(level_tag, fmt, ...) \
  fprintf(stdout, "%s\t%s\t%d\t" fmt "\n", level_tag, __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#if LIBPEER_LOG_LEVEL >= LIBPEER_LOG_LEVEL_DEBUG
#define LOGD(fmt, ...) LIBPEER_LOG_PRINT(DEBUG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGD(fmt, ...)
#endif

#if LIBPEER_LOG_LEVEL >= LIBPEER_LOG_LEVEL_INFO
#define LOGI(fmt, ...) LIBPEER_LOG_PRINT(INFO_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGI(fmt, ...)
#endif

#if LIBPEER_LOG_LEVEL >= LIBPEER_LOG_LEVEL_WARN
#define LOGW(fmt, ...) LIBPEER_LOG_PRINT(WARN_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...)
#endif

#if LIBPEER_LOG_LEVEL >= LIBPEER_LOG_LEVEL_ERROR
#define LOGE(fmt, ...) LIBPEER_LOG_PRINT(ERROR_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...)
#endif

#define ALIGN32(num) ((num + 3) & ~3)

void utils_random_string(char* s, const int len);

void utils_get_hmac_sha1(const char* input, size_t input_len, const char* key, size_t key_len, unsigned char* output);

void utils_get_md5(const char* input, size_t input_len, unsigned char* output);

#endif  // UTILS_H_
