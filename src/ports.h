#ifndef PORTS_H_
#define PORTS_H_

#include <stdlib.h>
#include "address.h"
#include "udp.h"

int ports_resolve_mdns_host(const char *host, Address *addr);

int ports_get_current_ip(UdpSocket *udp_socket, Address *addr);

void ports_set_current_ip(const char *ip);

#endif // PORTS_H_

