#ifndef PLATFORM_SOCKET_H_
#define PLATFORM_SOCKET_H_

#ifdef _WIN32
#include <winsock2.h>

// WinAPI's send/recv use `char*` instead of `void*` for buffers
#define send(s, buf, len, flags) send(s, (const char*)(buf), len, flags)
#define recv(s, buf, len, flags) recv(s, (char*)(buf), len, flags)
#define sendto(s, buf, len, flags, to, tolen) sendto(s, (const char*)(buf), len, flags, to, tolen)
#define recvfrom(s, buf, len, flags, from, fromlen) recvfrom(s, (char*)(buf), len, flags, from, fromlen)
#else
#include <arpa/inet.h>
#include <sys/socket.h>

// To prevent accidents while developing in POSIX,
// we'll make use of Windows-named `closesocket()` instead of just `close()`.
#define closesocket(s) close(s)
#endif

#endif // PLATFORM_SOCKET_H_
