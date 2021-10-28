#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "h264_depacketizer.h"

#include "signal_service.h"
#include "utils.h"
#include "pear.h"

#define MTU 1400

rtp_decode_context_t *rtp_decode_context;
char *g_sdp = NULL;
static GCond g_cond;
static GMutex g_mutex;
PeerConnection *g_peer_connection = NULL;

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {
  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  if(g_sdp)
    g_free(g_sdp);

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
  g_cond_signal(&g_cond);
}

static void on_transport_ready(void *data) {

}


void on_track(uint8_t *packet, size_t bytes, void *data) {

  rtp_decode_frame(rtp_decode_context, packet, bytes);
}

char* on_offer_get_cb(char *offer, void *data) {

  g_mutex_lock(&g_mutex);
  peer_connection_destroy(g_peer_connection);
  g_peer_connection = peer_connection_create();

  MediaStream *media_stream = media_stream_new();
  media_stream_add_track(media_stream, CODEC_H264);

  peer_connection_add_stream(g_peer_connection, media_stream);

  transceiver_t transceiver = {.video = RECVONLY};
  peer_connection_add_transceiver(g_peer_connection, transceiver);
  peer_connection_ontrack(g_peer_connection, on_track, NULL);
  peer_connection_onicecandidate(g_peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(g_peer_connection, &on_transport_ready, NULL);
  peer_connection_oniceconnectionstatechange(g_peer_connection, &on_iceconnectionstatechange, NULL);
  peer_connection_create_answer(g_peer_connection);

  g_cond_wait(&g_cond, &g_mutex);
  peer_connection_set_remote_description(g_peer_connection, offer);
  g_mutex_unlock(&g_mutex);

  return g_sdp;
}

static void print_usage(const char *prog) {

  printf("Usage: %s \n"
   " -p      - port (default: 8080)\n"
   " -H      - address to bind (default: 0.0.0.0)\n"
   " -r      - document root\n"
   " -h      - print help\n", prog);

}

void parse_argv(int argc, char **argv, options_t *options) {

  int opt;

  while((opt = getopt(argc, argv, "p:H:r:h")) != -1) {
    switch(opt) {
      case 'p':
        options->port = atoi(optarg);
        break;
      case 'H':
        options->host = optarg;
        break;
      case 'r':
        options->root = optarg;
        break;
      case 'h':
        print_usage(argv[0]);
        exit(1);
        break;
      default :
        printf("Unknown option %c\n", opt);
        break;
    }
  }

}

int main(int argc, char **argv) {

  signal_service_t signal_service;
  options_t options = {8000, "0.0.0.0", "root"};
  parse_argv(argc, argv, &options);

  rtp_decode_context = create_rtp_decode_context();

  if(signal_service_create(&signal_service, options)) {
    exit(1);
  }
  signal_service_on_offer_get(&signal_service, &on_offer_get_cb, NULL);
  signal_service_dispatch(&signal_service);

  return 0;
}
