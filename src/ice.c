#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ice.h"
#include "mdns.h"
#include "ports.h"
#include "socket.h"
#include "utils.h"

static uint8_t ice_candidate_type_preference(IceCandidateType type) {
  switch (type) {
    case ICE_CANDIDATE_TYPE_HOST:
      return 126;
    case ICE_CANDIDATE_TYPE_PRFLX:
      return 110;  /* RFC 5245: peer reflexive between HOST(126) and SRFLX(100) */
    case ICE_CANDIDATE_TYPE_SRFLX:
      return 100;
    case ICE_CANDIDATE_TYPE_RELAY:
      return 0;
    default:
      return 0;
  }
}

static uint16_t ice_candidate_local_preference(IceCandidate* candidate) {
  return candidate->addr.port;
}

static void ice_candidate_priority(IceCandidate* candidate) {
  // priority = (2^24)*(type preference) + (2^8)*(local preference) + (256 - component ID)
  candidate->priority = (1 << 24) * ice_candidate_type_preference(candidate->type) + (1 << 8) * ice_candidate_local_preference(candidate) + (256 - candidate->component);
}

void ice_candidate_create(IceCandidate* candidate, int foundation, IceCandidateType type, Address* addr) {
  memcpy(&candidate->addr, addr, sizeof(Address));
  candidate->type = type;

  snprintf(candidate->foundation, sizeof(candidate->foundation), "%d", foundation);
  // 1: RTP, 2: RTCP
  candidate->component = 1;

  ice_candidate_priority(candidate);

  snprintf(candidate->transport, sizeof(candidate->transport), "%s", "UDP");
}

void ice_candidate_to_description(IceCandidate* candidate, char* description, int length) {
  char addr_string[ADDRSTRLEN];
  char typ_raddr[128];

  memset(typ_raddr, 0, sizeof(typ_raddr));
  addr_to_string(&candidate->raddr, addr_string, sizeof(addr_string));

  switch (candidate->type) {
    case ICE_CANDIDATE_TYPE_HOST:
      snprintf(typ_raddr, sizeof(typ_raddr), "host");
      break;
    case ICE_CANDIDATE_TYPE_SRFLX:
      snprintf(typ_raddr, sizeof(typ_raddr), "srflx raddr %s rport %d", addr_string, candidate->raddr.port);
      break;
    case ICE_CANDIDATE_TYPE_PRFLX:
      snprintf(typ_raddr, sizeof(typ_raddr), "prflx");
      break;
    case ICE_CANDIDATE_TYPE_RELAY:
      snprintf(typ_raddr, sizeof(typ_raddr), "relay raddr %s rport %d", addr_string, candidate->raddr.port);
    default:
      break;
  }

  addr_to_string(&candidate->addr, addr_string, sizeof(addr_string));
  snprintf(description, length, "a=candidate:%s %d %s %" PRIu32 " %s %d typ %s\r\n",
           candidate->foundation,
           candidate->component,
           candidate->transport,
           candidate->priority,
           addr_string,
           candidate->addr.port,
           typ_raddr);
}

int ice_candidate_from_description(IceCandidate* candidate, char* description, char* end) {
  char* candidate_start = description;
  uint32_t port;
  char type[16];
  char addrstring[256]; // 局域网会使用 mDNS 主机名

  if (strncmp("a=", candidate_start, strlen("a=")) == 0) {
    candidate_start += strlen("a=");
  }
  candidate_start += strlen("candidate:");

// a=candidate:448736988 1 udp 2122260223 172.17.0.1 49250 typ host generation 0 network-id 1 network-cost 50
// a=candidate:3989800143 1 udp 2113937151 48c82aba-d349-4784-a733-404f193524f5.local 64630 typ host generation 0 network-cost 999
// a=candidate:udpcandidate 1 udp 120 192.168.1.102 8000 typ host
// a=candidate:1623718428 1 udp 2113937151 10.65.209.95 55476 typ host generation 0 network-cost 999
// a=candidate:69123048 1 udp 2113939711 240e:469:246:4066:6c3e:6cff:fefd:58dd 43249 typ host generation 0 network-cost 999
  if (sscanf(candidate_start, "%s %d %s %" PRIu32 " %s %" PRIu32 " typ %s",
             candidate->foundation,
             &candidate->component,
             candidate->transport,
             &candidate->priority,
             addrstring,
             &port,
             type) != 7) {
    LOGE("Failed to parse ICE candidate description");
    return -1;
  }

  if (strncmp(candidate->transport, "UDP", 3) != 0 && strncmp(candidate->transport, "udp", 3) != 0) {
    LOGE("Only UDP transport is supported");
    return -1;
  }

  if (strncmp(type, "host", 4) == 0) {
    candidate->type = ICE_CANDIDATE_TYPE_HOST;
  } else if (strncmp(type, "srflx", 5) == 0) {
    candidate->type = ICE_CANDIDATE_TYPE_SRFLX;
  } else if (strncmp(type, "relay", 5) == 0) {
    candidate->type = ICE_CANDIDATE_TYPE_RELAY;
  } else {
    LOGE("Unknown candidate type: %s", type);
    return -1;
  }

  addr_set_port(&candidate->addr, port);

  if (strstr(addrstring, ".local") != NULL) {
    /**
     * In a LAN environment, Chrome assigns a local domain name like uuid.local.
     * If the device is a Wi‑Fi AP and the PC is directly connected via Wi‑Fi,
     * while the PC's wired network interface is also connected to another network,
     * an mDNS query will return the IP of the wired interface.
     * In this case, mdns_resolve_addr() will have 3 retries × 5 receive attempts × 1s timeout
     * = up to 15 seconds, and it will definitely fail.
     */
#if CONFIG_USE_MDNS
    if (mdns_resolve_addr(addrstring, &candidate->addr) == 0)
#endif
    {
      LOGW("Failed to resolve mDNS address");
      return -1;
    }
  } else if (addr_from_string(addrstring, &candidate->addr) == 0) {
    return -1;
  }

  return 0;
}
