#ifndef PLATFORM_MISC_H_
#define PLATFORM_MISC_H_

int platform_init(void);
void platform_deinit(void);

// In MinGW, we'll just use <sys/time.h> as well.
#if defined(_WIN32) && !defined(__MINGW32__)
#include <winsock2.h>
int platform_gettimeofday(struct timeval* tv, const void* tz);
#else
#include <sys/time.h>
#define platform_gettimeofday(tv, tz) gettimeofday(tv, tz);
#endif

#include <string.h>
#ifdef _WIN32
char* platform_strndup(const char* str, size_t size);
#else
#define platform_strndup(str, size) strndup(str, size)
#endif

#if defined(_WIN32) && !defined(__MINGW32__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define platform_sleep_ms(ms)   Sleep(ms)
#else
#include <unistd.h>
#define platform_sleep_ms(ms)   usleep((ms) * 1000ULL)
#endif


#endif /* PLATFORM_MISC_H_ */
