#ifndef ADDRESS_H_
#define ADDRESS_H_

#include <stdint.h>

typedef struct Address Address;

struct Address {

  uint8_t family;
  uint16_t port;
  uint8_t ipv4[4];
  uint8_t ipv6[16];

}__attribute__((packed));

int addr_ipv4_validate(const char *ipv4, size_t len, Address *addr);

#endif // ADDRESS_H_
