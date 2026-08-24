#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "address.h"
#include "agent.h"
#include "ice.h"
#include "mdns.h"
#include "stun.h"

#define TEST_HOSTNAME "00000000-0000-0000-0000-000000000000.local"

static int failures = 0;

#define CHECK(condition)                                                   \
  do {                                                                     \
    if (!(condition)) {                                                    \
      fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, \
              #condition);                                                 \
      failures++;                                                          \
    }                                                                      \
  } while (0)

static int append_dns_name(uint8_t* packet, int offset, const char* hostname) {
  const char* label = hostname;

  while (*label != '\0') {
    const char* dot = strchr(label, '.');
    int length;
    if (dot == NULL) {
      dot = label + strlen(label);
    }
    length = (int)(dot - label);
    packet[offset++] = (uint8_t)length;
    memcpy(packet + offset, label, length);
    offset += length;
    label = *dot != '\0' ? dot + 1 : dot;
  }
  packet[offset++] = 0;

  return offset;
}

static void test_compressed_additional_mdns_answer(void) {
  uint8_t packet[128] = {0};
  Address addr = {0};
  char address_string[ADDRSTRLEN];
  int offset = 12;

  packet[2] = 0x84;
  packet[5] = 1;
  packet[11] = 1;
  offset = append_dns_name(packet, offset, TEST_HOSTNAME);
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0xc0;
  packet[offset++] = 0x0c;
  packet[offset++] = 0;
  packet[offset++] = 1;
  packet[offset++] = 0x80;
  packet[offset++] = 1;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 0;
  packet[offset++] = 120;
  packet[offset++] = 0;
  packet[offset++] = 4;
  packet[offset++] = 192;
  packet[offset++] = 0;
  packet[offset++] = 2;
  packet[offset++] = 44;

  CHECK(mdns_parse_response(packet, offset, &addr, TEST_HOSTNAME) == 0);
  CHECK(addr.family == AF_INET);
  CHECK(addr_to_string(&addr, address_string, sizeof(address_string)) != 0);
  CHECK(strcmp(address_string, "192.0.2.44") == 0);
}

static void test_malformed_compression_is_rejected(void) {
  uint8_t packet[16] = {0};
  Address addr = {0};

  packet[2] = 0x80;
  packet[5] = 1;
  packet[12] = 0xc0;
  packet[13] = 0x0c;

  CHECK(mdns_parse_response(packet, 14, &addr, TEST_HOSTNAME) == -1);
}

static void test_unresolved_mdns_candidate_is_retained(void) {
  char description[] =
      "a=candidate:1 1 udp 2122260223 " TEST_HOSTNAME
      " 54400 typ host generation 0\r\n";
  char address_string[ADDRSTRLEN];
  IceCandidate candidate;
  char* end = strstr(description, "\r\n");

  CHECK(ice_candidate_from_description(&candidate, description, end) == 0);
  CHECK(candidate.type == ICE_CANDIDATE_TYPE_HOST);
  CHECK(candidate.addr.family == AF_INET);
  CHECK(candidate.addr.port == 54400);
  CHECK(addr_to_string(&candidate.addr, address_string,
                       sizeof(address_string)) != 0);
  CHECK(strcmp(address_string, "0.0.0.0") == 0);
}

static void create_binding_request(StunMessage* request, const char* password,
                                   int use_candidate) {
  uint32_t priority = 0;

  memset(request, 0, sizeof(*request));
  stun_msg_create(request, STUN_CLASS_REQUEST | STUN_METHOD_BINDING);
  stun_msg_write_attr(request, STUN_ATTR_TYPE_USERNAME, 9, "local:rem");
  stun_msg_write_attr(request, STUN_ATTR_TYPE_PRIORITY, sizeof(priority),
                      (char*)&priority);
  if (use_candidate) {
    stun_msg_write_attr(request, STUN_ATTR_TYPE_USE_CANDIDATE, 0, NULL);
  }
  stun_msg_finish(request, STUN_CREDENTIAL_SHORT_TERM, password,
                  strlen(password));
  stun_parse_msg_buf(request);
}

static void reset_nominated_pair(Agent* agent, IceCandidate* remote,
                                 IceCandidatePair* pair) {
  memset(remote, 0, sizeof(*remote));
  addr_set_family(&remote->addr, AF_INET);
  addr_set_port(&remote->addr, 54400);
  remote->type = ICE_CANDIDATE_TYPE_HOST;

  memset(pair, 0, sizeof(*pair));
  pair->remote = remote;
  pair->state = ICE_CANDIDATE_STATE_INPROGRESS;
  agent->nominated_pair = pair;
}

static void test_authenticated_nomination_learns_peer_reflexive_address(void) {
  Agent agent;
  Address source = {0};
  IceCandidate remote;
  IceCandidatePair pair;
  StunMessage request;
  char address_string[ADDRSTRLEN];

  memset(&agent, 0, sizeof(agent));
  CHECK(agent_create(&agent, 0, 0) == 0);
  snprintf(agent.local_upwd, sizeof(agent.local_upwd), "%s", "secret");
  CHECK(addr_from_string("127.0.0.2", &source) != 0);
  addr_set_port(&source, 55000);

  reset_nominated_pair(&agent, &remote, &pair);
  create_binding_request(&request, "wrong-secret", 1);
  CHECK(request.use_candidate == 1);
  agent_process_stun_request(&agent, &request, &source);
  CHECK(pair.state == ICE_CANDIDATE_STATE_INPROGRESS);
  CHECK(remote.type == ICE_CANDIDATE_TYPE_HOST);

  reset_nominated_pair(&agent, &remote, &pair);
  create_binding_request(&request, "secret", 0);
  CHECK(request.use_candidate == 0);
  agent_process_stun_request(&agent, &request, &source);
  CHECK(pair.state == ICE_CANDIDATE_STATE_INPROGRESS);
  CHECK(remote.type == ICE_CANDIDATE_TYPE_HOST);

  reset_nominated_pair(&agent, &remote, &pair);
  create_binding_request(&request, "secret", 1);
  agent_process_stun_request(&agent, &request, &source);
  CHECK(pair.state == ICE_CANDIDATE_STATE_SUCCEEDED);
  CHECK(remote.type == ICE_CANDIDATE_TYPE_PRFLX);
  CHECK(remote.addr.port == 55000);
  CHECK(addr_to_string(&remote.addr, address_string,
                       sizeof(address_string)) != 0);
  CHECK(strcmp(address_string, "127.0.0.2") == 0);

  agent_destroy(&agent);
}

static void test_remote_credentials_are_bounded(void) {
  Agent agent;
  char description[1024];
  char long_ufrag[400];
  char long_upwd[400];
  size_t index;

  for (index = 0; index + 1 < sizeof(long_ufrag); index++) {
    long_ufrag[index] = 'u';
    long_upwd[index] = 'p';
  }
  long_ufrag[sizeof(long_ufrag) - 1] = '\0';
  long_upwd[sizeof(long_upwd) - 1] = '\0';
  snprintf(description, sizeof(description),
           "a=ice-ufrag:%s\r\na=ice-pwd:%s\r\n", long_ufrag, long_upwd);

  memset(&agent, 0, sizeof(agent));
  CHECK(agent_create(&agent, 0, 0) == 0);
  agent_set_remote_description(&agent, description);
  CHECK(strlen(agent.remote_ufrag) == ICE_UFRAG_LENGTH);
  CHECK(strlen(agent.remote_upwd) == ICE_UPWD_LENGTH);
  CHECK(agent.remote_ufrag[ICE_UFRAG_LENGTH] == '\0');
  CHECK(agent.remote_upwd[ICE_UPWD_LENGTH] == '\0');
  agent_destroy(&agent);
}

int main(void) {
  test_compressed_additional_mdns_answer();
  test_malformed_compression_is_rejected();
  test_unresolved_mdns_candidate_is_retained();
  test_authenticated_nomination_learns_peer_reflexive_address();
  test_remote_credentials_are_bounded();

  if (failures != 0) {
    fprintf(stderr, "%d test checks failed\n", failures);
  }
  return failures == 0 ? 0 : 1;
}
