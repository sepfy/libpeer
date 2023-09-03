#ifndef UDP_SOCKET_H_
#define UDP_SOCKET_H_

#include "address.h"

typedef struct UdpSocket UdpSocket;

struct UdpSocket {

  int fd;
  Address bind_addr;
  uint64_t timeout;
};

int udp_socket_open(UdpSocket *udp_socket);

int udp_socket_bind(UdpSocket *udp_socket, Address *addr);

void udp_socket_close(UdpSocket *udp_socket);

int udp_socket_sendto(UdpSocket *udp_socket, Address *addr, const uint8_t *buf, int len);

int udp_socket_recvfrom(UdpSocket *udp_socket, Address *addr, uint8_t *buf, int len);

int udp_socket_get_host_address(UdpSocket *udp_socket, Address *addr);

int udp_resolve_mdns_host(const char *host, Address *addr);

#endif // UDP_SOCKET_H_

