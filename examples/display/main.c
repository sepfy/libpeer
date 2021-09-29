#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/gst.h>
#include "h264_depacketizer.h"

#include "signal_service.h"
#include "utils.h"
#include "pear.h"

#define MTU 1400

rtp_decode_context_t *rtp_decode_context;
GstElement *gst_element;
char *g_sdp = NULL;
static GCond g_cond;
static GMutex g_mutex;
peer_connection_t *g_peer_connection = NULL;

const char PIPE_LINE[] = "appsrc name=pear-src ! rtph264depay ! h264parse ! nvh264dec ! videoconvert ! autovideosink";

static void on_iceconnectionstatechange(iceconnectionstate_t state, void *data) {
  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
    gst_element_set_state(gst_element, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  if(g_sdp)
    g_free(g_sdp);

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
  g_cond_signal(&g_cond);
}

static void on_transport_ready(void *data) {

  gst_element_set_state(gst_element, GST_STATE_PLAYING);
}

void on_track(uint8_t *packet, size_t bytes) {
#if 0
//printf("%d\n", bytes);
if(bytes > 16) {
  int i = 0;
  for(i = 0; i < 16; i++) {
    printf("%.2X ", (uint8_t)packet[i+12]);
  }
  printf("\n");
}
#endif

rtp_decode_frame(rtp_decode_context, packet, bytes);

}

char* on_offer_get_cb(char *offer, void *data) {

  gst_element_set_state(gst_element, GST_STATE_PAUSED);

  g_mutex_lock(&g_mutex);
  peer_connection_destroy(g_peer_connection);
  g_peer_connection = peer_connection_create();
  peer_connection_add_stream(g_peer_connection, "H264");
  peer_connection_set_on_track(g_peer_connection, on_track, NULL);
  peer_connection_set_on_icecandidate(g_peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(g_peer_connection, &on_transport_ready, NULL);
  peer_connection_set_on_iceconnectionstatechange(g_peer_connection, &on_iceconnectionstatechange, NULL);
  peer_connection_create_answer(g_peer_connection);

  g_cond_wait(&g_cond, &g_mutex);
  peer_connection_set_remote_description(g_peer_connection, offer);
  g_mutex_unlock(&g_mutex);

  return g_sdp;
}

static GstFlowReturn need_data(GstElement *src, guint arg, void *data) {
  //static uint8_t rtp_packet[MTU] = {0};
  //int bytes;

  GstFlowReturn ret;
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  buffer = gst_buffer_new_and_alloc(arg);

  
  g_signal_emit_by_name (src, "push-buffer", buffer, &ret);

  if (ret != GST_FLOW_OK) {
    return FALSE;
  }

  gst_buffer_unref(buffer);

  return GST_FLOW_ERROR;
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

  GstElement *pear_src;

  gst_init(&argc, &argv);


  rtp_decode_context = create_rtp_decode_context();

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_src = gst_bin_get_by_name(GST_BIN(gst_element), "pear-src");
//  g_signal_connect(pear_src, "need-data", G_CALLBACK(need_data), NULL);
//  g_object_set(pear_src, "emit-signals", TRUE, NULL);

  if(signal_service_create(&signal_service, options)) {
    exit(1);
  }
  signal_service_on_offer_get(&signal_service, &on_offer_get_cb, NULL);
  signal_service_dispatch(&signal_service);

  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}
