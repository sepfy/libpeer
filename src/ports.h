#ifndef PORTS_H_
#define PORTS_H_

#include <stdlib.h>
#include "address.h"
#include "udp.h"

int ports_resolve_addr(const char *host, Address *addr);

int ports_resolve_mdns_host(const char *host, Address *addr);

int ports_get_host_addr(Address *addr);

#endif // PORTS_H_
