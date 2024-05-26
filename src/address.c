#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "address.h"

int addr_ipv4_validate(const char *ipv4, size_t len, Address *addr) {

  int start = 0;
  int index = 0;
  for (int i = 0; i < len; i++) {

    if (ipv4[i] != '.' && (ipv4[i] < '0' || ipv4[i] > '9')) {
      return 0;
    } else if (ipv4[i] == '.') {

      addr->ipv4[index++] = atoi(ipv4 + start);
      start = i + 1;
    } else if (i == (len - 1)) {

      addr->ipv4[index++] = atoi(ipv4 + start);
    } else if (index == 4) {
      return 0;
    }
  }
  addr->family = AF_INET;
  return 1;
}

int addr_ipv6_validate(const char *ipv6, size_t len, Address *addr) {
  int ret;
  struct sockaddr_in6 sa6;
  char astring[INET6_ADDRSTRLEN];
  ret = inet_pton(AF_INET6, ipv6, &(sa6.sin6_addr));
  inet_ntop(AF_INET6, &(sa6.sin6_addr), astring, INET6_ADDRSTRLEN);
  memcpy(addr->ipv6, sa6.sin6_addr.s6_addr, 16);
  return ret;
}

int addr_to_text(const Address *addr, char *buf, size_t len) {

  switch (addr->family) {
   case AF_INET:
    return inet_ntop(AF_INET, addr->ipv4, buf, len) != NULL;
    break;
   case AF_INET6:
    return inet_ntop(AF_INET6, addr->ipv6, buf, len) != NULL;
    break;
  }
  return 0;
}

int addr_equal(const Address *a, const Address *b) {
  if (a->family != b->family) {
    return 0;
  }

  switch (a->family) {
   case AF_INET:
    for (int i = 0; i < 4; i++) {
      if (a->ipv4[i] != b->ipv4[i]) {
	return 0;
      }
    }
    break;
   case AF_INET6:
    break;
  }

  return 1;
}
