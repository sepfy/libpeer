#ifndef AGENT_H_
#define AGENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base64.h"
#include "ice.h"
#include "socket.h"
#include "stun.h"
#include "utils.h"

#ifndef AGENT_MAX_DESCRIPTION
#define AGENT_MAX_DESCRIPTION 40960
#endif

#ifndef AGENT_MAX_CANDIDATES
#define AGENT_MAX_CANDIDATES 10
#endif

#ifndef AGENT_MAX_CANDIDATE_PAIRS
#define AGENT_MAX_CANDIDATE_PAIRS 100
#endif

#ifndef AGENT_MAX_CHANNEL_BINDINGS
#define AGENT_MAX_CHANNEL_BINDINGS 8
#endif

#define CHANNEL_NUMBER_MIN 0x4000
#define CHANNEL_NUMBER_MAX 0x7FFF

typedef enum AgentState {

  AGENT_STATE_GATHERING_ENDED = 0,
  AGENT_STATE_GATHERING_STARTED,
  AGENT_STATE_GATHERING_COMPLETED,

} AgentState;

typedef enum AgentMode {

  AGENT_MODE_CONTROLLED = 0,
  AGENT_MODE_CONTROLLING

} AgentMode;

typedef struct Agent Agent;

typedef struct {
  uint16_t channel_number;  // 0 = unused, else 0x4000-0x7FFF
  Address peer_addr;
  int bound;                // 1 once ChannelBind succeeds
} ChannelBinding;

struct Agent {
  char remote_ufrag[ICE_UFRAG_LENGTH + 1];
  char remote_upwd[ICE_UPWD_LENGTH + 1];

  char local_ufrag[ICE_UFRAG_LENGTH + 1];
  char local_upwd[ICE_UPWD_LENGTH + 1];

  IceCandidate local_candidates[AGENT_MAX_CANDIDATES];
  IceCandidate remote_candidates[AGENT_MAX_CANDIDATES];

  int local_candidates_count;
  int remote_candidates_count;

  UdpSocket udp_sockets[2];

  // Separate socket for direct ICE connectivity checks.
  // Using a different socket than TURN avoids NAT/firewall issues where
  // Cloudflare may reject non-TURN traffic on the TURN-bound socket.
  UdpSocket ice_socket;

  Address host_addr;
  int b_host_addr;
  uint64_t binding_request_time;
  AgentState state;

  AgentMode mode;

  IceCandidatePair candidate_pairs[AGENT_MAX_CANDIDATE_PAIRS];
  IceCandidatePair* selected_pair;
  IceCandidatePair* nominated_pair;

  int candidate_pairs_num;
  int use_candidate;
  uint32_t transaction_id[3];

  // TURN allocation context
  Address turn_server_addr;
  int has_turn_allocation;
  char turn_username[256];
  size_t turn_username_len;
  char turn_credential[256];
  size_t turn_credential_len;
  char turn_realm[128];
  size_t turn_realm_len;
  char turn_nonce[192];
  size_t turn_nonce_len;

  // TURN Channel Binding state
  ChannelBinding channel_bindings[AGENT_MAX_CHANNEL_BINDINGS];
  uint16_t next_channel_number;

  // Relay-first mode: prioritize relay→relay pairs for cloud services
  // When set, relay pairs are tried before host/srflx pairs
  int prefer_relay;
};

void agent_gather_candidate(Agent* agent, const char* urls, const char* username, const char* credential);

void agent_create_ice_credential(Agent* agent);

void agent_get_local_description(Agent* agent, char* description, int length);

int agent_send(Agent* agent, const uint8_t* buf, int len);

int agent_recv(Agent* agent, uint8_t* buf, int len);

void agent_set_remote_description(Agent* agent, char* description);

int agent_select_candidate_pair(Agent* agent);

int agent_connectivity_check(Agent* agent, int is_heartbeat);

void agent_clear_candidates(Agent* agent);

int agent_create(Agent* agent);

void agent_destroy(Agent* agent);

void agent_update_candidate_pairs(Agent* agent);

// Exposed for unit testing ChannelData helpers
int agent_channel_data_encode(uint16_t channel, const uint8_t* payload, size_t payload_len, uint8_t* out, size_t out_size);
int agent_channel_data_decode(const uint8_t* data, size_t data_len, uint16_t* channel, const uint8_t** payload, size_t* payload_len);

// Build a TURN ChannelBind request (no network I/O). Exposed for tests.
int agent_build_channel_bind_request(Agent* agent, ChannelBinding* binding, StunMessage* out_msg);

// Build a STUN Binding Request (no network I/O). Exposed for tests.
// is_heartbeat=1: consent freshness check (no USE-CANDIDATE), is_heartbeat=0: nomination check.
void agent_create_binding_request(Agent* agent, StunMessage* msg, int is_heartbeat);

// Channel state management - exposed for unit testing
void agent_channel_init(Agent* agent);
ChannelBinding* agent_channel_find_by_peer(Agent* agent, const Address* peer_addr);
ChannelBinding* agent_channel_find_by_channel(Agent* agent, uint16_t channel);
int agent_channel_allocate(Agent* agent, const Address* peer_addr, ChannelBinding** out_binding);

#endif  // AGENT_H_
