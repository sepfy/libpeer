#include "misc.h"

#ifdef _WIN32
#include <winsock2.h>

#ifndef __MINGW32__
#include <assert.h>

typedef VOID WINAPI FN_GetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime);
static FN_GetSystemTimeAsFileTime* WINAPI_GetSystemTimeAsFileTime;
#endif
#endif

int platform_init(void)
{
#ifdef _WIN32
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(2, 2), &wsadata))
        return 0;

#ifndef __MINGW32__
    FN_GetSystemTimeAsFileTime* ptr = (FN_GetSystemTimeAsFileTime*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
    if(!ptr) ptr = GetSystemTimeAsFileTime;
#endif
#endif
    return 1;
}

void platform_deinit(void)
{
#ifdef _WIN32
    WSACleanup();
#else
#endif
}

#if defined(_WIN32) && !defined(__MINGW32__)
// https://stackoverflow.com/a/59359900/485088
int platform_gettimeofday(struct timeval* tv, const void* tz) {
  if(!tv) return 0;
  assert(!tz);  // we don't support non-NULL timezones

  FILETIME               filetime; /* 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 00:00 UTC */
  ULARGE_INTEGER         x;
  ULONGLONG              usec;
  static const ULONGLONG epoch_offset_us = 11644473600000000ULL; /* microseconds betweeen Jan 1,1601 and Jan 1,1970 */

  // Win8 and higher has a better-precision timer, so we'll try to fetch that first.
  if(!WINAPI_GetSystemTimeAsFileTime)
  {
    // kernel32.dll is always loaded
    FN_GetSystemTimeAsFileTime* ptr = (FN_GetSystemTimeAsFileTime*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
    if(!ptr) ptr = GetSystemTimeAsFileTime;
    // race conditions in this exchange are not a problem here, as the operation is idempotent
    InterlockedExchangePointer((volatile void*)WINAPI_GetSystemTimeAsFileTime, ptr);
  }

  WINAPI_GetSystemTimeAsFileTime(&filetime);
  x.LowPart =  filetime.dwLowDateTime;
  x.HighPart = filetime.dwHighDateTime;
  usec = x.QuadPart / 10  -  epoch_offset_us;
  tv->tv_sec  = (time_t)(usec / 1000000ULL);
  tv->tv_usec = (long)(usec % 1000000ULL);
  return 0;
}
#endif

#ifdef _WIN32
char* platform_strndup(const char* str, size_t size)
{
    size_t len = strnlen(str, size);
    char* ret = malloc(len + 1);
    memcpy(ret, str, len);
    ret[len] = 0;
    return ret;
}
#endif
