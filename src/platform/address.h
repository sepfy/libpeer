#ifndef PLATFORM_ADDRESS_H_
#define PLATFORM_ADDRESS_H_

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

#endif /* PLATFORM_ADDRESS_H_ */
