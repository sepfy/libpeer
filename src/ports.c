#include <errno.h>
#include <string.h>

#ifndef WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "config.h"

#if CONFIG_USE_LWIP
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/sys.h"
#elif WIN32 
#include <winsock2.h>
// Windows implementation using GetAdaptersAddresses
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/ioctl.h>
#endif

#include "ports.h"
#include "utils.h"

int ports_get_host_addr(Address* addr, const char* iface_prefix) {
  int ret = 0;

#if CONFIG_USE_LWIP
  struct netif* netif;
  int i;
  for (netif = netif_list; netif != NULL; netif = netif->next) {
    switch (addr->family) {
      case AF_INET6:
        for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
          if (!ip6_addr_isany(netif_ip6_addr(netif, i))) {
            memcpy(&addr->sin6.sin6_addr, netif_ip6_addr(netif, i), 16);
            ret = 1;
            break;
          }
        }
        break;
      case AF_INET:
      default:
        if (!ip_addr_isany(&netif->ip_addr)) {
          memcpy(&addr->sin.sin_addr, &netif->ip_addr.u_addr.ip4, 4);
          ret = 1;
        }
        break;
    }

    if (ret) {
      break;
    }
  }
#elif WIN32
  ULONG out_buf_len = 15000;
  IP_ADAPTER_ADDRESSES* addresses = (IP_ADAPTER_ADDRESSES *)malloc(out_buf_len);
  if (addresses == NULL) {
    LOGE("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
    return -1;
  }

  DWORD dwRetVal = GetAdaptersAddresses(addr->family,
      GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
      NULL, addresses, &out_buf_len);

  if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
    free(addresses);
    addresses = (IP_ADAPTER_ADDRESSES *)malloc(out_buf_len);
    if (addresses == NULL) {
      LOGE("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
      return -1;
    }
    dwRetVal = GetAdaptersAddresses(addr->family,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        NULL, addresses, &out_buf_len);
  }

  if (dwRetVal != NO_ERROR) {
    LOGE("GetAdaptersAddresses() failed: %ld", dwRetVal);
    free(addresses);
    return ret;
  }

  for (IP_ADAPTER_ADDRESSES* addr_ptr = addresses; addr_ptr != NULL; addr_ptr = addr_ptr->Next) {
    if (iface_prefix && strlen(iface_prefix) > 0) {
      if (strncmp(addr_ptr->AdapterName, iface_prefix, strlen(iface_prefix)) != 0) {
        continue;
      }
    } else {
      if (addr_ptr->OperStatus != IfOperStatusUp) {
        continue;
      }
      if (addr_ptr->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
        continue;
      }
    }

    IP_ADAPTER_UNICAST_ADDRESS *unicast = addr_ptr->FirstUnicastAddress;
    for (; unicast != NULL; unicast = unicast->Next) {
      if (unicast->Address.lpSockaddr->sa_family != addr->family)
        continue;

      switch (addr->family) {
        case AF_INET6:
          memcpy(&addr->sin6, unicast->Address.lpSockaddr, sizeof(struct sockaddr_in6));
          break;
        case AF_INET:
        default:
          memcpy(&addr->sin, unicast->Address.lpSockaddr, sizeof(struct sockaddr_in));
          break;
      }
      ret = 1;
      break;
    }
    if (ret)
      break;
  }

  free(addresses);

#else

  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    LOGE("getifaddrs failed: %s", strerror(errno));
    return -1;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }

    if (ifa->ifa_addr->sa_family != addr->family) {
      continue;
    }

    if (iface_prefix && strlen(iface_prefix) > 0) {
      if (strncmp(ifa->ifa_name, iface_prefix, strlen(iface_prefix)) != 0) {
        continue;
      }

    } else {
      if ((ifa->ifa_flags & IFF_UP) == 0) {
        continue;
      }

      if ((ifa->ifa_flags & IFF_RUNNING) == 0) {
        continue;
      }

      if ((ifa->ifa_flags & IFF_LOOPBACK) == IFF_LOOPBACK) {
        continue;
      }
    }

    switch (ifa->ifa_addr->sa_family) {
      case AF_INET6:
        memcpy(&addr->sin6, ifa->ifa_addr, sizeof(struct sockaddr_in6));
        break;
      case AF_INET:
      default:
        memcpy(&addr->sin, ifa->ifa_addr, sizeof(struct sockaddr_in));
        break;
    }
    ret = 1;
    break;
  }
  freeifaddrs(ifaddr);
#endif
  return ret;
}

int ports_resolve_addr(const char* host, Address* addr) {
  char addr_string[ADDRSTRLEN];
  int ret = -1;
  struct addrinfo hints, *res, *p;
  int status;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
    LOGE("getaddrinfo error: %d\n", status);
    return ret;
  }

  // TODO: Support for IPv6
  addr_set_family(addr, AF_INET);
  for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == addr->family) {
      switch (addr->family) {
        case AF_INET6:
          memcpy(&addr->sin6, p->ai_addr, sizeof(struct sockaddr_in6));
          break;
        case AF_INET:
        default:
          memcpy(&addr->sin, p->ai_addr, sizeof(struct sockaddr_in));
          break;
      }
      ret = 0;
    }
  }

  addr_to_string(addr, addr_string, sizeof(addr_string));
  LOGI("Resolved %s -> %s", host, addr_string);
  freeaddrinfo(res);
  return ret;
}

uint32_t ports_get_epoch_time() {
  struct timeval tv;
#if WIN32
  FILETIME ft;
  ULARGE_INTEGER uli;
  GetSystemTimeAsFileTime(&ft);
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  // FILETIME is in 100-nanosecond intervals since Jan 1, 1601 (UTC)
  // Convert to milliseconds since Unix epoch (Jan 1, 1970)
  uint64_t epoch = (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
  tv.tv_sec = (long)(epoch / 1000);
  tv.tv_usec = (long)((epoch % 1000) * 1000);
#else
  gettimeofday(&tv, NULL);
#endif
  return (uint32_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void ports_sleep_ms(int ms) {
#if CONFIG_USE_LWIP
  sys_msleep(ms);
#elif WIN32
  Sleep(ms);
#else
  usleep(ms * 1000);
#endif
}
