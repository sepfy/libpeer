#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

#include "ports.h"
#include "udp.h"
#include "utils.h"
#include "ice.h"

static uint8_t ice_candidate_type_preference(IceCandidateType type) {

  switch (type) {
    case ICE_CANDIDATE_TYPE_HOST:
      return 126;
    case ICE_CANDIDATE_TYPE_SRFLX:
      return 100;
    case ICE_CANDIDATE_TYPE_RELAY:
      return 0;
    default:
      return 0;
  }
}

static uint16_t ice_candidate_local_preference(IceCandidate *candidate) {

  return candidate->addr.port;
}

static void ice_candidate_priority(IceCandidate *candidate) {

  // priority = (2^24)*(type preference) + (2^8)*(local preference) + (256 - component ID)
  candidate->priority = (1 << 24) * ice_candidate_type_preference(candidate->type) + (1 << 8) * ice_candidate_local_preference(candidate) + (256 - candidate->component);
}

void ice_candidate_create(IceCandidate *candidate, int foundation, IceCandidateType type, Address *addr) {

  memcpy(&candidate->addr, addr, sizeof(Address));
  candidate->type = type;
  candidate->foundation = foundation;
  // 1: RTP, 2: RTCP
  candidate->component = 1;

  ice_candidate_priority(candidate);

  snprintf(candidate->transport, sizeof(candidate->transport), "%s", "UDP");
}

void ice_candidate_to_description(IceCandidate *candidate, char *description, int length) {

  char typ_raddr[64];

  memset(typ_raddr, 0, sizeof(typ_raddr));

  switch (candidate->type) {

    case ICE_CANDIDATE_TYPE_HOST:
      snprintf(typ_raddr, sizeof(typ_raddr), "host");
      break;

    case ICE_CANDIDATE_TYPE_SRFLX:
      snprintf(typ_raddr, sizeof(typ_raddr), "srflx raddr %d.%d.%d.%d rport %d",
       candidate->raddr.ipv4[0],
       candidate->raddr.ipv4[1],
       candidate->raddr.ipv4[2],
       candidate->raddr.ipv4[3],
       candidate->raddr.port);
      break;
    case ICE_CANDIDATE_TYPE_RELAY:
      snprintf(typ_raddr, sizeof(typ_raddr), "relay raddr %d.%d.%d.%d rport %d",
       candidate->raddr.ipv4[0],
       candidate->raddr.ipv4[1],
       candidate->raddr.ipv4[2],
       candidate->raddr.ipv4[3],
       candidate->raddr.port);
    default:
      break;
  }

  snprintf(description, length, "a=candidate:%d %d %s %" PRIu32 " %d.%d.%d.%d %d typ %s\r\n",
   candidate->foundation,
   candidate->component,
   candidate->transport,
   candidate->priority,
   candidate->addr.ipv4[0],
   candidate->addr.ipv4[1],
   candidate->addr.ipv4[2],
   candidate->addr.ipv4[3],
   candidate->addr.port,
   typ_raddr);
}

int ice_candidate_from_description(IceCandidate *candidate, char *description, char *end) {

  char *split_start = description + strlen("a=candidate:");
  char *split_end = NULL;
  int index = 0;
  char buf[64];
  // a=candidate:448736988 1 udp 2122260223 172.17.0.1 49250 typ host generation 0 network-id 1 network-cost 50
  // a=candidate:udpcandidate 1 udp 120 192.168.1.102 8000 typ host
  while ((split_end = strstr(split_start, " ")) != NULL && split_start < end) {

    memset(buf, 0, sizeof(buf));
    strncpy(buf, split_start, split_end - split_start);
    switch (index) {

      case 0:
        candidate->foundation = atoi(buf);
        break;
      case 1:
        candidate->component = atoi(buf);
        break;
      case 2:
        if (strstr(buf, "UDP") == 0 && strstr(buf, "udp") == 0) {
          // Only accept UDP candidates
          return -1;
        }
        strncpy(candidate->transport, buf, strlen(buf));
        break;
      case 3:
        candidate->priority = atoi(buf);
        break;
      case 4:
        if (strstr(buf, "local") != 0) {
          if (ports_resolve_mdns_host(buf, &candidate->addr) < 0) {
            return -1;
          }
          LOGD("mDNS host: %s, ip: %d.%d.%d.%d", buf, candidate->addr.ipv4[0], candidate->addr.ipv4[1], candidate->addr.ipv4[2], candidate->addr.ipv4[3]);
        } else if (!addr_ipv4_validate(buf, strlen(buf), &candidate->addr)) {
          LOGW("Unknow address");
          return -1;
        }
        break;
      case 5:
        candidate->addr.port = atoi(buf);
        break;
      case 7:

        if (strncmp(buf, "host", 4) == 0) {
          candidate->type = ICE_CANDIDATE_TYPE_HOST;
        } else if (strncmp(buf, "srflx", 5) == 0) {
          candidate->type = ICE_CANDIDATE_TYPE_SRFLX;
        } else if (strncmp(buf, "relay", 5) == 0) {
          candidate->type = ICE_CANDIDATE_TYPE_RELAY;
        } else {
          LOGE("Unknown candidate type: %s", buf);
          return -1;
        }
        // End of description
        return 0;
      default:
        break;
    }

    split_start = split_end + 1;
    index++;
  }

  return 0;
}
