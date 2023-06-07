#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>

#if 1
#include <netdb.h>
#include <ifaddrs.h>
#endif
#include <net/if.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "utils.h"
#include "udp.h"

int udp_socket_open(UdpSocket *udp_socket) {

  udp_socket->fd = socket(AF_INET, SOCK_DGRAM, 0);

  int flags = fcntl(udp_socket->fd, F_GETFL, 0);

  fcntl(udp_socket->fd, F_SETFL, flags | O_NONBLOCK);
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

int udp_socket_sendto(UdpSocket *udp_socket, Address *addr, const uint8_t *buf, int len) {

  if (udp_socket->fd < 0) {

    LOGE("sendto before socket init");
    return -1;
  }

  struct sockaddr_in sin;

  sin.sin_family = AF_INET;

  memcpy(&sin.sin_addr.s_addr, addr->ipv4, 4);

  //LOGD("s_addr: %d", sin.sin_addr.s_addr);

  sin.sin_port = htons(addr->port);

  //LOGD("sendto addr %d.%d.%d.%d (%d)", addr->ipv4[0], addr->ipv4[1], addr->ipv4[2], addr->ipv4[3], addr->port);
  int ret = sendto(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, sizeof(sin));

  if (ret < 0) {
    LOGE("Failed to sendto");
    return -1;
  }

  return ret;
}

int udp_socket_recvfrom(UdpSocket *udp_socket, Address *addr, uint8_t *buf, int len) {

  struct sockaddr_in sin;

  int ret;

  if (udp_socket->fd < 0) {

    LOGE("recvfrom before socket init");
    return -1; 
  }

  socklen_t sin_len = sizeof(sin);

  memset(&sin, 0, sizeof(sin));

  sin.sin_family = AF_INET;

  sin.sin_port = htons(addr->port);
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
#if 0
  fd_set read_set;
  FD_ZERO(&read_set);
  FD_SET(udp_socket->fd, &read_set);

  struct timeval tv;
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  ret = select(udp_socket->fd + 1, &read_set, NULL, NULL, &tv);

  if (ret < 0) {

    LOGE("select() failed");
    return -1;

  } else if (ret == 0) {

    LOGD("select() timeout");
    return 0;
  }
#endif
  ret = recvfrom(udp_socket->fd, buf, len, 0, (struct sockaddr *)&sin, &sin_len);
  if (ret < 0) {

    if (errno == EWOULDBLOCK || errno == EAGAIN) {

    } else  {

      LOGE("recvfrom() failed %d", ret);
    }
    return -1;
  }

  return ret;
  
}

int udp_socket_get_host_address(UdpSocket *udp_socket, Address *addr) {
  int ret = 0;
#if 0

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
#endif
  return ret;
}


int udp_resolve_mdns_host(const char *host, Address *addr) {

  int ret = -1;
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    LOGE("getaddrinfo error: %s\n", gai_strerror(status));
    return ret;
  }

 for (p = res; p != NULL; p = p->ai_next) {

    if (p->ai_family == AF_INET) {
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      ret = 0;
      memcpy(addr->ipv4, &ipv4->sin_addr.s_addr, 4);
    }
  }

  freeaddrinfo(res); 
  return ret;
}


