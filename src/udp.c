#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>


#include "utils.h"
#include "udp.h"

void udp_blocking_timeout(UdpSocket *udp_socket, long long int ms) {

  udp_socket->timeout_sec = ms/1000;
  udp_socket->timeout_usec = ms%1000*1000;
}

int udp_socket_create(UdpSocket *udp_socket, int family) {

  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct sockaddr *sa;
  socklen_t sin_len;

  switch (family) {
    case AF_INET6:
      udp_socket->fd = socket(AF_INET6, SOCK_DGRAM, 0);
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(0);
      sin6.sin6_addr = in6addr_any;
      sa = (struct sockaddr *)&sin6;
      sin_len = sizeof(sin6);
      break;
    case AF_INET:
    default:
      udp_socket->fd = socket(AF_INET, SOCK_DGRAM, 0);
      sin.sin_family = AF_INET;
      sin.sin_port = htons(0);
      sin.sin_addr.s_addr = htonl(INADDR_ANY);
      sa = (struct sockaddr *)&sin;
      sin_len = sizeof(sin);
      break;
  }

  if (udp_socket->fd < 0) {

    LOGE("Failed to create socket");
    return -1;
  }

  if (bind(udp_socket->fd, sa, sin_len) < 0) {

    LOGE("Failed to bind socket");
    return -1;
  }

  if (getsockname(udp_socket->fd, (struct sockaddr *)&sin, &sin_len) < 0) {
    LOGE("Failed to get local address");
    return -1;
  }

  LOGI("local port: %d", ntohs(sin.sin_port));
  udp_socket->bind_addr.family = sin.sin_family;
  udp_socket->bind_addr.port = ntohs(sin.sin_port);
  return 0;
}

int udp_socket_open(UdpSocket *udp_socket, int family) {

  switch (family) {
    case AF_INET6:
      udp_socket->fd = socket(AF_INET6, SOCK_DGRAM, 0);
      break;
    case AF_INET:
    default:
      udp_socket->fd = socket(AF_INET, SOCK_DGRAM, 0);
      break;
  }

  if (udp_socket->fd < 0) {

    LOGE("Failed to create socket");
    return -1;
  }

  //int flags = fcntl(udp_socket->fd, F_GETFL, 0);
  //fcntl(udp_socket->fd, F_SETFL, flags | O_NONBLOCK);
  udp_blocking_timeout(udp_socket, 5);
  return 0;
}

int udp_socket_bind(UdpSocket *udp_socket, Address *addr) {

  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct sockaddr *sa;
  socklen_t sin_len;

  if (udp_socket->fd < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  switch (addr->family) {
    case AF_INET6:
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(addr->port);
      sin6.sin6_addr = in6addr_any;
      sa = (struct sockaddr *)&sin6;
      sin_len = sizeof(sin6);
      break;
    case AF_INET:
    default:
      sin.sin_family = AF_INET;
      sin.sin_port = htons(addr->port);
      sin.sin_addr.s_addr = htonl(INADDR_ANY);
      sa = (struct sockaddr *)&sin;
      sin_len = sizeof(sin);
      break;
  }

  if (bind(udp_socket->fd, sa, sin_len) < 0) {

    LOGE("Failed to bind socket");
    return -1;
  }

  udp_socket->bind_addr.family = addr->family;
  udp_socket->bind_addr.port = addr->port;
  memcpy(udp_socket->bind_addr.ipv4, addr->ipv4, 4);
  LOGD("bind_addr: %p", &udp_socket->bind_addr);
  return 0;
}

void udp_socket_close(UdpSocket *udp_socket) {

  if (udp_socket->fd > 0) {
    close(udp_socket->fd);
  }
}

int udp_get_local_address(UdpSocket *udp_socket, Address *addr) {

  struct sockaddr_in sin;

  socklen_t len = sizeof(sin);

  if (udp_socket->fd < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  if (getsockname(udp_socket->fd, (struct sockaddr *)&sin, &len) < 0) {
    LOGE("Failed to get local address");
    return -1;
  }

  memcpy(addr->ipv4, &sin.sin_addr.s_addr, 4);

  addr->port = ntohs(sin.sin_port);

  addr->family = AF_INET;

  LOGD("local port: %d", addr->port);
  LOGD("local address: %d.%d.%d.%d", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3]);

  return 0;
}

int udp_socket_sendto(UdpSocket *udp_socket, Address *addr, const uint8_t *buf, int len) {

  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct sockaddr *sa;
  socklen_t sin_len;
  int ret = -1;

  if (udp_socket->fd < 0) {

    LOGE("sendto before socket init");
    return -1;
  }

  switch (addr->family) {
    case AF_INET6:
      sin6.sin6_family = AF_INET6;
      sin6.sin6_port = htons(addr->port);
      memcpy(&sin6.sin6_addr, addr->ipv6, 16);
      sa = (struct sockaddr *)&sin6;
      sin_len = sizeof(sin6);
      break;
    case AF_INET:
    default:
      sin.sin_family = AF_INET;
      sin.sin_port = htons(addr->port);
      memcpy(&sin.sin_addr.s_addr, addr->ipv4, 4);
      sa = (struct sockaddr *)&sin;
      sin_len = sizeof(sin);
      break;
  }

  ret = sendto(udp_socket->fd, buf, len, 0, sa, sin_len);

  if (ret < 0) {

    LOGE("Failed to sendto: %s", strerror(errno));
    return -1;
  }

  return ret;
}

int udp_socket_recvfrom(UdpSocket *udp_socket, Address *addr, uint8_t *buf, int len) {

  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
  struct sockaddr *sa;
  socklen_t sin_len;
  int ret;

  if (udp_socket->fd < 0) {

    LOGE("recvfrom before socket init");
    return -1; 
  }

  switch (udp_socket->bind_addr.family) {
    case AF_INET6:
      sin_len = sizeof(sin6);
      sa = (struct sockaddr *)&sin6;
      break;
    case AF_INET:
    default:
      sin_len = sizeof(sin);
      sa = (struct sockaddr *)&sin;
      break;
  }

  ret = recvfrom(udp_socket->fd, buf, len, 0, sa, &sin_len);
  if (ret < 0) {
    LOGE("Failed to recvfrom: %s", strerror(errno));
    return -1;
  }

  if (addr) {
    switch (udp_socket->bind_addr.family) {
      case AF_INET6:
        addr->family = AF_INET6;
        addr->port = ntohs(sin6.sin6_port);
        memcpy(addr->ipv6, &sin6.sin6_addr, 16);
        break;
      case AF_INET:
      default:
        addr->family = AF_INET;
        addr->port = ntohs(sin.sin_port);
        memcpy(addr->ipv4, &sin.sin_addr.s_addr, 4);
        break;
    }
  }

  return ret;
}

