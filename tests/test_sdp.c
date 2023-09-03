#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"

void on_agent_state_changed(AgentState state, void *user_data) {

  printf("Agent state changed: %d\n", state);
} 

int main(int argc, char *argv[]) {
#if 0
  Agent agent;

  char remote_description[AGENT_MAX_DESCRIPTION];
  char remote_description_base64[AGENT_MAX_DESCRIPTION];
  char local_description[AGENT_MAX_DESCRIPTION];
  char local_description_base64[AGENT_MAX_DESCRIPTION];

  if (argc < 2) {

    printf("Usage: %s peer_id\n", argv[0]);
    return 0;
  }

  memset(remote_description, 0, sizeof(remote_description));
  memset(remote_description_base64, 0, sizeof(remote_description_base64));

  memset(local_description, 0, sizeof(local_description));
  memset(local_description_base64, 0, sizeof(local_description_base64));

  memset(&agent, 0, sizeof(agent));

  agent_gather_candidates(&agent);

  snprintf(local_description, sizeof(local_description), "m=text 50327 ICE/SDP\nc=IN IP4 0.0.0.0\n");

  agent_get_local_description(&agent, local_description + strlen(local_description), sizeof(local_description));

  base64_encode(local_description, strlen(local_description), local_description_base64, sizeof(local_description_base64));

  printf("Local description: \n%s\n", local_description);

  printf("Local description base64: \n%s\n", local_description_base64);

  printf("Enter remote description base64: \n");

  scanf("%s", remote_description_base64);

  base64_decode(remote_description_base64, strlen(remote_description_base64), remote_description, sizeof(remote_description));

  printf("Remote description: \n%s", remote_description);

  agent.mode = AGENT_MODE_CONTROLLING;
  //agent.mode = AGENT_MODE_CONTROLLED;
  agent_set_remote_description(&agent, remote_description);
#endif
  return 0;
}

