#ifndef ADDRESS_H_
#define ADDRESS_H_

#include <stdint.h>

typedef struct Address Address;

struct Address {

  uint8_t family;
  uint16_t port;
  uint8_t ipv4[4];
  uint16_t ipv6[8];

};

int addr_ipv6_validate(const char *ipv6, size_t len, Address *addr);

int addr_ipv4_validate(const char *ipv4, size_t len, Address *addr);

int addr_to_text(const Address *addr, char *buf, size_t len);

int addr_equal(const Address *a, const Address *b);

#endif // ADDRESS_H_
