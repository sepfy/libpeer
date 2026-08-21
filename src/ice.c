#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

static int ice_is_mdns_hostname(const char* hostname) {
  size_t length;
  int result = 0;

  if (hostname != NULL) {
    length = strlen(hostname);
    if (length > 0 && hostname[length - 1] == '.') {
      length--;
    }
    if (length > 6 && strncasecmp(hostname + length - 6, ".local", 6) == 0) {
      result = 1;
    }
  }

  return result;
}

int ice_candidate_from_description(IceCandidate* candidate, char* description,
                                   char* end) {
  char line[512];
  char* candidate_start = line;
  char type[16];
  char addrstring[ADDRSTRLEN];
  uint32_t port = 0;
  size_t line_length = 0;
  int parsed = 0;
  int result = -1;

  if (candidate != NULL && description != NULL) {
    if (end == NULL) {
      end = description + strlen(description);
    }
    if (end >= description) {
      line_length = (size_t)(end - description);
    }
  }

  if (line_length > 0 && line_length < sizeof(line)) {
    memcpy(line, description, line_length);
    line[line_length] = '\0';
    memset(candidate, 0, sizeof(*candidate));
    memset(type, 0, sizeof(type));
    memset(addrstring, 0, sizeof(addrstring));

    if (strncmp(candidate_start, "a=", 2) == 0) {
      candidate_start += 2;
    }
    if (strncmp(candidate_start, "candidate:", 10) == 0) {
      candidate_start += 10;
      parsed = sscanf(candidate_start,
                      "%32s %d %32s %" PRIu32 " %45s %" PRIu32
                      " typ %15s",
                      candidate->foundation,
                      &candidate->component,
                      candidate->transport,
                      &candidate->priority,
                      addrstring,
                      &port,
                      type);
    }
  }

  if (parsed != 7) {
    LOGE("Failed to parse ICE candidate description");
  } else if (strcasecmp(candidate->transport, "UDP") != 0) {
    LOGE("Only UDP transport is supported");
  } else if (candidate->component < 1 || candidate->component > 256) {
    LOGE("ICE candidate component is out of range");
  } else if (port > UINT16_MAX) {
    LOGE("ICE candidate port is out of range");
  } else {
    if (strcasecmp(type, "host") == 0) {
      candidate->type = ICE_CANDIDATE_TYPE_HOST;
    } else if (strcasecmp(type, "srflx") == 0) {
      candidate->type = ICE_CANDIDATE_TYPE_SRFLX;
    } else if (strcasecmp(type, "relay") == 0) {
      candidate->type = ICE_CANDIDATE_TYPE_RELAY;
    } else {
      LOGE("Unknown candidate type: %s", type);
      parsed = 0;
    }

    if (parsed == 7) {
      addr_set_family(&candidate->addr, AF_INET);
      addr_set_port(&candidate->addr, (uint16_t)port);
      if (candidate->type == ICE_CANDIDATE_TYPE_HOST &&
          ice_is_mdns_hostname(addrstring)) {
        if (mdns_resolve_addr(addrstring, &candidate->addr) == 0) {
          LOGW("Failed to resolve mDNS address; retaining host candidate");
        }
        result = 0;
      } else if (addr_from_string(addrstring, &candidate->addr) != 0) {
        addr_set_port(&candidate->addr, (uint16_t)port);
        result = 0;
      } else {
        LOGE("Failed to parse ICE candidate address");
      }
    }
  }

  return result;
}
