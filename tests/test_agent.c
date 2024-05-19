#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ports.h"
#include "agent.h"

void test_turn(Agent *agent, char *turnserver, char *username, char *credential) {

  char description[1024];
  memset(&description, 0, sizeof(description));
  agent_gather_candidate(agent, turnserver, username, credential);
  agent_get_local_description(agent, description, sizeof(description));
  printf("turn server: %s\n", turnserver);
  printf("sdp: %s\n", description);
}

void test_stun(Agent *agent, char *stunserver) {

  char description[1024];
  memset(&description, 0, sizeof(description));
  agent_gather_candidate(agent, stunserver, NULL, NULL);
  agent_get_local_description(agent, description, sizeof(description));
  printf("stun server: %s\n", stunserver);
  printf("sdp: %s\n", description);
}

int main(int argc, char *argv[]) {

  Agent agent;

  char stunserver[] = "stun:stun.l.google.com:19302";
  char turnserver[] = "turn:global.turn.twilio.com:3478?transport=udp";
  char username[] = "8e9ed7b70d63b66684c74932c8bce26363956e338d353a4e0e039fe6140f64c2";
  char credential[] = "hXQwBJFQX8YHPH6adGgb4XCmKnJQPvhU/VaWwypf/LI=";

  agent_init(&agent);

  test_stun(&agent, stunserver);
#if 0
  test_turn(&agent, turnserver, username, credential);
#endif
  agent_deinit(&agent);

  return 0;
}

