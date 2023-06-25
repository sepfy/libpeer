#include <string.h>
#include <sys/types.h>
#include <net/if.h>

#ifdef ESP32
#include <mdns.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#endif

#include "ports.h"
#include "utils.h"

static char g_current_ip[INET_ADDRSTRLEN] = {0};

void ports_set_current_ip(const char *ip) {

  snprintf(g_current_ip, INET_ADDRSTRLEN, "%s", ip);
}

int ports_get_current_ip(UdpSocket *udp_socket, Address *addr) {

  int ret = 0;

#ifdef ESP32
  struct sockaddr_in sa;

  inet_pton(AF_INET, g_current_ip, &(sa.sin_addr));

  memcpy(addr->ipv4, &sa.sin_addr.s_addr, 4);

  ret = 1;
#else
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

      if (ioctl(udp_socket->fd, SIOCGIFADDR, &ifr) == 0) {

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


int ports_resolve_mdns_host(const char *host, Address *addr) {

  int ret = -1;

#ifdef ESP32

  struct esp_ip4_addr esp_addr;
  char host_name[64] = {0};
  char *pos = strstr(host, ".local"); 
  snprintf(host_name, pos - host + 1, "%s", host);
  esp_addr.addr = 0;
    
  esp_err_t err = mdns_query_a(host_name, 2000,  &esp_addr);
  if (err) {
    if (err == ESP_ERR_NOT_FOUND) {
      LOGW("%s: Host was not found!", esp_err_to_name(err));
      return ret;
    }
    LOGE("Query Failed: %s", esp_err_to_name(err));
      return ret;
  }

  memcpy(addr->ipv4, &esp_addr.addr, 4);

#else
  struct addrinfo hints, *res, *p;
  int status;
  char ipstr[INET6_ADDRSTRLEN];
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    //LOGE("getaddrinfo error: %s\n", gai_strerror(status));
    LOGE("getaddrinfo error: %d\n", status);
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
#endif

  return ret;
}


