#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "utils.h"
#include "udp.h"

int udp_socket_open(UdpSocket *udp_socket) {

  udp_socket->fd = socket(AF_INET, SOCK_DGRAM, 0);

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

  if (udp_socket->fd > 0)
    close(udp_socket->fd);
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

int udp_socket_sendto(UdpSocket *udp_socket, Address *addr, const char *buf, int len) {

  if (udp_socket->fd < 0) {

    LOGE("sendto before socket init");
    return -1;
  }

  struct sockaddr_in sin;

  sin.sin_family = AF_INET;

  memcpy(&sin.sin_addr.s_addr, addr->ipv4, 4);

  //LOGD("s_addr: %d", sin.sin_addr.s_addr);

  sin.sin_port = htons(addr->port);

  LOGD("sendto addr %d.%d.%d.%d (%d)", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3], addr->port);
  int ret = sendto(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));

  if (ret < 0) {
    LOGE("Failed to sendto");
    return -1;
  }

  return ret;
}

int udp_socket_recvfrom(UdpSocket *udp_socket, Address *addr, char *buf, int len) {

  if (udp_socket->fd < 0) {

    LOGE("recvfrom before socket init");
    return -1; 
  }

  struct sockaddr_in sin;

  socklen_t sin_len = sizeof(sin);

  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;

  sin.sin_port = htons(addr->port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);

  LOGD("recvfrom addr %d.%d.%d.%d (%d)", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3], addr->port);
  int ret = recvfrom(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, &sin_len);
  if (ret < 0) {

    LOGE("recvfrom() failed");
    return -1;
  }

  return ret;
  
}

int udp_socket_get_host_address(UdpSocket *udp_socket, Address *addr) {

  int ret = 0;

  struct ifaddrs *addrs,*tmp;

  struct ifreq ifr;

  if (udp_socket->fd < 0) {

    LOGE("get_host_address before socket init");
    return -1;
  }


  getifaddrs(&addrs);

  tmp = addrs;

  while (tmp) {

    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {

      strncpy(ifr.ifr_name, tmp->ifa_name, IFNAMSIZ); 

      if (strstr(ifr.ifr_name, "wlx2887ba6688d7") != 0 && ioctl(udp_socket->fd, SIOCGIFADDR, &ifr) == 0) {

        LOGD("interface: %s, address: %s", ifr.ifr_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

        addr[ret].family = AF_INET;
        memcpy(addr[ret].ipv4, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr, 4);
        ret++;
      }


    }

    tmp = tmp->ifa_next;
  }

  freeifaddrs(addrs);
  return ret;
}



