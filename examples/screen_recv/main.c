#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "index_html.h"
#include "h264_depacketizer.h"
#include "signaling.h"
#include "peer_connection.h"

typedef struct ScreenRecv {

  GCond cond;
  GMutex mutex;

  PeerConnection *pc;
  Signaling *signaling;
  H264Depacketizer *h264_depacketizer;

} ScreenRecv;

ScreenRecv g_screen_recv = {0};

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    printf("Disconnect with browser.");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_channel(g_screen_recv.signaling, sdp);
  g_cond_signal(&g_screen_recv.cond);
}

static void on_transport_ready(void *data) {

}


void on_track(uint8_t *packet, size_t bytes, void *data) {

  h264_depacketizer_recv(g_screen_recv.h264_depacketizer, packet, bytes);
}

void on_channel_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("Get offer from singaling\n");
    g_mutex_lock(&g_screen_recv.mutex);

    if(g_screen_recv.pc)
      peer_connection_destroy(g_screen_recv.pc);

    g_screen_recv.pc = peer_connection_create();

    MediaStream *media_stream = media_stream_new();
    media_stream_add_track(media_stream, CODEC_H264);

    peer_connection_add_stream(g_screen_recv.pc, media_stream);
    Transceiver transceiver = {.video = RECVONLY};
    peer_connection_add_transceiver(g_screen_recv.pc, transceiver);

    peer_connection_ontrack(g_screen_recv.pc, on_track, NULL);
    peer_connection_onicecandidate(g_screen_recv.pc, on_icecandidate, NULL);
    peer_connection_oniceconnectionstatechange(g_screen_recv.pc, &on_iceconnectionstatechange, NULL);
    peer_connection_set_on_transport_ready(g_screen_recv.pc, &on_transport_ready, NULL);
    peer_connection_create_answer(g_screen_recv.pc);

    g_cond_wait(&g_screen_recv.cond, &g_screen_recv.mutex);
    peer_connection_set_remote_description(g_screen_recv.pc, msg);
    g_mutex_unlock(&g_screen_recv.mutex);
  }

}


void signal_handler(int signal) {

  if(g_screen_recv.h264_depacketizer)
    h264_depacketizer_destroy(g_screen_recv.h264_depacketizer);

  if(g_screen_recv.signaling)
    signaling_destroy(g_screen_recv.signaling);

  if(g_screen_recv.pc)
    peer_connection_destroy(g_screen_recv.pc);

  exit(0);
}

int main(int argc, char *argv[]) {

  signal(SIGINT, signal_handler);

  g_screen_recv.h264_depacketizer = h264_depacketizer_create();

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_screen_recv.signaling = signaling_create(signaling_option);
  if(!g_screen_recv.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_channel_event(g_screen_recv.signaling, &on_channel_event, NULL);
  signaling_dispatch(g_screen_recv.signaling);

  return 0;
}

