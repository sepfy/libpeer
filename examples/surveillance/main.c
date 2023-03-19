#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "index_html.h"
#include "signaling.h"
#include "peer_connection.h"

const char PIPE_LINE[] = "libcamerasrc ! video/x-raw, format=NV12, width=800, height=600, framerate=20/1, interlace-mode=progressive, colorimetry=bt709 ! v4l2h264enc capture-io-mode=4 output-io-mode=4";

void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  printf("state is changed: %d\n", state);
}

void on_icecandidate(char *answer_sdp, void *data) {

  signaling_set_answer(answer_sdp);
}

void on_connected(void *data) {

  printf("on connected\n");
}

void signaling_on_offersdp_get(const char *offersdp, size_t len) {

  static PeerConnection *pc = NULL;

  if(pc) {

    printf("destroy\n");
    peer_connection_destroy(pc);
  }

  pc = peer_connection_create(NULL);

  peer_connection_add_stream(pc, CODEC_H264, PIPE_LINE, NULL);

  peer_connection_onicecandidate(pc, on_icecandidate);

  peer_connection_oniceconnectionstatechange(pc, on_iceconnectionstatechange);

  peer_connection_on_connected(pc, on_connected);

  peer_connection_set_remote_description(pc, offersdp);

  peer_connection_create_answer(pc);
}

void signal_handler(int signal) {

  printf("catch interrupt. exit app\n");
  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  signaling_dispatch(8000, index_html, signaling_on_offersdp_get);

  return 0;
}
