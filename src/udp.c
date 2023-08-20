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

int udp_socket_open(UdpSocket *udp_socket) {

  udp_socket->fd = socket(AF_INET, SOCK_DGRAM, 0);

  if (udp_socket->fd < 0) {

    LOGE("Failed to create socket");
    return -1;
  }

  //int flags = fcntl(udp_socket->fd, F_GETFL, 0);

  //fcntl(udp_socket->fd, F_SETFL, flags | O_NONBLOCK);
  return 0;
}

int udp_socket_bind(UdpSocket *udp_socket, Address *addr) {

  if (udp_socket->fd < 0) {
    LOGE("Failed to create socket");
    return -1;
  }

  struct sockaddr_in sin;
  socklen_t sin_len = sizeof(sin);
  sin.sin_family = AF_INET;
  sin.sin_port = htons(addr->port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(udp_socket->fd, (struct sockaddr *)&sin, sin_len) < 0) {

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

  fd_set write_set;
  struct timeval tv;
  struct sockaddr_in sin;
  int ret = -1;

  if (udp_socket->fd < 0) {

    LOGE("sendto before socket init");
    return -1;
  }

  FD_ZERO(&write_set);
  FD_SET(udp_socket->fd, &write_set);

  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  sin.sin_family = AF_INET;

  memcpy(&sin.sin_addr.s_addr, addr->ipv4, 4);

  //LOGD("s_addr: %d", sin.sin_addr.s_addr);

  sin.sin_port = htons(addr->port);

  //LOGD("sendto addr %d.%d.%d.%d (%d)", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3], addr->port);

  if ((ret = select(udp_socket->fd + 1, NULL, &write_set, NULL, &tv)) < 0) {

    LOGE("Failed to select: %s", strerror(errno));
    return -1;
  }

  if (FD_ISSET(udp_socket->fd, &write_set)) {

    ret = sendto(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));

    if (ret < 0) {

      LOGE("Failed to sendto: %s", strerror(errno));
      return -1;
    }
  }

  return ret;
}

int udp_socket_recvfrom(UdpSocket *udp_socket, Address *addr, uint8_t *buf, int len) {

  fd_set read_set;
  struct timeval tv;
  struct sockaddr_in sin;
  int ret;

  if (udp_socket->fd < 0) {

    LOGE("recvfrom before socket init");
    return -1; 
  }

  FD_ZERO(&read_set);
  FD_SET(udp_socket->fd, &read_set);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;

  socklen_t sin_len = sizeof(sin);

  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;

  sin.sin_port = htons(addr->port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  if ((ret = select(udp_socket->fd + 1, &read_set, NULL, NULL, &tv)) < 0) {

    LOGE("Failed to select: %s", strerror(errno));
    return -1;
  }

  if (FD_ISSET(udp_socket->fd, &read_set)) {

    ret = recvfrom(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, &sin_len);

    if (ret < 0) {

      LOGE("Failed to recvfrom: %s", strerror(errno));
      return -1;
    }
  }

  return ret;
}

