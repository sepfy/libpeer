#ifndef ADDRESS_H_
#define ADDRESS_H_

#include <stdint.h>

typedef struct {

  uint8_t family;
  uint16_t port;
  uint8_t ipv4[4];
  uint8_t ipv6[16];

} Address;

#endif // ADDRESS_H_
