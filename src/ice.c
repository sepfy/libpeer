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
    case ICE_CANDIDATE_TYPE_SRFLX:
      return 100;
    case ICE_CANDIDATE_TYPE_PRFLX:
      return 110;  // RFC 8445: peer-reflexive between host and srflx
    case ICE_CANDIDATE_TYPE_RELAY:
      // Boosted from 0 to 50 to ensure relay candidates are tried earlier.
      // RFC 8445 recommends lower priority for relay, but libpeer's sequential
      // checking means relay pairs may never be reached if host/srflx all fail.
      // With priority=50, relay pairs interleave with srflx (100) checks.
      return 50;
    default:
      return 0;
  }
}

// RFC 8445 §7.1.1: PRIORITY attribute in binding request must use peer-reflexive
// type preference, not the actual candidate type preference.
uint32_t ice_candidate_compute_prflx_priority(IceCandidate* candidate) {
  // priority = (2^24)*(type preference) + (2^8)*(local preference) + (256 - component ID)
  // Use PRFLX type preference (110) instead of actual candidate type
  // Use maximum local_pref (65535) like aiortc - the prflx candidate doesn't exist yet
  // so we use maximum preference for this hypothetical candidate
  uint8_t prflx_type_pref = 110;
  uint16_t local_pref = 65535;  // aiortc uses max local preference
  return (1 << 24) * prflx_type_pref + (1 << 8) * local_pref + (256 - candidate->component);
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

  snprintf(candidate->transport, sizeof(candidate->transport), "%s", "udp");
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
      snprintf(typ_raddr, sizeof(typ_raddr), "prflx raddr %s rport %d", addr_string, candidate->raddr.port);
      break;
    case ICE_CANDIDATE_TYPE_RELAY:
      snprintf(typ_raddr, sizeof(typ_raddr), "relay raddr %s rport %d", addr_string, candidate->raddr.port);
      break;
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
  char addrstring[ADDRSTRLEN];

  if (strncmp("a=", candidate_start, strlen("a=")) == 0) {
    candidate_start += strlen("a=");
  }
  candidate_start += strlen("candidate:");

  // a=candidate:448736988 1 udp 2122260223 172.17.0.1 49250 typ host generation 0 network-id 1 network-cost 50
  // a=candidate:udpcandidate 1 udp 120 192.168.1.102 8000 typ host
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

  // IMPORTANT: udp_socket_sendto() uses addr->sin/sin6 port from the sockaddr.
  // So we must set IP/family first, then set the port via addr_set_port().
  memset(&candidate->addr, 0, sizeof(candidate->addr));
  memset(&candidate->raddr, 0, sizeof(candidate->raddr));

  if (strstr(addrstring, "local") != NULL) {
    if (mdns_resolve_addr(addrstring, &candidate->addr) == 0) {
      LOGW("Failed to resolve mDNS address");
      return -1;
    }
  } else {
    if (addr_from_string(addrstring, &candidate->addr) == 0) {
      return -1;
    }
  }

  addr_set_port(&candidate->addr, (uint16_t)port);

  // Parse optional base address (raddr/rport) for srflx/relay candidates.
  // Example:
  // a=candidate:... 1 udp ... 1.2.3.4 12345 typ srflx raddr 192.168.1.10 rport 56789
  char raddrstring[ADDRSTRLEN];
  uint32_t rport = 0;
  memset(raddrstring, 0, sizeof(raddrstring));
  if (strstr(candidate_start, " raddr ") && strstr(candidate_start, " rport ")) {
    if (sscanf(strstr(candidate_start, " raddr "), " raddr %s", raddrstring) == 1 &&
        sscanf(strstr(candidate_start, " rport "), " rport %" PRIu32, &rport) == 1) {
      if (addr_from_string(raddrstring, &candidate->raddr) != 0) {
        addr_set_port(&candidate->raddr, (uint16_t)rport);
      }
    }
  }

  return 0;
}
