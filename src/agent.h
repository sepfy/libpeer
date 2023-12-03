#ifndef AGENT_H_
#define AGENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "udp.h"
#include "utils.h"
#include "stun.h"
#include "ice.h"
#include "base64.h"

#define AGENT_MAX_DESCRIPTION 40960
#define AGENT_MAX_CANDIDATES 10
#define AGENT_MAX_CANDIDATE_PAIRS 100

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

struct Agent {

  char remote_ufrag[ICE_UFRAG_LENGTH + 1];
  char remote_upwd[ICE_UPWD_LENGTH + 1];

  char local_ufrag[ICE_UFRAG_LENGTH + 1];
  char local_upwd[ICE_UPWD_LENGTH + 1];

  IceCandidate local_candidates[AGENT_MAX_CANDIDATES];

  IceCandidate remote_candidates[AGENT_MAX_CANDIDATES];

  int local_candidates_count;

  int remote_candidates_count;

  UdpSocket udp_socket;

  Address host_addr;
  int b_host_addr;
  int controlling;
  uint64_t binding_request_time;
  AgentState state;

  AgentMode mode;

  IceCandidatePair candidate_pairs[AGENT_MAX_CANDIDATE_PAIRS];
  IceCandidatePair *selected_pair;
  IceCandidatePair *nominated_pair;

  int candidate_pairs_num;

  int use_candidate;

  uint32_t transaction_id[3];
};

void agent_gather_candidate(Agent *agent, const char *urls, const char *username, const char *credential);

void agent_get_local_description(Agent *agent, char *description, int length);

int agent_loop(Agent *agent);

int agent_send(Agent *agent, const uint8_t *buf, int len);

int agent_recv(Agent *agent, uint8_t *buf, int len);

void agent_set_remote_description(Agent *agent, char *description);

void *agent_thread(void *arg);

void agent_select_candidate_pair(Agent *agent);

void agent_attach_recv_cb(Agent *agent, void (*data_recv_cb)(char *buf, int len, void *user_data));

void agent_set_host_address(Agent *agent, Address *addr);

int agent_connectivity_check(Agent *agent);

void agent_init(Agent *agent);

void agent_deinit(Agent *agent);

#endif // AGENT_H_
