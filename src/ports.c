#include <string.h>
#include <sys/types.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef ESP32
#include <mdns.h>
#include <esp_netif.h>
#else
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <errno.h>
#endif

#include "ports.h"
#include "utils.h"

int ports_get_host_addr(Address *addr) {

  int ret = 0;

#ifdef ESP32
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  esp_netif_ip_info_t ip_info;
  esp_ip6_addr_t ip6_info;

  switch (addr->family) {
    case AF_INET6:
      if (esp_netif_get_ip6_global(netif, &ip6_info) == ESP_OK) {
        memcpy(addr->ipv6, &ip6_info.addr, 16);
        ret = 1;
      } else if (esp_netif_get_ip6_linklocal(netif, &ip6_info) == ESP_OK) {
        memcpy(addr->ipv6, &ip6_info.addr, 16);
        ret = 1;
      }
      break;
    case AF_INET:
    default:
      if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        memcpy(addr->ipv4, &ip_info.ip.addr, 4);
        ret = 1;
      }
      break;
  }
#else

  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    LOGE("getifaddrs failed: %s", strerror(errno));
    return -1;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr != NULL && strstr(ifa->ifa_name, IFR_NAME)) {
      if (ifa->ifa_addr->sa_family == addr->family) {
        switch (ifa->ifa_addr->sa_family) {
	  case AF_INET:
	    memcpy(addr->ipv4, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr, 4);
	    ret = 1;
	    break;
	  case AF_INET6:
	    memcpy(addr->ipv6, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.s6_addr, 16);
	    ret = 1;
	    break;
	  default:
	    break;
	}
	if (ret) {
          break;
        }
      }
    }
  }
  freeifaddrs(ifaddr);

  return ret;


#if 0
  struct ifaddrs *addrs,*tmp;

  struct ifreq ifr;

  int fd = socket(AF_INET6, SOCK_DGRAM, 0);

  if (fd < 0) {

    LOGE("get_host_address before socket init");
    return 0;
  }

  getifaddrs(&addrs);

  tmp = addrs;
LOGI("get_host_address before while loop");
  while (tmp) {
LOGI("get_host_address inside while loop");
    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {

      strncpy(ifr.ifr_name, tmp->ifa_name, IFNAMSIZ);

      if (strstr(ifr.ifr_name, IFR_NAME) && ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
LOGI("get_host_address inside while loop");
        for (int i = 0; i < 4; i++) {
	  addr->ipv4[i] = ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr >> (i * 8);
	}
#if 0
        LOGD("interface: %s, address: %s", ifr.ifr_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

        addr->family = AF_INET;
        memcpy(addr->ipv4, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr, 4);
#endif
	ret = 1;
        break;
      }
    }

    tmp = tmp->ifa_next;
  }

  freeifaddrs(addrs);
  close(fd);
#endif
#endif
  return ret;
}

int ports_resolve_addr(const char *host, Address *addr) {
  int ret = -1;
  struct addrinfo hints, *res, *p;
  int family;
  int status;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    //LOGE("getaddrinfo error: %s\n", gai_strerror(status));
    LOGE("getaddrinfo error: %d\n", status);
    return ret;
  }

  if (addr->family == AF_INET6) {
    family = AF_INET6;
  } else {
    family = AF_INET;
  }

 for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == family) {
      switch (p->ai_family) {
	case AF_INET6:
	  memcpy(addr->ipv6, &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr.s6_addr, 16);
	  break;
        case AF_INET:
	  LOGI("AF_INET");
	  memcpy(addr->ipv4, &((struct sockaddr_in *)p->ai_addr)->sin_addr.s_addr, 4);
	default:
	  break;
      }
      ret = 0;
    }
  }

  freeaddrinfo(res);
  return ret;
}

int ports_resolve_mdns_host(const char *host, Address *addr) {
#ifdef ESP32
  int ret = -1;
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

  addr->family = AF_INET;
  memcpy(addr->ipv4, &esp_addr.addr, 4);
  return 0;
#else
  return ports_resolve_addr(host, addr);
#endif
}


