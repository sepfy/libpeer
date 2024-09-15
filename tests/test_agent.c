#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "agent.h"
#include "ports.h"

void test_gather_host(Agent* agent) {
  agent_gather_candidate(agent, NULL, NULL, NULL);
}

void test_gather_turn(Agent* agent, char* turnserver, char* username, char* credential) {
  agent_gather_candidate(agent, turnserver, username, credential);
}

void test_gather_stun(Agent* agent, char* stunserver) {
  agent_gather_candidate(agent, stunserver, NULL, NULL);
}

int main(int argc, char* argv[]) {
  Agent agent;

  char stunserver[] = "stun:stun.l.google.com:19302";
  char turnserver[] = "";
  char username[] = "";
  char credential[] = "";
  char description[1024];
  memset(&description, 0, sizeof(description));

  agent_create(&agent);

  test_gather_host(&agent);
  test_gather_stun(&agent, stunserver);
  test_gather_turn(&agent, turnserver, username, credential);
  agent_get_local_description(&agent, description, sizeof(description));

  printf("sdp:\n%s\n", description);

  agent_destroy(&agent);

  return 0;
}
