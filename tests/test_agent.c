#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ports.h"
#include "agent.h"

void test_turn(Agent *agent, char *turnserver, char *username, char *credential) {

  char description[1024];

  agent_gather_candidate(agent, turnserver, username, credential);

  memset(&description, 0, sizeof(description));

  ice_candidate_to_description(&agent->local_candidates[1], description, sizeof(description));

  printf("turn server: %s. ice candidate: %s", turnserver, description);
}

void test_stun(Agent *agent, char *stunserver) {

  char description[1024];

  agent_gather_candidate(agent, stunserver, NULL, NULL);

  memset(&description, 0, sizeof(description));

  ice_candidate_to_description(&agent->local_candidates[0], description, sizeof(description));

  printf("stun server: %s. ice candidate: %s", stunserver, description);
}

void test_local_description(Agent *agent) {

  char description[1024];
  agent_get_local_description(agent, description, sizeof(description));
  printf("local description: %s", description);
}

int main(int argc, char *argv[]) {

  Agent agent;

  char stunserver[] = "stun:stun.l.google.com:19302";
  char turnserver[] = "turn:global.turn.twilio.com:3478?transport=udp";
  char username[] = "8e9ed7b70d63b66684c74932c8bce26363956e338d353a4e0e039fe6140f64c2";
  char credential[] = "hXQwBJFQX8YHPH6adGgb4XCmKnJQPvhU/VaWwypf/LI=";

  agent_init(&agent);

  test_stun(&agent, stunserver);

  test_turn(&agent, turnserver, username, credential);

  test_local_description(&agent);

  agent_deinit(&agent);

  return 0;
}
