#ifndef AGENT_H_
#define AGENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <pthread.h>

#include "udp.h"
#include "utils.h"
#include "stun.h"
#include "ice.h"
#include "base64.h"

#define STUN_IP "142.250.21.127"
#define STUN_PORT 19302

#define AGENT_MAX_DESCRIPTION 40960
#define AGENT_MAX_CANDIDATES 10


typedef enum AgentState {

  AGENT_STATE_NEW = 0,
  AGENT_STATE_GATHERING,
  AGENT_STATE_READY,
  AGENT_STATE_FINISHED,
  AGENT_STATE_CONNECTED,
  AGENT_STATE_FAILED

} AgentState;


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

  int controlling;

  AgentState state;

  IceCandidatePair selected_pair;

  int use_candidate;

  uint32_t transaction_id[3];

  void (*on_recv_cb)(Agent *agent, char *buf, int len, void *user_data);
};

void agent_gather_candidates(Agent *agent);

void agent_get_local_description(Agent *agent, char *description, int length);

void agent_send(Agent *agent, char *buf, int len);

int agent_recv(Agent *agent, char *buf, int len);

void agent_set_remote_description(Agent *agent, char *description);

void *agent_thread(void *arg);

void agent_select_candidate_pair(Agent *agent);

void agent_attach_recv_cb(Agent *agent, void (*on_recv_cb)(Agent *agent, char *buf, int len, void *user_data));

#endif // AGENT_H_

