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
#include "agent.h"

static void agent_get_stun_candidates(Agent *agent) {

  Address addr;

  stun_get_local_address(STUN_IP, STUN_PORT, &addr);

  ice_candidate_create(&agent->local_candidates[agent->local_candidates_count++], ICE_CANDIDATE_TYPE_SRFLX, &addr);

}

void agent_set_host_address(Agent *agent, const char *ip) {

  agent->host_addr.family = AF_INET;
  inet_pton(AF_INET, ip, agent->host_addr.ipv4);
  agent->b_host_addr = 1;
}

void agent_gather_candidates(Agent *agent) {

  int ret, i;

  Address addr[AGENT_MAX_CANDIDATES];

  memset(agent->local_candidates, 0, sizeof(agent->local_candidates));

  udp_socket_open(&agent->udp_socket);

  if (agent->b_host_addr) {

    memcpy(&addr[0], &agent->host_addr, sizeof(agent->host_addr));
    ret = 1;
  } else {
    ret = udp_socket_get_host_address(&agent->udp_socket, addr);
  }

  for (i = 0; i < ret; i++) {

    ice_candidate_create(&agent->local_candidates[agent->local_candidates_count++], ICE_CANDIDATE_TYPE_HOST, &addr[i]);
  }

  agent_get_stun_candidates(agent);

  for (i = 0; i < ret; i++) {

    agent->local_candidates[i].addr.port = agent->local_candidates[agent->local_candidates_count-1].addr.port;
  }

}

void agent_get_local_description(Agent *agent, char *description, int length) {

  char buffer[1024];

  memset(description, 0, length);

  memset(agent->local_ufrag, 0, sizeof(agent->local_ufrag));
  memset(agent->local_upwd, 0, sizeof(agent->local_upwd));

  utils_random_string(agent->local_ufrag, ICE_UFRAG_LENGTH);
  utils_random_string(agent->local_upwd, ICE_UPWD_LENGTH);

  snprintf(description, length, "a=ice-ufrag:%s\na=ice-pwd:%s\n", agent->local_ufrag, agent->local_upwd);

  for (int i = 0; i < agent->local_candidates_count; i++) {

    memset(buffer, 0, sizeof(buffer));

    agent->local_candidates[i].foundation = i + 1;

    ice_candidate_to_description(&agent->local_candidates[i], buffer, sizeof(buffer));

    strncat(description, buffer, length - strlen(description) - 1);
  }

  // remove last \n
  description[strlen(description)] = '\0';

  udp_socket_bind(&agent->udp_socket, &agent->local_candidates[0].addr);
}

int agent_send(Agent *agent, const uint8_t *buf, int len) {

  //printf("send to ip: %d.%d.%d.%d:%d\n", agent->remote_candidates[0].addr.ipv4[0], agent->remote_candidates[0].addr.ipv4[1], agent->remote_candidates[0].addr.ipv4[2], agent->remote_candidates[0].addr.ipv4[3], agent->remote_candidates[0].addr.port);

  //udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, buf, len);
  return udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, buf, len);
}

int agent_recv(Agent *agent, uint8_t *buf, int len) {

  int ret;

  memset(buf, 0, len); 
  ret = udp_socket_recvfrom(&agent->udp_socket, &agent->nominated_pair->local->addr, buf, len);
  if (ret > 0) {

    StunMsgType type = stun_is_stun_msg(buf, ret);


    if (type == STUN_MSG_TYPE_BINDING_ERROR_RESPONSE) {

      agent->nominated_pair->state = ICE_CANDIDATE_STATE_WAITING;
      LOGD("recv STUN_MSG_TYPE_BINDING_ERROR_RESPONSE");
    } else if (type == STUN_MSG_TYPE_BINDING_RESPONSE) {

      LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE");

      if (stun_response_is_valid(buf, ret, agent->remote_upwd) == 0) {

        LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE is valid");

        agent->nominated_pair->state = ICE_CANDIDATE_STATE_SUCCEEDED;
      }

    } else if (type == STUN_MSG_TYPE_BINDING_REQUEST) {

      if (stun_response_is_valid(buf, ret, agent->local_upwd) == 0) {

        StunHeader *header = (StunHeader *)buf;
        memcpy(agent->transaction_id, header->transaction_id, sizeof(header->transaction_id));
        //LOGD("recv STUN_MSG_TYPE_BINDING_REQUEST is valid");

        StunMessage msg;
        stun_msg_create(&msg, STUN_MSG_TYPE_BINDING_RESPONSE);
   
        header = (StunHeader *)msg.buf;
        memcpy(header->transaction_id, agent->transaction_id, sizeof(header->transaction_id));
 
        char username[64];

        snprintf(username, sizeof(username), "%s:%s", agent->local_ufrag, agent->remote_ufrag);

        // TODO: XOR-MAPPED-ADDRESS
        char mapped_address[8];
        stun_set_mapped_address(mapped_address, NULL, &agent->nominated_pair->remote->addr);
        stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_MAPPED_ADDRESS, 8, mapped_address);
        stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);
        stun_msg_finish(&msg, agent->local_upwd);

        udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, msg.buf, msg.size);
      LOGD("send binding respnse to remote ip: %d.%d.%d.%d, port: %d", agent->nominated_pair->remote->addr.ipv4[0], agent->nominated_pair->remote->addr.ipv4[1], agent->nominated_pair->remote->addr.ipv4[2], agent->nominated_pair->remote->addr.ipv4[3], agent->nominated_pair->remote->addr.port);


      }
    } else {

      LOGD("Not STUN message recvied");
      return ret;

    }

  }

  return -1;
}



void agent_set_remote_description(Agent *agent, char *description) {

/*
a=ice-ufrag:Iexb
a=ice-pwd:IexbSoY7JulyMbjKwISsG9
a=candidate:1 1 UDP 1 36.231.28.50 38143 typ srflx
*/
  int i, j;

  LOGD("Set remote description:\n%s", description);

  char *line = strtok(description, "\r\n");

  while (line) {

    if (strncmp(line, "a=ice-ufrag:", strlen("a=ice-ufrag:")) == 0) {

      memcpy(agent->remote_ufrag, line + strlen("a=ice-ufrag:"), ICE_UFRAG_LENGTH);

    } else if (strncmp(line, "a=ice-pwd:", strlen("a=ice-pwd:")) == 0) {

      memcpy(agent->remote_upwd, line + strlen("a=ice-pwd:"), ICE_UPWD_LENGTH);

    } else if (strncmp(line, "a=candidate:", strlen("a=candidate:")) == 0) {

      if (ice_candidate_from_description(&agent->remote_candidates[agent->remote_candidates_count], line) == 0) {
        agent->remote_candidates_count++;
      }
    }

    line = strtok(NULL, "\r\n");
  }

  LOGD("remote ufrag: %s", agent->remote_ufrag);
  LOGD("remote upwd: %s", agent->remote_upwd);

  // Please set gather candidates before set remote description
  for (i = 0; i < agent->local_candidates_count; i++) {

    for (j = 0; j < agent->remote_candidates_count; j++) {

      agent->candidate_pairs[agent->candidate_pairs_num].local = &agent->local_candidates[i];
      agent->candidate_pairs[agent->candidate_pairs_num].remote = &agent->remote_candidates[j];
      agent->candidate_pairs[agent->candidate_pairs_num].priority = agent->local_candidates[i].priority + agent->remote_candidates[j].priority;
      agent->candidate_pairs[agent->candidate_pairs_num].state = ICE_CANDIDATE_STATE_FROZEN;
      agent->candidate_pairs_num++;
    }
  }

  agent->state = AGENT_STATE_READY;
}

int agent_connectivity_check(Agent *agent) {

  uint8_t buf[1400];

  StunMessage msg;
  memset(&msg, 0, sizeof(msg));
  StunHeader *header = (StunHeader *)msg.buf;


    if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_WAITING) {

      agent_recv(agent, buf, sizeof(buf));
      stun_create_binding_request(&msg);
      char username[64];
      memset(username, 0, sizeof(username));
      snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);
      stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_PRIORITY, 4, (char *)&agent->nominated_pair->priority); 
      stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);
      stun_msg_finish(&msg, agent->remote_upwd);

      LOGD("send binding request to remote ip: %d.%d.%d.%d, port: %d", agent->nominated_pair->remote->addr.ipv4[0], agent->nominated_pair->remote->addr.ipv4[1], agent->nominated_pair->remote->addr.ipv4[2], agent->nominated_pair->remote->addr.ipv4[3], agent->nominated_pair->remote->addr.port);

     udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, msg.buf, msg.size);

     agent->nominated_pair->state = ICE_CANDIDATE_STATE_INPROGRESS;

    } else if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_INPROGRESS) {

      agent_recv(agent, buf, sizeof(buf));
    }

#if 0
  if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_WAITING) {
 
    stun_create_binding_request(&msg);
    char username[64];
    memset(username, 0, sizeof(username));
    snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

    stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);
    stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);
    stun_msg_finish(&msg, agent->remote_upwd);
    udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, (char *)msg.buf, msg.size);
    agent->nominated_pair->state = ICE_CANDIDATE_STATE_INPROGRESS;
  }
#endif
  return agent->nominated_pair->state == ICE_CANDIDATE_STATE_SUCCEEDED;
}

void agent_select_candidate_pair(Agent *agent) {

  int i;

  static time_t t1 = 0;
  if (t1 == 0) {
    t1 = time(NULL);
  }

  time_t t2 = time(NULL);


  //LOGD("Mode: %d", agent->mode);

  for (i = 0; i < agent->candidate_pairs_num; i++) {

    if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_FROZEN) {

      // nominate this pair
      agent->nominated_pair = &agent->candidate_pairs[i];
      agent->nominated_pair->state = ICE_CANDIDATE_STATE_WAITING;
      break;

    } else if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_INPROGRESS && (t2 - t1) > 2) {
      LOGD("timeout for nominate (pair %d)", i);
      agent->nominated_pair->state = ICE_CANDIDATE_STATE_FAILED;
      t1 = t2;

    } else if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_FAILED) {

    } else if (agent->candidate_pairs[i].state <= ICE_CANDIDATE_STATE_SUCCEEDED) {
      // still in progress. wait for it 
      agent->nominated_pair = &agent->candidate_pairs[i];
      break;
    }

  }

  //LOGD("nominated_pair: %p", agent->nominated_pair);
}

int agent_loop(Agent *agent) {

  uint8_t buf[1024];
  // try to connect to remote

  if (agent->state < AGENT_STATE_CONNECTED && agent->selected_pair == NULL) {

    agent_select_candidate_pair(agent);

    if (agent_connectivity_check(agent)) {

      LOGD("Connectivity check success. pair: %p", agent->nominated_pair);

      agent->state = AGENT_STATE_CONNECTED;
      agent->selected_pair = agent->nominated_pair;
    }
  }

  agent_recv(agent, buf, sizeof(buf));
  return 0;
}
