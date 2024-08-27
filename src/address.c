#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "utils.h"
#include "address.h"

void addr_set_family(Address *addr, int family) {
  switch (family) {
    case AF_INET6:
      addr->family = AF_INET6;
      break;
    case AF_INET:
    default:
      addr->family = AF_INET;
      break;
  }
}

void addr_set_port(Address *addr, uint16_t port) {
  addr->port = port;
  switch (addr->family) {
    case AF_INET6:
      addr->sin6.sin6_port = htons(port);
      break;
    case AF_INET:
    default:
      addr->sin.sin_port = htons(port);
      break;
  }
}


int addr_from_string(const char *buf, Address *addr) {
  if (inet_pton(AF_INET, buf, &(addr->sin.sin_addr)) == 1) {
    addr_set_family(addr, AF_INET);
    return 1;
  } else if (inet_pton(AF_INET6, buf, &(addr->sin6.sin6_addr)) == 1) {
    addr_set_family(addr, AF_INET6);
    return 1;
  }
  return 0;
}

int addr_to_string(const Address *addr, char *buf, size_t len) {

  memset(buf, 0, sizeof(len));
  switch (addr->family) {
   case AF_INET6:
    return inet_ntop(AF_INET6, &addr->sin6.sin6_addr, buf, len) != NULL;
   case AF_INET:
   default:
    return inet_ntop(AF_INET, &addr->sin.sin_addr, buf, len) != NULL;
  }
  return 0;
}

int addr_equal(const Address *a, const Address *b) {
  // TODO
  return 1;
}
