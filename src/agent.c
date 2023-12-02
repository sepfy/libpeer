#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/socket.h"

#include "udp.h"
#include "utils.h"
#include "stun.h"
#include "ice.h"
#include "base64.h"
#include "agent.h"
#include "ports.h"

static int agent_create_host_addr(Agent *agent, uint16_t port, Address *host_addr) {

  if (ports_get_host_addr(host_addr)) {
    host_addr->port = port;
    return 0;
  }
  return -1;
}

static int agent_create_bind_addr(Agent *agent, Address *serv_addr) {

  int ret = -1;
  Address bind_addr;
  StunMessage send_msg;
  StunMessage recv_msg;
  UdpSocket udp_socket;
  udp_socket_open(&udp_socket);
  // blocking 5s
  udp_blocking_timeout(&udp_socket, 5000);

  memset(&send_msg, 0, sizeof(send_msg));

  stun_msg_create(&send_msg, STUN_CLASS_REQUEST | STUN_METHOD_BINDING);

  do {

    ret = udp_socket_sendto(&udp_socket, serv_addr, send_msg.buf, send_msg.size);

    if (ret == -1) {
      LOGE("Failed to send STUN Binding Request.");
      break;
    }

    memset(&recv_msg, 0, sizeof(recv_msg));
    ret = udp_socket_recvfrom(&udp_socket, serv_addr, recv_msg.buf, sizeof(recv_msg.buf));

    if (ret == -1) {
      LOGD("Failed to receive STUN Binding Response.");
      break;
    }

    stun_parse_msg_buf(&recv_msg);
    memcpy(&bind_addr, &recv_msg.mapped_addr, sizeof(Address));

  } while(0);

  udp_socket_close(&udp_socket);

  IceCandidate *ice_candidate = agent->local_candidates + agent->local_candidates_count++;
  ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_SRFLX, &bind_addr);
  return ret;
}

static int agent_create_turn_addr(Agent *agent, Address *serv_addr, const char *username, const char *credential) {

  int ret = -1;
  uint32_t attr = ntohl(0x11000000);
  Address turn_addr;
  StunMessage send_msg;
  StunMessage recv_msg;
  UdpSocket udp_socket;
  udp_socket_open(&udp_socket);
  // blocking 5s
  udp_blocking_timeout(&udp_socket, 5000);
  memset(&send_msg, 0, sizeof(send_msg));
  stun_msg_create(&send_msg, STUN_METHOD_ALLOCATE);
  stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REQUESTED_TRANSPORT, sizeof(attr), (char*)&attr); // UDP
  stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_USERNAME, strlen(username), (char*)username);

  do {

    ret = udp_socket_sendto(&udp_socket, serv_addr, send_msg.buf, send_msg.size);

    if (ret == -1) {
      LOGE("Failed to send STUN Binding Request.");
      break;
    }

    memset(&recv_msg, 0, sizeof(recv_msg));
    ret = udp_socket_recvfrom(&udp_socket, serv_addr, recv_msg.buf, sizeof(recv_msg.buf));

    if (ret <= 0) {
      LOGE("Failed to receive STUN Binding Response.");
      break;
    }
    stun_parse_msg_buf(&recv_msg);

    if (recv_msg.stunclass == STUN_CLASS_ERROR && recv_msg.stunmethod == STUN_METHOD_ALLOCATE) {

      memset(&send_msg, 0, sizeof(send_msg));
      stun_msg_create(&send_msg, STUN_METHOD_ALLOCATE);
      stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REQUESTED_TRANSPORT, sizeof(attr), (char*)&attr); // UDP
      stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_USERNAME, strlen(username), (char*)username);
      stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_NONCE, strlen(recv_msg.nonce), recv_msg.nonce);
      stun_msg_write_attr(&send_msg, STUN_ATTR_TYPE_REALM, strlen(recv_msg.realm), recv_msg.realm);
      stun_msg_finish(&send_msg, STUN_CREDENTIAL_LONG_TERM, credential, strlen(credential));
      ret = udp_socket_sendto(&udp_socket, serv_addr, send_msg.buf, send_msg.size);
      memset(&recv_msg, 0, sizeof(recv_msg));
      ret = udp_socket_recvfrom(&udp_socket, serv_addr, recv_msg.buf, sizeof(recv_msg.buf));
      stun_parse_msg_buf(&recv_msg);
    }
    memcpy(&turn_addr, &recv_msg.relayed_addr, sizeof(Address));

  } while(0);

  udp_socket_close(&udp_socket);

  IceCandidate *ice_candidate = agent->local_candidates + agent->local_candidates_count++;
  ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_RELAY, &turn_addr);
  return ret;
}

void agent_init(Agent *agent) {

}

void agent_deinit(Agent *agent) {

  udp_socket_close(&agent->udp_socket);
  memset(agent, 0, sizeof(Agent));
}

void agent_gather_candidate(Agent *agent, const char *urls, const char *username, const char *credential) {

  char *port = NULL;
  char hostname[64];
  Address resolved_addr;
  memset(hostname, 0, sizeof(hostname));

  agent->state = AGENT_STATE_GATHERING_STARTED;

  do {

    if ((port = strstr(urls + 5, ":")) == NULL) {
      break;
    }

    snprintf(hostname, port - urls - 5 + 1, "%s", urls + 5);

    if (!addr_ipv4_validate(hostname, strlen(hostname), &resolved_addr)) {

      ports_resolve_addr(hostname, &resolved_addr);
    }

    resolved_addr.port = atoi(port + 1);

    LOGI("resolved_addr.ipv4: %d.%d.%d.%d",
     resolved_addr.ipv4[0], resolved_addr.ipv4[1], resolved_addr.ipv4[2], resolved_addr.ipv4[3]);
    LOGI("resolved_addr.port: %d", resolved_addr.port);

    if (resolved_addr.port <= 0) {
      break;
    }

    if (strncmp(urls, "stun:", 5) == 0) {

      agent_create_bind_addr(agent, &resolved_addr);

    } else if (strncmp(urls, "turn:", 5) == 0) {

      agent_create_turn_addr(agent, &resolved_addr, username, credential);
    }

  } while (0);

}

void agent_get_local_description(Agent *agent, char *description, int length) {

  int i;
  int ncandidates = 0;
  Address host_addr;
  IceCandidate *ice_candidate = NULL;

  memset(description, 0, length);
  memset(agent->local_ufrag, 0, sizeof(agent->local_ufrag));
  memset(agent->local_upwd, 0, sizeof(agent->local_upwd));

  utils_random_string(agent->local_ufrag, 4);
  utils_random_string(agent->local_upwd, 24);

  snprintf(description, length, "a=ice-ufrag:%s\r\na=ice-pwd:%s\r\n", agent->local_ufrag, agent->local_upwd);
  ncandidates = agent->local_candidates_count;
  // add host candidate
  for (i = 0; i < ncandidates; i++) {
    agent_create_host_addr(agent, agent->local_candidates[i].addr.port, &host_addr);
    ice_candidate = agent->local_candidates + agent->local_candidates_count++;
    ice_candidate_create(ice_candidate, agent->local_candidates_count, ICE_CANDIDATE_TYPE_HOST, &host_addr);
  }

  for (int i = 0; i < agent->local_candidates_count; i++) {
    ice_candidate_to_description(&agent->local_candidates[i], description + strlen(description), length - strlen(description));
  }

  /*if (ncandidates)
    // remove last \r\n
    description[strlen(description)-2] = '\0';
  }*/
}

int agent_send(Agent *agent, const uint8_t *buf, int len) {

  //printf("send to ip: %d.%d.%d.%d:%d\n", agent->remote_candidates[0].addr.ipv4[0], agent->remote_candidates[0].addr.ipv4[1], agent->remote_candidates[0].addr.ipv4[2], agent->remote_candidates[0].addr.ipv4[3], agent->remote_candidates[0].addr.port);

  //udp_socket_sendto(&agent->udp_socket, &agent->remote_candidates[0].addr, buf, len);
  return udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, buf, len);
}

void agent_process_stun_request(Agent *agent, StunMessage *stun_msg) {

  switch (stun_msg->stunmethod) {

    case STUN_METHOD_BINDING:
      if (stun_msg_is_valid(stun_msg->buf, stun_msg->size, agent->local_upwd) == 0) {

        StunHeader *header = (StunHeader *)stun_msg->buf;
        memcpy(agent->transaction_id, header->transaction_id, sizeof(header->transaction_id));
        //LOGD("recv STUN_MSG_TYPE_BINDING_REQUEST is valid");

        StunMessage msg;
        stun_msg_create(&msg, STUN_CLASS_RESPONSE | STUN_METHOD_BINDING);

        header = (StunHeader *)msg.buf;
        memcpy(header->transaction_id, agent->transaction_id, sizeof(header->transaction_id));

        char username[584];

        snprintf(username, sizeof(username), "%s:%s", agent->local_ufrag, agent->remote_ufrag);

        char mapped_address[8];
        stun_set_mapped_address(mapped_address, NULL, &agent->nominated_pair->remote->addr);
        stun_msg_write_attr(&msg, STUN_ATTR_TYPE_MAPPED_ADDRESS, 8, mapped_address);
        stun_set_mapped_address(mapped_address, (uint8_t*)agent->transaction_id, &agent->nominated_pair->remote->addr);
        stun_msg_write_attr(&msg, STUN_ATTR_TYPE_XOR_MAPPED_ADDRESS, 8, mapped_address);
        stun_msg_write_attr(&msg, STUN_ATTR_TYPE_USERNAME, strlen(username), username);
        stun_msg_finish(&msg, STUN_CREDENTIAL_SHORT_TERM, agent->local_upwd, strlen(agent->local_upwd));

        udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, msg.buf, msg.size);
        agent->binding_request_time = utils_get_timestamp();
      }


      break;
    default:
      break;
  }
}

void agent_process_stun_response(Agent *agent, StunMessage *stun_msg) {

  switch (stun_msg->stunmethod) {

    case STUN_METHOD_BINDING:
      LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE");

      if (stun_msg_is_valid(stun_msg->buf, stun_msg->size, agent->remote_upwd) == 0) {

        LOGD("recv STUN_MSG_TYPE_BINDING_RESPONSE is valid");

        agent->nominated_pair->state = ICE_CANDIDATE_STATE_SUCCEEDED;
      }

      break;
    default:
      break;
  }
}

int agent_recv(Agent *agent, uint8_t *buf, int len) {

  int ret = -1;
  StunMessage stun_msg;

  ret = udp_socket_recvfrom(&agent->udp_socket, &agent->nominated_pair->local->addr, buf, len);

  if (ret > 0 && stun_probe(buf, ret) == 0) {

    memcpy(stun_msg.buf, buf, ret);
    stun_msg.size = ret;
    stun_parse_msg_buf(&stun_msg);

    switch (stun_msg.stunclass) {

      case STUN_CLASS_REQUEST:
        agent_process_stun_request(agent, &stun_msg);
        break;
      case STUN_CLASS_RESPONSE:
        agent_process_stun_response(agent, &stun_msg);
        break;
      case STUN_CLASS_ERROR:
        agent->nominated_pair->state = ICE_CANDIDATE_STATE_WAITING;
        break;
      default:
        break;

    }

    ret = 0;
  }

  return ret;
}



void agent_set_remote_description(Agent *agent, char *description) {

/*
a=ice-ufrag:Iexb
a=ice-pwd:IexbSoY7JulyMbjKwISsG9
a=candidate:1 1 UDP 1 36.231.28.50 38143 typ srflx
*/
  int i, j;

  LOGD("Set remote description:\n%s", description);

  char *line_start = description;
  char *line_end = NULL;

  while ((line_end = strstr(line_start, "\r\n")) != NULL) {

    if (strncmp(line_start, "a=ice-ufrag:", strlen("a=ice-ufrag:")) == 0) {

      strncpy(agent->remote_ufrag, line_start + strlen("a=ice-ufrag:"), line_end - line_start - strlen("a=ice-ufrag:"));

    } else if (strncmp(line_start, "a=ice-pwd:", strlen("a=ice-pwd:")) == 0) {

      strncpy(agent->remote_upwd, line_start + strlen("a=ice-pwd:"), line_end - line_start - strlen("a=ice-pwd:"));

    } else if (strncmp(line_start, "a=candidate:", strlen("a=candidate:")) == 0) {

      if (ice_candidate_from_description(&agent->remote_candidates[agent->remote_candidates_count], line_start, line_end) == 0) {
        agent->remote_candidates_count++;
      }
    }

    line_start = line_end + 2;
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
}

int agent_connectivity_check(Agent *agent) {

  uint8_t buf[1400];

  StunMessage msg;
  memset(&msg, 0, sizeof(msg));

  if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_WAITING) {

    agent_recv(agent, buf, sizeof(buf));

    stun_msg_create(&msg, STUN_CLASS_REQUEST | STUN_METHOD_BINDING);
    char username[584];
    memset(username, 0, sizeof(username));
    snprintf(username, sizeof(username), "%s:%s", agent->remote_ufrag, agent->local_ufrag);

    stun_msg_write_attr(&msg, STUN_ATTR_TYPE_USERNAME, strlen(username), username);
    stun_msg_write_attr(&msg, STUN_ATTR_TYPE_PRIORITY, 4, (char *)&agent->nominated_pair->priority);
    uint64_t tie_breaker = utils_get_timestamp();
    stun_msg_write_attr(&msg, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);
    stun_msg_write_attr(&msg, STUN_ATTR_TYPE_ICE_CONTROLLED, 8, (char *)&tie_breaker);
    stun_msg_finish(&msg, STUN_CREDENTIAL_SHORT_TERM, agent->remote_upwd, strlen(agent->remote_upwd));

    LOGD("send binding request to remote ip: %d.%d.%d.%d, port: %d", agent->nominated_pair->remote->addr.ipv4[0], agent->nominated_pair->remote->addr.ipv4[1], agent->nominated_pair->remote->addr.ipv4[2], agent->nominated_pair->remote->addr.ipv4[3], agent->nominated_pair->remote->addr.port);

   udp_socket_sendto(&agent->udp_socket, &agent->nominated_pair->remote->addr, msg.buf, msg.size);

   agent->nominated_pair->state = ICE_CANDIDATE_STATE_INPROGRESS;

  } else if (agent->nominated_pair->state == ICE_CANDIDATE_STATE_INPROGRESS) {

    agent_recv(agent, buf, sizeof(buf));
  }

  return agent->nominated_pair->state == ICE_CANDIDATE_STATE_SUCCEEDED;
}

void agent_select_candidate_pair(Agent *agent) {

  int i;

  time_t t2 = time(NULL);

  //LOGD("Mode: %d", agent->mode);

  for (i = 0; i < agent->candidate_pairs_num; i++) {

    if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_FROZEN) {

      // nominate this pair
      agent->nominated_pair = &agent->candidate_pairs[i];
      agent->nominated_pair->state = ICE_CANDIDATE_STATE_WAITING;
      agent->candidate_pairs[i].nominated_time = time(NULL);
      udp_socket_close(&agent->udp_socket);
      udp_socket_open(&agent->udp_socket);
      udp_socket_bind(&agent->udp_socket, &agent->candidate_pairs[i].local->addr);
      break;

    } else if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_INPROGRESS
     && (t2 - agent->candidate_pairs[i].nominated_time) > 2) {

      LOGD("timeout for nominate (pair %d)", i);
      agent->nominated_pair->state = ICE_CANDIDATE_STATE_FAILED;

    } else if (agent->candidate_pairs[i].state == ICE_CANDIDATE_STATE_FAILED) {

    } else if (agent->candidate_pairs[i].state <= ICE_CANDIDATE_STATE_SUCCEEDED) {
      // still in progress. wait for it
      agent->nominated_pair = &agent->candidate_pairs[i];
      break;
    }

  }

  //LOGD("nominated_pair: %p", agent->nominated_pair);
}
