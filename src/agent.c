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

void agent_gather_candidates(Agent *agent) {

  int ret, i, j, candidate_count;

  Address addr[AGENT_MAX_CANDIDATES];

  memset(agent->local_candidates, 0, sizeof(agent->local_candidates));

  udp_socket_open(&agent->udp_socket);

  ret = udp_socket_get_host_address(&agent->udp_socket, addr);

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

  char upwd[ICE_UPWD_LENGTH + 1];

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

void agent_send(Agent *agent, char *buf, int len) {

  printf("send to ip: %d.%d.%d.%d:%d\n", agent->remote_candidates[0].addr.ipv4[0], agent->remote_candidates[0].addr.ipv4[1], agent->remote_candidates[0].addr.ipv4[2], agent->remote_candidates[0].addr.ipv4[3], agent->remote_candidates[0].addr.port);

  udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, buf, len);

}

int agent_recv(Agent *agent, char *buf, int len) {

  int ret;
  printf("Listening port: %d\n", agent->local_candidates[0].addr.port);

  while (1) {

    memset(buf, 0, len); 
    ret = udp_socket_recvfrom(&agent->udp_socket, &agent->local_candidates[0].addr, buf, len);
    if (ret > 0) {

      StunMsgType type = stun_is_stun_msg(buf, ret);

      if (type == STUN_MSG_TYPE_BINDING_RESPONSE) {

        LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE");

        if (stun_response_is_valid(buf, ret, agent->remote_upwd) == 0) {

          LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE is valid");
          if (agent->use_candidate) {
            agent->selected_pair.local->state = ICE_CANDIDATE_STATE_SUCCEEDED;

          } else {

            LOGD("received STUN response, send STUN with USE-CANDIDATE");
            agent->use_candidate = 1;
            agent->selected_pair.local->state = ICE_CANDIDATE_STATE_INPROGRESS;
	  }
          break;
        }

      } else if (type == STUN_MSG_TYPE_BINDING_REQUEST) {

        if (stun_response_is_valid(buf, ret, agent->local_upwd) == 0) {

          StunHeader *header = (StunHeader *)buf;
          memcpy(agent->transaction_id, header->transaction_id, sizeof(header->transaction_id));
          LOGD("recv STUN_MSG_TYPE_BINDING_REQUEST is valid");
          agent->selected_pair.remote->state = ICE_CANDIDATE_STATE_INPROGRESS;
          break;
        }
      } else {
        LOGD("DTLS!!!!!!!!!!!!!!!!!!!!");
        LOGD("DTLS!!!!!!!!!!!!!!!!!!!!");
        LOGD("DTLS!!!!!!!!!!!!!!!!!!!!");
        LOGD("DTLS!!!!!!!!!!!!!!!!!!!!");
        LOGD("DTLS!!!!!!!!!!!!!!!!!!!!");
        LOGD("Not STUN message recvfrom: %s", buf);
        return ret;
      }
    }
  }
  return 0;
}



void agent_set_remote_description(Agent *agent, char *description) {

/*
a=ice-ufrag:Iexb
a=ice-pwd:IexbSoY7JulyMbjKwISsG9
a=candidate:1 1 UDP 1 36.231.28.50 38143 typ srflx
*/

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

}

void agent_select_candidate_pair(Agent *agent) {

  int i, j;

  uint64_t priority = 0;

  agent->selected_pair.local = &agent->local_candidates[0];

  agent->selected_pair.remote = &agent->remote_candidates[0];

  StunMessage msg;

  StunHeader *header = (StunHeader *)msg.buf;

  char buf[1400];

  LOGD("controlling mode: %d", agent->controlling);

  if (!agent->controlling) {

    agent_recv(agent, buf, sizeof(buf));

    if (agent->selected_pair.remote->state == ICE_CANDIDATE_STATE_INPROGRESS) {

      stun_msg_create(&msg, STUN_MSG_TYPE_BINDING_RESPONSE);
   
      StunHeader *header = (StunHeader *)msg.buf;
      memcpy(header->transaction_id, agent->transaction_id, sizeof(header->transaction_id));
 
      char username[64];

      snprintf(username, sizeof(username), "%s:%s", agent->local_ufrag, agent->remote_ufrag);

      // TODO: XOR-MAPPED-ADDRESS
      char mapped_address[8];
      stun_set_mapped_address(mapped_address, NULL, &agent->selected_pair.remote->addr);

      stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_MAPPED_ADDRESS, 8, mapped_address);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);

      stun_msg_finish(&msg, agent->local_upwd);

      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, (char *)msg.buf, msg.size);

      LOGD("send to response to remote");
      agent->selected_pair.remote->state = ICE_CANDIDATE_STATE_SUCCEEDED;
      agent->state = AGENT_STATE_CONNECTED;

    } else if(agent->selected_pair.local->state == ICE_CANDIDATE_STATE_SUCCEEDED) {

    

    } else if (agent->selected_pair.remote->state == ICE_CANDIDATE_STATE_SUCCEEDED) {

      LOGD("send to request to remote");
      stun_create_binding_request(&msg);

      char username[64];

      snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);

      stun_msg_finish(&msg, agent->remote_upwd);

      agent->use_candidate = 1;

      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, (char *)msg.buf, msg.size);

    }
    

  } else {

    if (agent->selected_pair.local->state <= ICE_CANDIDATE_STATE_WAITING) {

      LOGD("send to request to remote");
      stun_create_binding_request(&msg);

      char username[64];

      snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);

      stun_msg_finish(&msg, agent->remote_upwd);

      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, (char *)msg.buf, msg.size);

    } else if (agent->selected_pair.local->state == ICE_CANDIDATE_STATE_INPROGRESS) {

      LOGD("send to request to remote");
      stun_create_binding_request(&msg);

      char username[64];

      snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);

      stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);

      stun_msg_finish(&msg, agent->remote_upwd);

      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, (char *)msg.buf, msg.size);

    } else if (agent->selected_pair.remote->state == ICE_CANDIDATE_STATE_INPROGRESS) {

      stun_msg_create(&msg, STUN_MSG_TYPE_BINDING_RESPONSE);
   
      StunHeader *header = (StunHeader *)msg.buf;
      memcpy(header->transaction_id, agent->transaction_id, sizeof(header->transaction_id));
 
      char username[64];

      snprintf(username, sizeof(username), "%s:%s", agent->local_ufrag, agent->remote_ufrag);

      // TODO: XOR-MAPPED-ADDRESS
      char mapped_address[8];
      stun_set_mapped_address(mapped_address, NULL, &agent->selected_pair.remote->addr);

      stun_msg_write_attr2(&msg, STUN_ATTR_TYPE_MAPPED_ADDRESS, 8, mapped_address);

      stun_msg_write_attr(&msg, STUN_ATTRIBUTE_USERNAME, strlen(username), username);

      stun_msg_finish(&msg, agent->local_upwd);

      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, (char *)msg.buf, msg.size);

      LOGD("send to response to remote");
      agent->selected_pair.remote->state = ICE_CANDIDATE_STATE_SUCCEEDED;

    } else {

      char test[64];
      sprintf(test, "test\n");
      udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, test, strlen(test));
    }

    agent_recv(agent, buf, sizeof(buf));
  }
}

void *agent_thread(void *arg) {
#if 0
  Agent *agent = (Agent *)arg;

  agent_recv(agent);
#endif
  return NULL;
}

