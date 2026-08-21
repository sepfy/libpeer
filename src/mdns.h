#ifndef MDNS_H_
#define MDNS_H_

#include <stdint.h>
#include <stdlib.h>

#include "address.h"

int mdns_parse_response(const uint8_t* packet, int size, Address* addr,
                        const char* hostname);

int mdns_resolve_addr(const char* hostname, Address* addr);

#endif  // MDNS_H_
