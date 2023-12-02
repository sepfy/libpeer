#include <string.h>
#include "platform/socket.h"
#include "platform/address.h"

#ifdef ESP32
#include <mdns.h>
#include <esp_netif.h>
#endif

#include "ports.h"
#include "utils.h"

int ports_get_host_addr(Address *addr) {

  int ret = 0;

#ifdef _WIN32
  char buf[256];
  if(gethostname(buf, sizeof(buf)))
  {
    LOGE("could not get own hostname");
    return 0;
  }

  struct hostent* host = gethostbyname(buf);
  if(!host || !host->h_addr_list[0])
  {
    LOGE("could not get host info for self");
    return 0;
  }

  addr->family = host->h_addrtype;
  switch(host->h_addrtype)
  {
  case AF_INET:
    memcpy(addr->ipv4, host->h_addr_list[0], sizeof(addr->ipv4));
    break;
  case AF_INET6:
    memcpy(addr->ipv6, host->h_addr_list[0], sizeof(addr->ipv6));
    break;
  default:
    LOGE("unknown address type for own host info");
    return 0;
  }
  ret = 1;
#elif defined(ESP32)

  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

  esp_netif_ip_info_t ip_info;

  if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {

    memcpy(addr->ipv4, &ip_info.ip.addr, 4);
    ret = 1;
  }
#else
  struct ifaddrs *addrs,*tmp;

  struct ifreq ifr;

  int fd = socket(AF_INET, SOCK_DGRAM, 0);

  if (fd < 0) {

    LOGE("get_host_address before socket init");
    return 0;
  }

  getifaddrs(&addrs);

  tmp = addrs;

  while (tmp) {

    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET) {

      strncpy(ifr.ifr_name, tmp->ifa_name, IFNAMSIZ);

      if (strstr(ifr.ifr_name, IFR_NAME) && ioctl(fd, SIOCGIFADDR, &ifr) == 0) {

        LOGD("interface: %s, address: %s", ifr.ifr_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));

        addr->family = AF_INET;
        memcpy(addr->ipv4, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr, 4);
        ret = 1;
        break;
      }
    }

    tmp = tmp->ifa_next;
  }

  freeifaddrs(addrs);
  close(fd);
#endif
  return ret;
}

int ports_resolve_addr(const char *host, Address *addr) {

  int ret = -1;
  struct addrinfo hints, *res, *p;
  int status;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    LOGE("getaddrinfo error: `%s` => %s\n", host, gai_strerror(status));
    //LOGE("getaddrinfo error: %d\n", status);
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

  memcpy(addr->ipv4, &esp_addr.addr, 4);
  return ret;
#else
  return ports_resolve_addr(host, addr);
#endif
}
