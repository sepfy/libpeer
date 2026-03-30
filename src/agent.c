#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

#include "agent.h"
#include "base64.h"
#include "ice.h"
#include "ports.h"
#include "socket.h"
#include "stun.h"
#include "utils.h"

static int addr_is_private_ipv4(const Address* addr) {
  if (addr->family != AF_INET) return 0;
  uint32_t ip = ntohl(addr->sin.sin_addr.s_addr);
  if ((ip & 0xFF000000) == 0x0A000000) return 1;               // 10.0.0.0/8
  if ((ip & 0xFFF00000) == 0xAC100000) return 1;               // 172.16.0.0/12
  if ((ip & 0xFFFF0000) == 0xC0A80000) return 1;               // 192.168.0.0/16
  return 0;
}

// Return an ID for the private space: 10 => 1, 172.16/12 => 2, 192.168/16 => 3, else 0
static int addr_private_space_id(const Address* addr) {
  if (addr->family != AF_INET) return 0;
  uint32_t ip = ntohl(addr->sin.sin_addr.s_addr);
  if ((ip & 0xFF000000) == 0x0A000000) return 1;
  if ((ip & 0xFFF00000) == 0xAC100000) return 2;
  if ((ip & 0xFFFF0000) == 0xC0A80000) return 3;
  return 0;
}

#define AGENT_POLL_TIMEOUT 150  // 150ms poll timeout - increased for slow TURN relay responses
#define AGENT_CONNCHECK_MAX 1    // 1 check per pair - cycle through ALL candidates quickly
                                  // aiortc sends to all candidates within ~100ms before waiting
                                 // Give Pipecat enough time to process SDP and start sending
#define AGENT_CONNCHECK_PERIOD 1  // Log every check for debugging
// Max receive attempts for synchronous STUN/TURN transactions.
// Each attempt waits up to AGENT_POLL_TIMEOUT (150ms) in select().
// Keep this bounded to avoid watchdog resets when a TURN transaction never gets a response.
#define AGENT_TURN_RECV_MAX_ATTEMPTS 20  // ~2 seconds max blocking per transaction

static uint64_t htonll_u64(uint64_t v) {
  // Network byte order is big-endian.
  uint32_t hi = (uint32_t)(v >> 32);
  uint32_t lo = (uint32_t)(v & 0xFFFFFFFFu);
  return ((uint64_t)htonl(lo) << 32) | (uint64_t)htonl(hi);
}

// Forward declarations for STUN processing during TURN setup
void agent_process_stun_request(Agent* agent, StunMessage* stun_msg, Address* addr, ChannelBinding* via_channel);
static int agent_socket_recv(Agent* agent, Address* addr, uint8_t* buf, int len);
static int agent_socket_send(Agent* agent, Address* addr, const uint8_t* buf, int len);

void agent_channel_init(Agent* agent) {
  memset(agent->channel_bindings, 0, sizeof(agent->channel_bindings));
  agent->next_channel_number = CHANNEL_NUMBER_MIN;
}

ChannelBinding* agent_channel_find_by_peer(Agent* agent, const Address* peer_addr) {
  for (int i = 0; i < AGENT_MAX_CHANNEL_BINDINGS; i++) {
    if (agent->channel_bindings[i].channel_number == 0) continue;
    if (addr_cmp(&agent->channel_bindings[i].peer_addr, peer_addr) == 0) {
      return &agent->channel_bindings[i];
    }
  }
  return NULL;
}

ChannelBinding* agent_channel_find_by_channel(Agent* agent, uint16_t channel) {
  for (int i = 0; i < AGENT_MAX_CHANNEL_BINDINGS; i++) {
    if (agent->channel_bindings[i].channel_number == channel) {
      return &agent->channel_bindings[i];
    }
  }
  return NULL;
}

int agent_channel_allocate(Agent* agent, const Address* peer_addr, ChannelBinding** out_binding) {
  ChannelBinding* existing = agent_channel_find_by_peer(agent, peer_addr);
  if (existing) {
    *out_binding = existing;
    return 0;
  }

  // Find free slot
  int free_idx = -1;
  for (int i = 0; i < AGENT_MAX_CHANNEL_BINDINGS; i++) {
    if (agent->channel_bindings[i].channel_number == 0) {
      free_idx = i;
      break;
    }
  }
  if (free_idx < 0) {
    LOGE("TURN: No free channel binding slots");
    return -1;
  }

  uint16_t channel = agent->next_channel_number;
  if (channel < CHANNEL_NUMBER_MIN || channel > CHANNEL_NUMBER_MAX) {
    channel = CHANNEL_NUMBER_MIN;
  }
  agent->next_channel_number = channel + 1;
  if (agent->next_channel_number > CHANNEL_NUMBER_MAX) {
    agent->next_channel_number = CHANNEL_NUMBER_MIN;
  }

  agent->channel_bindings[free_idx].channel_number = channel;
  agent->channel_bindings[free_idx].bound = 0;
  memcpy(&agent->channel_bindings[free_idx].peer_addr, peer_addr, sizeof(Address));
  *out_binding = &agent->channel_bindings[free_idx];
  return 0;
}

int agent_channel_data_encode(uint16_t channel, const uint8_t* payload, size_t payload_len, uint8_t* out, size_t out_size) {
  if (channel < CHANNEL_NUMBER_MIN || channel > CHANNEL_NUMBER_MAX) {
    return -1;
  }
  if (!payload || !out) {
    return -1;
  }
  if (payload_len > 0xFFFF || out_size < payload_len + 4) {
    return -1;
  }
  uint16_t net_chan = htons(channel);
  uint16_t net_len = htons((uint16_t)payload_len);
  memcpy(out, &net_chan, sizeof(net_chan));
  memcpy(out + 2, &net_len, sizeof(net_len));
  memcpy(out + 4, payload, payload_len);
  return (int)(payload_len + 4);
}

int agent_channel_data_decode(const uint8_t* data, size_t data_len, uint16_t* channel, const uint8_t** payload, size_t* payload_len) {
  if (!data || data_len < 4 || !channel || !payload || !payload_len) {
    return -1;
  }
  uint16_t chan = ntohs(*(uint16_t*)data);
  uint16_t len = ntohs(*(uint16_t*)(data + 2));
  if (chan < CHANNEL_NUMBER_MIN || chan > CHANNEL_NUMBER_MAX) {
    return -1;
  }
  if (data_len < 4 + len) {
    return -1;
  }
  *channel = chan;
  *payload = data + 4;
  *payload_len = len;
  return 0;
}

void agent_clear_candidates(Agent* agent) {
  agent->local_candidates_count = 0;
  agent->remote_candidates_count = 0;
  agent->candidate_pairs_num = 0;
}

int agent_create(Agent* agent) {
  int ret;
  // Ensure unused sockets are invalid so select/FD_ISSET never touches garbage fds.
  agent->udp_sockets[0].fd = -1;
  agent->udp_sockets[1].fd = -1;
  agent->ice_socket.fd = -1;
  // Do not nominate immediately; only nominate (USE-CANDIDATE) after a successful check.
  agent->use_candidate = 0;
  // Default: use RFC 8445 priority order. Set prefer_relay=1 for cloud services.
  agent->prefer_relay = 0;

  if ((ret = udp_socket_open(&agent->udp_sockets[0], AF_INET, 0)) < 0) {
    LOGE("Failed to create UDP socket.");
    return ret;
  }
  LOGD("UDP socket: %d", agent->udp_sockets[0].fd);

#if CONFIG_IPV6
  if ((ret = udp_socket_open(&agent->udp_sockets[1], AF_INET6, 0)) < 0) {
    LOGE("Failed to create IPv6 UDP socket.");
    return ret;
  }
  LOGD("UDP6 socket: %d", agent->udp_sockets[1].fd);
#endif

  // Create separate socket for direct ICE connectivity checks.
  // Using a different socket than TURN avoids NAT/firewall issues where
  // Cloudflare may reject non-TURN traffic on the TURN-bound socket.
  if ((ret = udp_socket_open(&agent->ice_socket, AF_INET, 0)) < 0) {
    LOGE("Failed to create ICE socket.");
    return ret;
  }
  LOGI("ICE socket: %d (separate from TURN socket %d)", agent->ice_socket.fd, agent->udp_sockets[0].fd);

  agent_clear_candidates(agent);
  agent_channel_init(agent);
  memset(agent->remote_ufrag, 0, sizeof(agent->remote_ufrag));
  memset(agent->remote_upwd, 0, sizeof(agent->remote_upwd));
  return 0;
}

void agent_destroy(Agent* agent) {
  if (agent->udp_sockets[0].fd > 0) {
    udp_socket_close(&agent->udp_sockets[0]);
  }

#if CONFIG_IPV6
  if (agent->udp_sockets[1].fd > 0) {
    udp_socket_close(&agent->udp_sockets[1]);
  }
#endif

  if (agent->ice_socket.fd > 0) {
    udp_socket_close(&agent->ice_socket);
  }
}

static int agent_socket_recv(Agent* agent, Address* addr, uint8_t* buf, int len) {
  int ret = -1;
  int i = 0;
  int maxfd = -1;
  fd_set rfds;
  struct timeval tv;
  int addr_type[] = { AF_INET,
#if CONFIG_IPV6
                      AF_INET6,
#endif
  };
  const int socket_count = (int)(sizeof(addr_type) / sizeof(addr_type[0]));

  tv.tv_sec = 0;
  tv.tv_usec = AGENT_POLL_TIMEOUT * 1000;
  FD_ZERO(&rfds);

  for (i = 0; i < socket_count; i++) {
    if (agent->udp_sockets[i].fd > maxfd) {
      maxfd = agent->udp_sockets[i].fd;
    }
    if (agent->udp_sockets[i].fd >= 0) {
      FD_SET(agent->udp_sockets[i].fd, &rfds);
    }
  }

  // Also poll the separate ICE socket
  if (agent->ice_socket.fd >= 0) {
    if (agent->ice_socket.fd > maxfd) {
      maxfd = agent->ice_socket.fd;
    }
    FD_SET(agent->ice_socket.fd, &rfds);
  }

  LOGD("socket_recv: maxfd=%d timeout=%dms", maxfd, AGENT_POLL_TIMEOUT);

  ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);

  if (ret < 0) {
    LOGE("select error: %d", errno);
  } else if (ret == 0) {
    // Timeout - no data available
    LOGD("socket_recv: select timeout (no data)");
  } else {
    LOGD("socket_recv: select ready, ret=%d", ret);
    // Check ICE socket first (more likely to have ICE responses)
    if (agent->ice_socket.fd >= 0 && FD_ISSET(agent->ice_socket.fd, &rfds)) {
      memset(buf, 0, len);
      ret = udp_socket_recvfrom(&agent->ice_socket, addr, buf, len);
      LOGD("socket_recv: ice_fd=%d ret=%d", agent->ice_socket.fd, ret);
    } else {
      for (i = 0; i < socket_count; i++) {
        if (agent->udp_sockets[i].fd >= 0 && FD_ISSET(agent->udp_sockets[i].fd, &rfds)) {
          memset(buf, 0, len);
          ret = udp_socket_recvfrom(&agent->udp_sockets[i], addr, buf, len);
          LOGD("socket_recv: fd=%d ret=%d", agent->udp_sockets[i].fd, ret);
          break;
        }
      }
    }
  }

  return ret;
}

static int agent_socket_recv_attempts(Agent* agent, Address* addr, uint8_t* buf, int len, int maxtimes) {
  int ret = -1;
  int i = 0;
  for (i = 0; i < maxtimes; i++) {
    if ((ret = agent_socket_recv(agent, addr, buf, len)) != 0) {
      break;
    }
  }
  return ret;
}

// Receive data while processing STUN binding requests from peers.
// This is critical during TURN setup: Pipecat may start sending binding requests
// before we finish CreatePermission/ChannelBind. If we don't respond, Pipecat
// times out and never establishes the connection.
// Returns: >0 for non-binding-request data (TURN responses etc), 0 for timeout, <0 for error
static int agent_recv_turn_response(Agent* agent, uint8_t* buf, int len, int maxtimes) {
  int ret = -1;
  Address addr;
  StunMessage stun_msg;

  // Check if we have ICE credentials to validate binding requests
  int can_process_binding = (agent->local_upwd[0] != '\0' && agent->remote_ufrag[0] != '\0');

  for (int i = 0; i < maxtimes; i++) {
    memset(&addr, 0, sizeof(addr));
    ret = agent_socket_recv(agent, &addr, buf, len);

    if (ret <= 0) {
      // Timeout or error, continue polling
      continue;
    }

    // Check if this is a STUN binding request from a peer (only if we can process it)
    if (can_process_binding && stun_probe(buf, ret) == 0) {
      memcpy(stun_msg.buf, buf, ret);
      stun_msg.size = ret;
      stun_parse_msg_buf(&stun_msg);

      if (stun_msg.stunclass == STUN_CLASS_REQUEST && stun_msg.stunmethod == STUN_METHOD_BINDING) {
        // This is a binding request from Pipecat - process it immediately!
        char addr_str[64];
        addr_to_string(&addr, addr_str, sizeof(addr_str));
        LOGI("TURN recv: Got STUN binding request from %s during TURN setup - responding inline", addr_str);
        agent_process_stun_request(agent, &stun_msg, &addr, NULL);
        // Continue waiting for our TURN response
        continue;
      }
    }

    // Not a binding request - return this data to caller (likely TURN response)
    return ret;
  }

  return ret;  // Timeout
}

static int agent_socket_send(Agent* agent, Address* addr, const uint8_t* buf, int len) {
  switch (addr->family) {
    case AF_INET6:
      return udp_socket_sendto(&agent->udp_sockets[1], addr, buf, len);
    case AF_INET:
    default:
      return udp_socket_sendto(&agent->udp_sockets[0], addr, buf, len);
  }
  return -1;
}

// Send via the separate ICE socket for direct connectivity checks.
// This avoids NAT/firewall issues with the TURN-bound socket.
static int agent_ice_socket_send(Agent* agent, Address* addr, const uint8_t* buf, int len) {
  if (agent->ice_socket.fd < 0) {
    // Fallback to main socket if ICE socket not available
    return agent_socket_send(agent, addr, buf, len);
  }
  return udp_socket_sendto(&agent->ice_socket, addr, buf, len);
}

static int agent_create_host_addr(Agent* agent) {
  int i, j;
  const char* iface_prefx[] = {CONFIG_IFACE_PREFIX};
  IceCandidate* ice_candidate;
  int addr_type[] = { AF_INET,
#if CONFIG_IPV6
                      AF_INET6,
#endif
  };

  for (i = 0; i < sizeof(addr_type) / sizeof(addr_type[0]); i++) {
    for (j = 0; j < sizeof(iface_prefx) / sizeof(iface_prefx[0]); j++) {
      ice_candidate = agent->local_candidates + agent->local_candidates_count;
      // CRITICAL: Use ICE socket for host candidate so all ICE traffic uses same port.
      // Previously used udp_sockets[i] (TURN socket), causing port mismatch where we
      // advertised one port but sent binding requests from another.
      Address* base_addr = (i == 0 && agent->ice_socket.fd >= 0)
                           ? &agent->ice_socket.bind_addr
                           : &agent->udp_sockets[i].bind_addr;
      ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_HOST,
                           base_addr);
      // if resolve host addr, add to local candidate
      if (ports_get_host_addr(&ice_candidate->addr, iface_prefx[j])) {
        // For host candidates the base (raddr/rport) is itself
        memcpy(&ice_candidate->raddr, &ice_candidate->addr, sizeof(Address));
        agent->local_candidates_count++;
      }
    }
  }

  return 0;
}

static int agent_create_stun_addr(Agent* agent, Address* serv_addr) {
  int ret = -1;
  Address bind_addr;
  StunMessage send_msg;
  StunMessage recv_msg;
  memset(&send_msg, 0, sizeof(send_msg));
  memset(&recv_msg, 0, sizeof(recv_msg));

  stun_msg_create(&send_msg, STUN_CLASS_REQUEST | STUN_METHOD_BINDING);

  // CRITICAL: Send STUN binding request FROM the ICE socket, not the TURN socket.
  // This gets the reflexive address of the ICE socket, which we then advertise.
  // When we later send ICE binding requests from the ICE socket, they match
  // the address Pipecat has created TURN permissions for.
  ret = agent_ice_socket_send(agent, serv_addr, send_msg.buf, send_msg.size);

  if (ret == -1) {
    LOGE("Failed to send STUN Binding Request.");
    return ret;
  }

  ret = agent_socket_recv_attempts(agent, NULL, recv_msg.buf, sizeof(recv_msg.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
  if (ret <= 0) {
    LOGD("Failed to receive STUN Binding Response.");
    return ret;
  }

  stun_parse_msg_buf(&recv_msg);
  memcpy(&bind_addr, &recv_msg.mapped_addr, sizeof(Address));
  IceCandidate* ice_candidate = agent->local_candidates + agent->local_candidates_count++;
  ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_SRFLX, &bind_addr);
  // Set base address (raddr/rport) to the first host candidate if available
  if (agent->local_candidates_count > 1) {  // host candidate already added
    memcpy(&ice_candidate->raddr, &agent->local_candidates[0].addr, sizeof(Address));
  } else {
    memcpy(&ice_candidate->raddr, &agent->udp_sockets[0].bind_addr, sizeof(Address));
  }
  return ret;
}

static int agent_create_turn_addr(Agent* agent, Address* serv_addr, const char* username, const char* credential) {
  int ret = -1;
  uint32_t attr = ntohl(0x11000000);
  Address turn_addr;
  StunMessage send_msg;
  StunMessage recv_msg;
  // Store TURN server addr and creds for later CreatePermission
  memcpy(&agent->turn_server_addr, serv_addr, sizeof(Address));
  memset(agent->turn_username, 0, sizeof(agent->turn_username));
  memset(agent->turn_credential, 0, sizeof(agent->turn_credential));
  agent->turn_username_len = strlen(username);
  agent->turn_credential_len = strlen(credential);
  if (agent->turn_username_len >= sizeof(agent->turn_username)) {
    agent->turn_username_len = sizeof(agent->turn_username) - 1;
  }
  if (agent->turn_credential_len >= sizeof(agent->turn_credential)) {
    agent->turn_credential_len = sizeof(agent->turn_credential) - 1;
  }
  memcpy(agent->turn_username, username, agent->turn_username_len);
  memcpy(agent->turn_credential, credential, agent->turn_credential_len);
  agent->has_turn_allocation = 0;
  agent->turn_nonce_len = 0;
  agent->turn_realm_len = 0;
  memset(&recv_msg, 0, sizeof(recv_msg));
  memset(&send_msg, 0, sizeof(send_msg));
  stun_msg_create(&send_msg, STUN_METHOD_ALLOCATE);
  stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REQUESTED_TRANSPORT, sizeof(attr), (char*)&attr);  // UDP
  stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_USERNAME, strlen(username), (char*)username);

  ret = agent_socket_send(agent, serv_addr, send_msg.buf, send_msg.size);
  if (ret == -1) {
    LOGE("Failed to send TURN Binding Request.");
    return -1;
  }

  ret = agent_recv_turn_response(agent, recv_msg.buf, sizeof(recv_msg.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
  if (ret <= 0) {
    LOGD("Failed to receive STUN Binding Response.");
    return ret;
  }

  stun_parse_msg_buf(&recv_msg);

  if (recv_msg.stunclass == STUN_CLASS_ERROR && recv_msg.stunmethod == STUN_METHOD_ALLOCATE) {
    // Log the 401 challenge parameters - USE STORED LENGTHS, NOT strlen()!
    LOGD("TURN 401 challenge, retrying with auth");
    // Persist nonce/realm for CreatePermission later
    agent->turn_nonce_len = recv_msg.nonce_len < sizeof(agent->turn_nonce) ? recv_msg.nonce_len : sizeof(agent->turn_nonce) - 1;
    agent->turn_realm_len = recv_msg.realm_len < sizeof(agent->turn_realm) ? recv_msg.realm_len : sizeof(agent->turn_realm) - 1;
    memcpy(agent->turn_nonce, recv_msg.nonce, agent->turn_nonce_len);
    agent->turn_nonce[agent->turn_nonce_len] = '\0';
    memcpy(agent->turn_realm, recv_msg.realm, agent->turn_realm_len);
    agent->turn_realm[agent->turn_realm_len] = '\0';
    
    // Sanity check: if nonce_len is 0, something went wrong in parsing
    if (recv_msg.nonce_len == 0) {
      LOGE("TURN 401 response missing nonce! Cannot authenticate.");
      return -1;
    }
    
    memset(&send_msg, 0, sizeof(send_msg));
    stun_msg_create(&send_msg, STUN_METHOD_ALLOCATE);
    stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REQUESTED_TRANSPORT, sizeof(attr), (char*)&attr);  // UDP
    stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_USERNAME, strlen(username), (char*)username);
    // CRITICAL: Use stored nonce_len and realm_len, NOT strlen()!
    stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_NONCE, recv_msg.nonce_len, recv_msg.nonce);
    stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REALM, recv_msg.realm_len, recv_msg.realm);
    stun_msg_finish(&send_msg, STUN_CREDENTIAL_LONG_TERM, credential, strlen(credential));
    LOGD("Retrying TURN Allocate with MESSAGE-INTEGRITY");
  } else {
    LOGE("Invalid TURN Binding Response.");
    return -1;
  }

  ret = agent_socket_send(agent, serv_addr, send_msg.buf, send_msg.size);
  if (ret < 0) {
    LOGE("Failed to send TURN Allocate with MESSAGE-INTEGRITY.");
    return -1;
  }
  LOGD("TURN Allocate retry sent");

  // BUGFIX: Actually capture the return value from recv!
  ret = agent_recv_turn_response(agent, recv_msg.buf, sizeof(recv_msg.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
  if (ret <= 0) {
    LOGE("Failed to receive TURN Allocate response (ret=%d)", ret);
    return -1;
  }
  LOGD("TURN Allocate response received");

  stun_parse_msg_buf(&recv_msg);
  
  // Check if we got ANOTHER 401 (MI was rejected) vs success
  if (recv_msg.stunclass == STUN_CLASS_ERROR) {
    LOGE("TURN Allocate with MESSAGE-INTEGRITY was rejected! Check credentials/nonce.");
    return -1;
  }
  
  if (recv_msg.relayed_addr.port == 0) {
    LOGE("TURN Allocate succeeded but no relay address in response!");
    return -1;
  }
  
  memcpy(&turn_addr, &recv_msg.relayed_addr, sizeof(Address));
  {
    char relay_addr_str[64];
    addr_to_string(&turn_addr, relay_addr_str, sizeof(relay_addr_str));
    LOGI("TURN relay address obtained: %s:%u", relay_addr_str, (unsigned)turn_addr.port);
  }
  IceCandidate* ice_candidate = agent->local_candidates + agent->local_candidates_count++;
  ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_RELAY, &turn_addr);
  // Base address for relay should point to the local host candidate
  if (agent->local_candidates_count > 0) {
    memcpy(&ice_candidate->raddr, &agent->local_candidates[0].addr, sizeof(Address));
  } else {
    memcpy(&ice_candidate->raddr, &agent->udp_sockets[0].bind_addr, sizeof(Address));
  }
  agent->has_turn_allocation = 1;
  return ret;
}

int agent_build_channel_bind_request(Agent* agent, ChannelBinding* binding, StunMessage* out_msg) {
  if (!agent || !binding || !out_msg) return -1;
  if (!agent->has_turn_allocation || agent->turn_nonce_len == 0 || agent->turn_realm_len == 0) {
    LOGW("TURN ChannelBind build skipped (no allocation or missing nonce/realm)");
    return -1;
  }
  uint8_t peer_attr[32] = {0};
  uint8_t mask[16] = {0};
  uint8_t channel_attr[4] = {0};
  int peer_len = 0;

  memset(out_msg, 0, sizeof(StunMessage));
  stun_msg_create(out_msg, STUN_CLASS_REQUEST | STUN_METHOD_CHANNEL_BIND);
  channel_attr[0] = (binding->channel_number >> 8) & 0xFF;
  channel_attr[1] = binding->channel_number & 0xFF;
  stun_msg_write_attr(out_msg, STUN_ATTR_TYPE_CHANNEL_NUMBER, sizeof(channel_attr), (char*)channel_attr);

  *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
  StunHeader* header = (StunHeader*)out_msg->buf;
  memcpy(mask + 4, header->transaction_id, sizeof(header->transaction_id));
  peer_len = stun_set_mapped_address((char*)peer_attr, mask, &binding->peer_addr);
  stun_msg_write_attr(out_msg, STUN_ATTR_TYPE_XOR_PEER_ADDRESS, peer_len, (char*)peer_attr);
  stun_msg_write_attr(out_msg, STUN_ATTR_TYPE_USERNAME, agent->turn_username_len, agent->turn_username);
  stun_msg_write_attr(out_msg, STUN_ATTR_TYPE_NONCE, agent->turn_nonce_len, agent->turn_nonce);
  stun_msg_write_attr(out_msg, STUN_ATTR_TYPE_REALM, agent->turn_realm_len, agent->turn_realm);
  stun_msg_finish(out_msg, STUN_CREDENTIAL_LONG_TERM, agent->turn_credential, agent->turn_credential_len);
  return 0;
}

static int agent_turn_channel_bind(Agent* agent, ChannelBinding* binding) {
  if (!agent || !binding) return -1;
  StunMessage send_msg;
  StunMessage recv_msg;
  // Some TURN servers are picky and require an explicit CreatePermission even when using ChannelBind.
  // RFC 5766 says ChannelBind creates/refreshes the corresponding permission, but doing an explicit
  // CreatePermission here improves interoperability.
  if (agent->has_turn_allocation && agent->turn_nonce_len > 0 && agent->turn_realm_len > 0) {
    StunMessage perm_msg;
    StunMessage perm_resp;
    uint8_t peer_attr[32] = {0};
    uint8_t mask[16] = {0};
    int peer_len = 0;

    memset(&perm_msg, 0, sizeof(perm_msg));
    stun_msg_create(&perm_msg, STUN_CLASS_REQUEST | STUN_METHOD_CREATE_PERMISSION);

    *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
    StunHeader* perm_header = (StunHeader*)perm_msg.buf;
    memcpy(mask + 4, perm_header->transaction_id, sizeof(perm_header->transaction_id));
    peer_len = stun_set_mapped_address((char*)peer_attr, mask, &binding->peer_addr);
    stun_msg_write_attr(&perm_msg, STUN_ATTR_TYPE_XOR_PEER_ADDRESS, peer_len, (char*)peer_attr);
    stun_msg_write_attr(&perm_msg, STUN_ATTR_TYPE_USERNAME, agent->turn_username_len, agent->turn_username);
    stun_msg_write_attr(&perm_msg, STUN_ATTR_TYPE_NONCE, agent->turn_nonce_len, agent->turn_nonce);
    stun_msg_write_attr(&perm_msg, STUN_ATTR_TYPE_REALM, agent->turn_realm_len, agent->turn_realm);
    stun_msg_finish(&perm_msg, STUN_CREDENTIAL_LONG_TERM, agent->turn_credential, agent->turn_credential_len);

    char perm_peer_str[ADDRSTRLEN];
    addr_to_string(&binding->peer_addr, perm_peer_str, sizeof(perm_peer_str));
    LOGI("TURN: Sending CreatePermission peer=%s:%d", perm_peer_str, binding->peer_addr.port);

    int perm_sent = agent_socket_send(agent, &agent->turn_server_addr, perm_msg.buf, perm_msg.size);
    if (perm_sent < 0) {
      LOGE("TURN: Failed to send CreatePermission");
      return -1;
    }
    memset(&perm_resp, 0, sizeof(perm_resp));
    int perm_ret = agent_recv_turn_response(agent, perm_resp.buf, sizeof(perm_resp.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
    if (perm_ret <= 0) {
      // Best-effort: proceed to ChannelBind anyway (many servers accept ChannelBind without this).
      LOGW("TURN: No CreatePermission response (ret=%d) — proceeding to ChannelBind anyway", perm_ret);
    } else {
      stun_parse_msg_buf(&perm_resp);
      LOGI("TURN: CreatePermission response class=0x%02x method=0x%04x", perm_resp.stunclass, perm_resp.stunmethod);
      if (perm_resp.stunclass == STUN_CLASS_ERROR) {
        LOGW("TURN: CreatePermission rejected — proceeding to ChannelBind anyway");
      }
    }
  }

  int ret = agent_build_channel_bind_request(agent, binding, &send_msg);
  if (ret < 0) {
    return ret;
  }

  char peer_str[ADDRSTRLEN];
  addr_to_string(&binding->peer_addr, peer_str, sizeof(peer_str));
  LOGI("TURN: Sending ChannelBind ch=0x%04x peer=%s:%d", binding->channel_number, peer_str, binding->peer_addr.port);

  ret = agent_socket_send(agent, &agent->turn_server_addr, send_msg.buf, send_msg.size);
  if (ret < 0) {
    LOGE("TURN: Failed to send ChannelBind");
    return -1;
  }

  memset(&recv_msg, 0, sizeof(recv_msg));
  ret = agent_recv_turn_response(agent, recv_msg.buf, sizeof(recv_msg.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
  if (ret <= 0) {
    LOGE("TURN: No ChannelBind response (ret=%d)", ret);
    return -1;
  }
  stun_parse_msg_buf(&recv_msg);
  LOGI("TURN: ChannelBind response class=0x%02x method=0x%04x", recv_msg.stunclass, recv_msg.stunmethod);

  if (recv_msg.stunclass == STUN_CLASS_ERROR) {
    // Retry once if nonce/realm provided (401/438)
    if (recv_msg.nonce_len > 0 && recv_msg.realm_len > 0) {
      agent->turn_nonce_len = recv_msg.nonce_len < sizeof(agent->turn_nonce) ? recv_msg.nonce_len : sizeof(agent->turn_nonce) - 1;
      agent->turn_realm_len = recv_msg.realm_len < sizeof(agent->turn_realm) ? recv_msg.realm_len : sizeof(agent->turn_realm) - 1;
      memcpy(agent->turn_nonce, recv_msg.nonce, agent->turn_nonce_len);
      agent->turn_nonce[agent->turn_nonce_len] = '\0';
      memcpy(agent->turn_realm, recv_msg.realm, agent->turn_realm_len);
      agent->turn_realm[agent->turn_realm_len] = '\0';

      memset(&send_msg, 0, sizeof(send_msg));
      ret = agent_build_channel_bind_request(agent, binding, &send_msg);
      if (ret < 0) {
        return ret;
      }
      ret = agent_socket_send(agent, &agent->turn_server_addr, send_msg.buf, send_msg.size);
      if (ret < 0) {
        LOGE("TURN: Failed to send ChannelBind retry");
        return -1;
      }
      ret = agent_recv_turn_response(agent, recv_msg.buf, sizeof(recv_msg.buf), AGENT_TURN_RECV_MAX_ATTEMPTS);
      if (ret <= 0) {
        LOGE("TURN: No ChannelBind response after retry");
        return -1;
      }
      stun_parse_msg_buf(&recv_msg);
      if (recv_msg.stunclass == STUN_CLASS_ERROR) {
        LOGE("TURN: ChannelBind retry still error");
        return -1;
      }
    } else {
      LOGE("TURN: ChannelBind error without nonce/realm");
      return -1;
    }
  }

  binding->bound = 1;
  return 0;
}

void agent_gather_candidate(Agent* agent, const char* urls, const char* username, const char* credential) {
  char* pos;
  int port;
  char hostname[64];
  char addr_string[ADDRSTRLEN];
  int i;
  int addr_type[1] = {AF_INET};  // ipv6 no need stun
  Address resolved_addr;
  memset(hostname, 0, sizeof(hostname));

  if (urls == NULL) {
    agent_create_host_addr(agent);
    return;
  }

  if ((pos = strstr(urls + 5, ":")) == NULL) {
    LOGE("Invalid URL");
    return;
  }

  port = atoi(pos + 1);
  if (port <= 0) {
    LOGE("Cannot parse port");
    return;
  }

  snprintf(hostname, pos - urls - 5 + 1, "%s", urls + 5);

  for (i = 0; i < sizeof(addr_type) / sizeof(addr_type[0]); i++) {
    if (ports_resolve_addr(hostname, &resolved_addr) == 0) {
      addr_set_port(&resolved_addr, port);
      addr_to_string(&resolved_addr, addr_string, sizeof(addr_string));
      LOGD("Resolved %s:%d", addr_string, port);

      if (strncmp(urls, "stun:", 5) == 0) {
        LOGD("Create stun addr");
        agent_create_stun_addr(agent, &resolved_addr);
      } else if (strncmp(urls, "turn:", 5) == 0) {
        LOGD("Create turn addr");
        agent_create_turn_addr(agent, &resolved_addr, username, credential);
      }
    }
  }
}

void agent_create_ice_credential(Agent* agent) {
  memset(agent->local_ufrag, 0, sizeof(agent->local_ufrag));
  memset(agent->local_upwd, 0, sizeof(agent->local_upwd));

  utils_random_string(agent->local_ufrag, 4);
  utils_random_string(agent->local_upwd, 24);
}

void agent_get_local_description(Agent* agent, char* description, int length) {
  for (int i = 0; i < agent->local_candidates_count; i++) {
    ice_candidate_to_description(&agent->local_candidates[i], description + strlen(description), length - strlen(description));
  }

  // remove last \n
  description[strlen(description)] = '\0';
  LOGD("local description:\n%s", description);
}

int agent_send(Agent* agent, const uint8_t* buf, int len) {
  if (!agent || !buf || len <= 0) return -1;
  if (agent->nominated_pair && agent->nominated_pair->local->type == ICE_CANDIDATE_TYPE_RELAY) {
    ChannelBinding* binding = NULL;
    uint8_t channel_buf[1400 + 4];
    if (agent_channel_allocate(agent, &agent->nominated_pair->remote->addr, &binding) != 0) {
      return -1;
    }
    if (!binding->bound) {
      if (agent_turn_channel_bind(agent, binding) < 0) {
        return -1;
      }
    }
    int encoded = agent_channel_data_encode(binding->channel_number, buf, len, channel_buf, sizeof(channel_buf));
    if (encoded < 0) {
      return -1;
    }
    return agent_socket_send(agent, &agent->turn_server_addr, channel_buf, encoded);
  }
  // For non-relay (host/srflx), use the ICE socket that was used for ICE negotiation.
  // This ensures DTLS packets come from the same source address/port as ICE binding requests.
  return agent_ice_socket_send(agent, &agent->nominated_pair->remote->addr, buf, len);
}

static void agent_create_binding_response(Agent* agent, StunMessage* msg, Address* addr) {
  int size = 0;
  char username[584];
  char mapped_address[32];
  uint8_t mask[16];
  StunHeader* header;
  stun_msg_create(msg, STUN_CLASS_RESPONSE | STUN_METHOD_BINDING);
  header = (StunHeader*)msg->buf;
  memcpy(header->transaction_id, agent->transaction_id, sizeof(header->transaction_id));
  snprintf(username, sizeof(username), "%s:%s", agent->local_ufrag, agent->remote_ufrag);
  *((uint32_t*)mask) = htonl(MAGIC_COOKIE);
  memcpy(mask + 4, agent->transaction_id, sizeof(agent->transaction_id));
  size = stun_set_mapped_address(mapped_address, mask, addr);
  stun_msg_write_attr(msg, STUN_ATTR_TYPE_XOR_MAPPED_ADDRESS, size, mapped_address);
  stun_msg_write_attr(msg, STUN_ATTR_TYPE_USERNAME, strlen(username), username);
  stun_msg_finish(msg, STUN_CREDENTIAL_SHORT_TERM, agent->local_upwd, strlen(agent->local_upwd));
}

void agent_create_binding_request(Agent* agent, StunMessage* msg, int is_heartbeat) {
  // RFC 8445 §6.1.1.2: tie-breaker MUST be a random number
  static uint64_t tie_breaker = 0;
  if (tie_breaker == 0) {
    // Generate a simple random tie-breaker using available entropy
    uint32_t r1 = (uint32_t)rand();
    uint32_t r2 = (uint32_t)rand();
    tie_breaker = ((uint64_t)r1 << 32) | r2;
  }
  stun_msg_create(msg, STUN_CLASS_REQUEST | STUN_METHOD_BINDING);

  // Both normal checks and heartbeats need full ICE credentials (RFC 7675 consent freshness).
  // Without USERNAME + MESSAGE-INTEGRITY the peer rejects with 400 Bad Request.
  char username[584];
  memset(username, 0, sizeof(username));
  snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);
  stun_msg_write_attr(msg, STUN_ATTR_TYPE_USERNAME, strlen(username), username);

  // RFC 8445 §7.1.1: PRIORITY attribute must use peer-reflexive type preference
  uint32_t prflx_priority = ice_candidate_compute_prflx_priority(agent->nominated_pair->local);
  uint32_t priority_network = htonl(prflx_priority);
  stun_msg_write_attr(msg, STUN_ATTR_TYPE_PRIORITY, 4, (char*)&priority_network);
  if (agent->mode == AGENT_MODE_CONTROLLING) {
    uint64_t tie_breaker_network = htonll_u64(tie_breaker);
    stun_msg_write_attr(msg, STUN_ATTR_TYPE_ICE_CONTROLLING, 8, (char*)&tie_breaker_network);
    // USE-CANDIDATE only for initial nomination, not heartbeat (RFC 7675 consent freshness)
    if (!is_heartbeat) {
      stun_msg_write_attr(msg, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);
    }
  } else {
    uint64_t tie_breaker_network = htonll_u64(tie_breaker);
    stun_msg_write_attr(msg, STUN_ATTR_TYPE_ICE_CONTROLLED, 8, (char*)&tie_breaker_network);
  }
  stun_msg_finish(msg, STUN_CREDENTIAL_SHORT_TERM, agent->remote_upwd, strlen(agent->remote_upwd));
}

void agent_process_stun_request(Agent* agent, StunMessage* stun_msg, Address* addr, ChannelBinding* via_channel) {
  StunMessage msg;
  StunHeader* header;
  switch (stun_msg->stunmethod) {
    case STUN_METHOD_BINDING:
      if (stun_msg_is_valid(stun_msg->buf, stun_msg->size, agent->local_upwd, strlen(agent->local_upwd)) == 0) {
        header = (StunHeader*)stun_msg->buf;
        memcpy(agent->transaction_id, header->transaction_id, sizeof(header->transaction_id));
        agent_create_binding_response(agent, &msg, addr);

        // Route response via TURN ChannelData if request came that way
        if (via_channel && via_channel->bound) {
          uint8_t channel_buf[sizeof(msg.buf) + 4];
          int encoded = agent_channel_data_encode(via_channel->channel_number, msg.buf, msg.size, channel_buf, sizeof(channel_buf));
          if (encoded > 0) {
            LOGD("ICE: Sending binding response via TURN ChannelData ch=0x%04x", via_channel->channel_number);
            agent_socket_send(agent, &agent->turn_server_addr, channel_buf, encoded);
          } else {
            LOGE("ICE: Failed to encode binding response as ChannelData");
          }
        } else {
          // Direct response - use ICE socket to match source address of our binding requests
          // This ensures aiortc sees consistent source address for all ICE traffic
          agent_ice_socket_send(agent, addr, msg.buf, msg.size);
        }
        agent->binding_request_time = ports_get_epoch_time();
      }
      break;
    default:
      break;
  }
}

void agent_process_stun_response(Agent* agent, StunMessage* stun_msg) {
  LOGD("STUN response: method=0x%04x", stun_msg->stunmethod);
  switch (stun_msg->stunmethod) {
    case STUN_METHOD_BINDING: {
      int valid = stun_msg_is_valid(stun_msg->buf, stun_msg->size, agent->remote_upwd, strlen(agent->remote_upwd));
      if (valid == 0) {
        if (!agent->use_candidate) {
          // First successful check - connectivity confirmed. Now trigger nomination.
          LOGI("ICE: Binding response valid, connectivity confirmed. Starting nomination...");
          agent->use_candidate = 1;
          // Keep state as INPROGRESS to trigger another check with USE-CANDIDATE
        } else {
          // Nomination check succeeded - ICE complete!
          LOGI("ICE: Nomination confirmed, connected");
          agent->nominated_pair->state = ICE_CANDIDATE_STATE_SUCCEEDED;
        }
      } else {
        LOGW("ICE binding response validation FAILED");
      }
      break;
    }
    default:
      LOGW("Unknown STUN response method: 0x%04x", stun_msg->stunmethod);
      break;
  }
}

int agent_recv(Agent* agent, uint8_t* buf, int len) {
  int ret = -1;
  StunMessage stun_msg;
  Address addr;
  // During ICE checks we can see responses slightly delayed; do a small bounded wait
  // instead of a single 100ms poll to avoid missing packets under load.
  const int recv_attempts = 3;  // 3 * AGENT_POLL_TIMEOUT (100ms) = 300ms max
  ret = agent_socket_recv_attempts(agent, &addr, buf, len, recv_attempts);
  if (ret > 0) {
    // Log first byte to detect packet type
    LOGD("agent_recv: %d bytes, first_byte=0x%02x", ret, buf[0]);
    // ChannelData must be demuxed before STUN probe
    if (stun_is_channel_data(buf, ret)) {
      LOGD("TURN: ChannelData RECV (%d bytes)", ret);
      uint16_t channel = 0;
      const uint8_t* payload = NULL;
      size_t payload_len = 0;
      if (agent_channel_data_decode(buf, ret, &channel, &payload, &payload_len) == 0) {
        LOGD("TURN: ChannelData ch=0x%04x payload_len=%zu", channel, payload_len);
        ChannelBinding* binding = agent_channel_find_by_channel(agent, channel);
        if (binding && payload_len > 0 && stun_probe((uint8_t*)payload, payload_len) == 0) {
          StunMessage inner;
          memset(&inner, 0, sizeof(inner));
          memcpy(inner.buf, payload, payload_len);
          inner.size = payload_len;
          stun_parse_msg_buf(&inner);
          LOGD("TURN: ChannelData contains STUN class=0x%02x method=0x%04x", inner.stunclass, inner.stunmethod);
          if (inner.stunclass == STUN_CLASS_RESPONSE) {
            agent_process_stun_response(agent, &inner);
          } else if (inner.stunclass == STUN_CLASS_REQUEST) {
            // Process binding requests from peer via TURN relay
            // The peer's address is the channel's bound peer address
            // Pass channel binding so response goes back via TURN
            agent_process_stun_request(agent, &inner, &binding->peer_addr, binding);
          }
        } else if (!binding) {
          LOGW("TURN: ChannelData for unknown channel 0x%04x", channel);
        }
      } else {
        LOGW("TURN: ChannelData decode failed");
      }
      return 0;
    } else if (stun_probe(buf, ret) == 0) {
      memcpy(stun_msg.buf, buf, ret);
      stun_msg.size = ret;
      stun_parse_msg_buf(&stun_msg);
      LOGD("STUN RECV class=0x%02x method=0x%04x size=%d", stun_msg.stunclass, stun_msg.stunmethod, ret);
      // Handle TURN Data Indication: unwrap DATA payload and feed to STUN processor
      if (stun_msg.stunclass == STUN_CLASS_INDICATION && stun_msg.stunmethod == STUN_METHOD_DATA) {
        LOGD("TURN: Data Indication received, data_len=%zu", stun_msg.data_len);
        if (stun_msg.data_len > 0 && stun_probe(stun_msg.data, stun_msg.data_len) == 0) {
          StunMessage inner;
          memset(&inner, 0, sizeof(inner));
          memcpy(inner.buf, stun_msg.data, stun_msg.data_len);
          inner.size = stun_msg.data_len;
          stun_parse_msg_buf(&inner);
          if (inner.stunclass == STUN_CLASS_RESPONSE) {
            agent_process_stun_response(agent, &inner);
          } else if (inner.stunclass == STUN_CLASS_REQUEST) {
            // Process binding request from peer via TURN Data Indication
            // Look up channel binding for this peer to route response via TURN
            ChannelBinding* peer_binding = agent_channel_find_by_peer(agent, &stun_msg.peer_addr);
            agent_process_stun_request(agent, &inner, &stun_msg.peer_addr, peer_binding);
          }
        }
        return 0;
      }
      switch (stun_msg.stunclass) {
        case STUN_CLASS_REQUEST:
          // Direct request (not via TURN), respond directly
          agent_process_stun_request(agent, &stun_msg, &addr, NULL);
          break;
        case STUN_CLASS_RESPONSE:
          agent_process_stun_response(agent, &stun_msg);
          break;
        default:
          break;
      }
      ret = 0;
    }
  }
  return ret;
}

void agent_set_remote_description(Agent* agent, char* description) {
  /*
  a=ice-ufrag:Iexb
  a=ice-pwd:IexbSoY7JulyMbjKwISsG9
  a=candidate:1 1 UDP 1 36.231.28.50 38143 typ srflx
  */
  int i;

  LOGD("Set remote description:\n%s", description);

  char* line_start = description;
  char* line_end = NULL;

  while ((line_end = strstr(line_start, "\r\n")) != NULL) {
    if (strncmp(line_start, "a=ice-ufrag:", strlen("a=ice-ufrag:")) == 0) {
      strncpy(agent->remote_ufrag, line_start + strlen("a=ice-ufrag:"), line_end - line_start - strlen("a=ice-ufrag:"));

    } else if (strncmp(line_start, "a=ice-pwd:", strlen("a=ice-pwd:")) == 0) {
      strncpy(agent->remote_upwd, line_start + strlen("a=ice-pwd:"), line_end - line_start - strlen("a=ice-pwd:"));

    } else if (strncmp(line_start, "a=candidate:", strlen("a=candidate:")) == 0) {
      if (ice_candidate_from_description(&agent->remote_candidates[agent->remote_candidates_count], line_start, line_end) == 0) {
        for (i = 0; i < agent->remote_candidates_count; i++) {
          if (strcmp(agent->remote_candidates[i].foundation, agent->remote_candidates[agent->remote_candidates_count].foundation) == 0) {
            break;
          }
        }
        if (i == agent->remote_candidates_count) {
          agent->remote_candidates_count++;
        }
      }
    }

    line_start = line_end + 2;
  }

  // Avoid logging ICE passwords; log lengths instead (helps debug mismatched credentials).
  LOGI("ICE creds: local_ufrag=%s remote_ufrag=%s remote_pwd_len=%zu",
       agent->local_ufrag,
       agent->remote_ufrag,
       strlen(agent->remote_upwd));
}

void agent_update_candidate_pairs(Agent* agent) {
  int i, j;
  // Please set gather candidates before set remote description
  for (i = 0; i < agent->local_candidates_count; i++) {
    for (j = 0; j < agent->remote_candidates_count; j++) {
      // NOTE: We no longer skip "unreachable" pairs. Even though sending from
      // a public IP to a remote private IP seems futile, the act of sending
      // establishes NAT/firewall state and triggers the remote peer's ICE agent.
      // aiortc sends to ALL candidates including private host addresses, and
      // this is required for some ICE implementations (like Pipecat/Daily) to respond.
      if (agent->local_candidates[i].addr.family == agent->remote_candidates[j].addr.family) {
        agent->candidate_pairs[agent->candidate_pairs_num].local = &agent->local_candidates[i];
        agent->candidate_pairs[agent->candidate_pairs_num].remote = &agent->remote_candidates[j];
        agent->candidate_pairs[agent->candidate_pairs_num].priority = agent->local_candidates[i].priority + agent->remote_candidates[j].priority;
        agent->candidate_pairs[agent->candidate_pairs_num].state = ICE_CANDIDATE_STATE_FROZEN;
        // Debug: log each candidate pair with types and priority
        LOGI("ICE: Pair[%d] local_type=%d remote_type=%d priority=%" PRIu64 " (local_pri=%" PRIu32 " + remote_pri=%" PRIu32 ")",
             agent->candidate_pairs_num,
             agent->local_candidates[i].type,
             agent->remote_candidates[j].type,
             agent->candidate_pairs[agent->candidate_pairs_num].priority,
             agent->local_candidates[i].priority,
             agent->remote_candidates[j].priority);
        agent->candidate_pairs_num++;
      }
    }
  }
  LOGI("ICE: %d candidate pairs created", agent->candidate_pairs_num);
  // Log summary by type for quick analysis
  int host_pairs = 0, srflx_pairs = 0, relay_pairs = 0;
  for (i = 0; i < agent->candidate_pairs_num; i++) {
    if (agent->candidate_pairs[i].local->type == ICE_CANDIDATE_TYPE_RELAY ||
        agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_RELAY) {
      relay_pairs++;
    } else if (agent->candidate_pairs[i].local->type == ICE_CANDIDATE_TYPE_SRFLX ||
               agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_SRFLX) {
      srflx_pairs++;
    } else {
      host_pairs++;
    }
  }
  LOGI("ICE: Pair types: host=%d srflx=%d relay=%d", host_pairs, srflx_pairs, relay_pairs);

  // NOTE: We intentionally do NOT pre-create TURN permissions here.
  // aiortc and other WebRTC implementations start ICE checks immediately without
  // pre-creating permissions. Permissions are created lazily only when we need
  // to send via our own TURN relay (ChannelData). For direct sends to the remote's
  // relay address, no permission on our TURN server is needed.
  // Pre-creating permissions adds delay and can cause Pipecat to time out.
}

int agent_connectivity_check(Agent* agent, int is_heartbeat) {
  uint8_t buf[1400];
  StunMessage msg;
  static int s_logged_first_binding_request = 0;

  // For heartbeat mode, we don't need a nominated pair in progress
  if (!is_heartbeat && agent->nominated_pair->state != ICE_CANDIDATE_STATE_INPROGRESS) {
    return -1;
  }

  memset(&msg, 0, sizeof(msg));

  if (is_heartbeat || (agent->nominated_pair->conncheck % AGENT_CONNCHECK_PERIOD == 0)) {
    agent_create_binding_request(agent, &msg, is_heartbeat);

    if (!s_logged_first_binding_request) {
      // Log the first Binding Request we generate, to verify ICE credentials & key fields.
      // Keep it compact to avoid log spam.
      StunMessage dbg;
      memset(&dbg, 0, sizeof(dbg));
      memcpy(dbg.buf, msg.buf, msg.size);
      dbg.size = msg.size;
      stun_parse_msg_buf(&dbg);

      LOGI("ICE: BindingRequest dbg: class=0x%02x method=0x%04x size=%zu username_len=%zu",
           dbg.stunclass, dbg.stunmethod, dbg.size, dbg.username_len);
      LOGI("ICE: BindingRequest dbg: username='%s'", dbg.username);
      LOGI("ICE: BindingRequest dbg: local_candidate_priority=%" PRIu32, agent->nominated_pair->local->priority);
      LOGI("ICE: BindingRequest dbg: agent_mode=%d (0=CONTROLLED,1=CONTROLLING)", agent->mode);

      // Hex dump first 48 bytes for debugging STUN message format
      char hexdump[150];
      int hlen = 0;
      for (size_t i = 0; i < 48 && i < msg.size; i++) {
        hlen += snprintf(hexdump + hlen, sizeof(hexdump) - hlen, "%02x ", (unsigned char)msg.buf[i]);
      }
      LOGI("ICE: BindingRequest hex (first 48): %s", hexdump);

      s_logged_first_binding_request = 1;
    }

    int sent = 0;
    if (agent->nominated_pair->local->type == ICE_CANDIDATE_TYPE_RELAY) {
      ChannelBinding* binding = NULL;
      uint8_t channel_buf[sizeof(msg.buf) + 4];
      if (agent_channel_allocate(agent, &agent->nominated_pair->remote->addr, &binding) == 0) {
        if (!binding->bound) {
          if (agent_turn_channel_bind(agent, binding) < 0) {
            LOGE("TURN: ChannelBind failed");
            sent = -1;
          }
        }
        if (binding->bound) {
          int encoded = agent_channel_data_encode(binding->channel_number, msg.buf, msg.size, channel_buf, sizeof(channel_buf));
          if (encoded < 0) {
            LOGE("TURN: ChannelData encode failed");
            sent = -1;
          } else {
            sent = agent_socket_send(agent, &agent->turn_server_addr, channel_buf, encoded);
            LOGD("TURN: ChannelData SEND ch=0x%04x len=%d sent=%d", binding->channel_number, encoded, sent);
          }
        }
      } else {
        sent = -1;
      }
    } else {
      // Direct binding request (host/srflx/relay candidate) - use separate ICE socket
      // Using the ICE socket (different from TURN socket) avoids NAT/firewall issues
      // where Cloudflare may reject non-TURN traffic on the TURN-bound socket.
      char addr_str[64];
      addr_to_string(&agent->nominated_pair->remote->addr, addr_str, sizeof(addr_str));
      LOGD("ICE: Direct SEND to %s:%u (type=%d)",
           addr_str,
           (unsigned)agent->nominated_pair->remote->addr.port,
           agent->nominated_pair->local->type);
      sent = agent_ice_socket_send(agent, &agent->nominated_pair->remote->addr, msg.buf, msg.size);
    }
    if (sent < 0) {
      LOGE("ICE: Failed to send binding request");
    }
  }

  // Only poll for binding response during initial connectivity checks.
  // During heartbeat, skip agent_recv to avoid consuming RTP/RTCP/DTLS packets
  // that should be processed by peer_connection_loop. The heartbeat binding
  // response will be picked up by the main agent_recv in the next loop iteration.
  if (!is_heartbeat) {
    agent_recv(agent, buf, sizeof(buf));
  }

  if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_SUCCEEDED) {
    LOGD("ICE: Connected");
    agent->selected_pair = agent->nominated_pair;
    return 0;
  }

  return -1;
}

int agent_select_candidate_pair(Agent* agent) {
  int i;

  // If we already have a pair in progress, continue it.
  for (i = 0; i < agent->candidate_pairs_num; i++) {
    if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_INPROGRESS) {
      agent->candidate_pairs[i].conncheck++;
      if (agent->candidate_pairs[i].conncheck < AGENT_CONNCHECK_MAX) {
        return 0;
      }
      agent->candidate_pairs[i].state = ICE_CANDIDATE_STATE_FAILED;
      break;
    }
    if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_SUCCEEDED) {
      agent->selected_pair = &agent->candidate_pairs[i];
      return 0;
    }
  }

  // Choose next pair. If prefer_relay is set, try relay→relay pairs first.
  // This is essential for cloud services like Pipecat where NAT/firewall rules
  // often block direct connectivity but relay→relay always works.
  int chosen_idx = -1;
  uint64_t best_priority = 0;

  if (agent->prefer_relay) {
    // For cloud services: aiortc sends to ALL candidate types including remote HOST.
    // Even though remote host addresses (172.31.x.x) are private and "unreachable",
    // sending to them triggers the remote peer's ICE agent to start responding.
    //
    // Strategy: Try host→host FIRST (triggers response), then srflx, then relay

    // First pass: host→host pairs (critical for triggering Pipecat's response)
    for (i = 0; i < agent->candidate_pairs_num; i++) {
      if (agent->candidate_pairs[i].state != ICE_CANDIDATE_STATE_FROZEN) continue;
      if (agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_HOST &&
          agent->candidate_pairs[i].local->type == ICE_CANDIDATE_TYPE_HOST) {
        if (chosen_idx < 0 || agent->candidate_pairs[i].priority > best_priority) {
          chosen_idx = i;
          best_priority = agent->candidate_pairs[i].priority;
        }
      }
    }
    if (chosen_idx >= 0) {
      LOGI("ICE: prefer_relay mode - trying host→host first (triggers remote ICE)");
    }

    // Second pass: host→srflx pairs (establishes our IP in Pipecat's view)
    if (chosen_idx < 0) {
      for (i = 0; i < agent->candidate_pairs_num; i++) {
        if (agent->candidate_pairs[i].state != ICE_CANDIDATE_STATE_FROZEN) continue;
        if (agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_SRFLX &&
            agent->candidate_pairs[i].local->type == ICE_CANDIDATE_TYPE_HOST) {
          if (chosen_idx < 0 || agent->candidate_pairs[i].priority > best_priority) {
            chosen_idx = i;
            best_priority = agent->candidate_pairs[i].priority;
          }
        }
      }
      if (chosen_idx >= 0) {
        LOGI("ICE: prefer_relay mode - trying host→srflx (to establish path)");
      }
    }

    // Second pass: host/srflx→relay pairs (direct send to remote relay)
    if (chosen_idx < 0) {
      for (i = 0; i < agent->candidate_pairs_num; i++) {
        if (agent->candidate_pairs[i].state != ICE_CANDIDATE_STATE_FROZEN) continue;
        if (agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_RELAY &&
            agent->candidate_pairs[i].local->type != ICE_CANDIDATE_TYPE_RELAY) {
          if (chosen_idx < 0 || agent->candidate_pairs[i].priority > best_priority) {
            chosen_idx = i;
            best_priority = agent->candidate_pairs[i].priority;
          }
        }
      }
      if (chosen_idx >= 0) {
        LOGI("ICE: prefer_relay mode - using host/srflx→relay pair (direct to remote relay)");
      }
    }

    // Third pass: try relay→relay if nothing else works
    if (chosen_idx < 0) {
      for (i = 0; i < agent->candidate_pairs_num; i++) {
        if (agent->candidate_pairs[i].state != ICE_CANDIDATE_STATE_FROZEN) continue;
        if (agent->candidate_pairs[i].local->type == ICE_CANDIDATE_TYPE_RELAY &&
            agent->candidate_pairs[i].remote->type == ICE_CANDIDATE_TYPE_RELAY) {
          if (chosen_idx < 0 || agent->candidate_pairs[i].priority > best_priority) {
            chosen_idx = i;
            best_priority = agent->candidate_pairs[i].priority;
          }
        }
      }
      if (chosen_idx >= 0) {
        LOGI("ICE: prefer_relay mode - using relay→relay pair");
      }
    }
  }

  // Standard pass: choose by highest ICE priority (host > srflx > relay)
  if (chosen_idx < 0) {
    for (i = 0; i < agent->candidate_pairs_num; i++) {
      if (agent->candidate_pairs[i].state != ICE_CANDIDATE_STATE_FROZEN) continue;
      if (chosen_idx < 0 || agent->candidate_pairs[i].priority > best_priority) {
        chosen_idx = i;
        best_priority = agent->candidate_pairs[i].priority;
      }
    }
  }

  if (chosen_idx >= 0) {
    agent->nominated_pair = &agent->candidate_pairs[chosen_idx];
    agent->candidate_pairs[chosen_idx].conncheck = 0;
    agent->candidate_pairs[chosen_idx].state = ICE_CANDIDATE_STATE_INPROGRESS;
    LOGI("ICE: Checking pair %d (priority=%" PRIu64 ", local_type=%d remote_type=%d)", chosen_idx,
         agent->candidate_pairs[chosen_idx].priority,
         agent->candidate_pairs[chosen_idx].local->type, agent->candidate_pairs[chosen_idx].remote->type);
    return 0;
  }

  // all candidate pairs are failed
  LOGE("ICE: All %d candidate pairs FAILED! (check if any responses were received)", agent->candidate_pairs_num);
  return -1;
}
