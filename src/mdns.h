#ifndef MDNS_H_
#define MDNS_H_

#include <stdlib.h>
#include "address.h"

int mdns_resolve_addr(const char* hostname, Address* addr);

#endif  // MDNS_H_
