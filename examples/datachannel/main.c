#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "peer_connection.h"
#include "signaling.h"

static PeerConnection g_pc;

void on_iceconnectionstatechange(IceCandidateState state, void *data) {

  printf("state is changed: %d\n", state);
}

void on_icecandidate(char *answersdp, void *data) {

  printf("on ice candidate\n");
}

void on_connected(void *data) {

  printf("on connected\n");
}

void on_signaling_event(SignalingEvent event, const char *buf, size_t len, void *user_data) {

  const char *local_description = NULL;

  PeerOptions options = {0,};

  switch (event) {

    case SIGNALING_EVENT_REQUEST_OFFER:

      options.b_datachannel = 1;
      peer_connection_configure(&g_pc, &options);
      peer_connection_init(&g_pc);
      peer_connection_on_connected(&g_pc, on_connected);
      peer_connection_onicecandidate(&g_pc, on_icecandidate);
      peer_connection_oniceconnectionstatechange(&g_pc, on_iceconnectionstatechange);

      local_description = peer_connection_create_offer(&g_pc);
printf("local_description: %s\n", local_description);
      signaling_set_local_description(local_description);

      break;

    case SIGNALING_EVENT_RESPONSE_ANSWER:

      peer_connection_set_remote_description(&g_pc, buf);

      break;

    default:
      break;
  }

}

void signal_handler(int signal) {

  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  char device_id[128] = {0,};
  
  snprintf(device_id, sizeof(device_id), "test_666");//%d", getpid());

  printf("open http://127.0.0.1?deviceId=%s\n", device_id);

  signal(SIGINT, signal_handler);

  signaling_dispatch(device_id, on_signaling_event, NULL);

  return 0;
}
