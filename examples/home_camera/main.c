#include <stdio.h>
#include <stdlib.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

const char VIDEO_SINK_PIPELINE[] = "libcamerasrc ! video/x-raw, format=NV12, width=1280, height=960, framerate=30/1, interlace-mode=progressive, colorimetry=bt709 ! v4l2h264enc capture-io-mode=4 output-io-mode=4";

//const char AUDIO_SINK_PIPELINE[] = "audiotestsrc ! alawenc";

const char AUDIO_SINK_PIPELINE[] = "audiotestsrc ! opusenc"; 
//const char AUDIO_SRC_PIPELINE[] = "alawdec ! audioresample ! audioconvert ! alsasink device=plughw:wm8960soundcard,0";

const char AUDIO_SRC_PIPELINE[] = "opusparse ! opusdec ! audioconvert ! audioresample ! queue ! alsasink device=hdmi:vc4hdmi,0";

void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {

    printf("Disconnect with browser.\n");
  }
}

void on_icecandidate(char *answersdp, void *data) {

  signaling_set_answer(answersdp);
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

  peer_connection_add_stream(pc, CODEC_H264, VIDEO_SINK_PIPELINE, NULL);

  peer_connection_add_stream(pc, CODEC_OPUS, AUDIO_SINK_PIPELINE, AUDIO_SRC_PIPELINE);

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

int main(int argc, char **argv) {

  signal(SIGINT, signal_handler);

  signaling_dispatch(8000, index_html, signaling_on_offersdp_get);

  return 0;
}
