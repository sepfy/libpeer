#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent.h"

int main(int argc, char *argv[]) {

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

  agent_set_remote_description(&agent, remote_description);

  pthread_t thread;

  pthread_create(&thread, NULL, agent_thread, &agent);

  char buf[64];

  memset(buf, 0, sizeof(buf));

  snprintf(buf, sizeof(buf), "hello from %s", argv[1]);

  while(1) {

    agent_select_candidate_pair(&agent);
//    agent_send(&agent, buf, sizeof(buf));
    usleep(1000 * 1000);
  }

  return 0;
}

