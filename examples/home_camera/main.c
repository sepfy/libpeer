#include <stdio.h>
#include <stdlib.h>

#include "index_html.h"
#include "peer_connection.h"
#include "signaling.h"

const char VIDEO_SINK_PIPELINE[] = "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15";

const char AUDIO_SINK_PIPELINE[] = "alsasrc latency-time=20000 device=plughw:U0x46d0x81b,0 ! audio/x-raw,channels=1,rate=8000 ! identity name=aec-cap ! alawenc";

const char AUDIO_SRC_PIPELINE[] = "alawdec ! audio/x-raw,channels=1,rate=8000 ! queue ! identity name=aec-far ! audioresample ! audioconvert ! audio/x-raw,format=S16LE,channels=2,rate=48000 ! alsasink device=hdmi:vc4hdmi,0";

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

void signaling_on_offersdp_get(char *offersdp, size_t len) {

  static PeerConnection *pc = NULL;

  if(pc) {

    printf("destroy\n");
    peer_connection_destroy(pc);
  }

  pc = peer_connection_create(NULL);

  peer_connection_add_stream(pc, CODEC_H264, VIDEO_SINK_PIPELINE, NULL);

  peer_connection_add_stream(pc, CODEC_PCMA, AUDIO_SINK_PIPELINE, AUDIO_SRC_PIPELINE);

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
