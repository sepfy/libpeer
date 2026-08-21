#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "address.h"
#include "utils.h"

void addr_set_family(Address* addr, int family) {
  switch (family) {
#if CONFIG_USE_IPV6
    case AF_INET6:
      addr->family = AF_INET6;
      break;
#endif
    case AF_INET:
    default:
      addr->family = AF_INET;
      break;
  }
}

void addr_set_port(Address* addr, uint16_t port) {
  addr->port = port;
  switch (addr->family) {
#if CONFIG_USE_IPV6
    case AF_INET6:
      addr->sin6.sin6_port = htons(port);
      break;
#endif
    case AF_INET:
    default:
      addr->sin.sin_port = htons(port);
      break;
  }
}

int addr_from_string(const char* buf, Address* addr) {
  if (inet_pton(AF_INET, buf, &(addr->sin.sin_addr)) == 1) {
    addr_set_family(addr, AF_INET);
    return 1;
  }
#if CONFIG_USE_IPV6
  else if (inet_pton(AF_INET6, buf, &(addr->sin6.sin6_addr)) == 1) {
    addr_set_family(addr, AF_INET6);
    return 1;
  }
#endif
  return 0;
}

int addr_to_string(const Address* addr, char* buf, size_t len) {
  memset(buf, 0, len);
  switch (addr->family) {
#if CONFIG_USE_IPV6
    case AF_INET6:
      return inet_ntop(AF_INET6, &addr->sin6.sin6_addr, buf, len) != NULL;
#endif
    case AF_INET:
    default:
      return inet_ntop(AF_INET, &addr->sin.sin_addr, buf, len) != NULL;
  }
  return 0;
}

int addr_equal(const Address* a, const Address* b) {
  if (!a || !b) return 0;
  if (a->family != b->family) return 0;
  if (a->port != b->port) return 0;

  switch (a->family) {
    case AF_INET:
      return a->sin.sin_addr.s_addr == b->sin.sin_addr.s_addr;
#if CONFIG_USE_IPV6
    case AF_INET6:
      return memcmp(&a->sin6.sin6_addr, &b->sin6.sin6_addr,
                    sizeof(struct in6_addr)) == 0;
#endif
    default:
      return 0;
  }
}
