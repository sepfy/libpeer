#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gst/gst.h>

#include "signal_service.h"
#include "utils.h"
#include "pear.h"

#define MTU 1400

GstElement *gst_element;
char *g_sdp;
static GCond g_cond;
static GMutex g_mutex;
peer_connection_t *g_peer_connection = NULL;

const char PIPE_LINE[] = "v4l2src ! videorate ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink";

static void on_iceconnectionstatechange(iceconnectionstate_t state, void *data) {
  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
    gst_element_set_state(gst_element, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
  g_cond_signal(&g_cond);
}

static void on_transport_ready(void *data) {

  gst_element_set_state(gst_element, GST_STATE_PLAYING);
}

char* on_offer_get_cb(char *offer, void *data) {

  gst_element_set_state(gst_element, GST_STATE_PAUSED);

  g_mutex_lock(&g_mutex);
  peer_connection_destroy(g_peer_connection);
  g_peer_connection = peer_connection_create();
  peer_connection_add_stream(g_peer_connection, "H264");
  peer_connection_set_on_icecandidate(g_peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(g_peer_connection, &on_transport_ready, NULL);
  peer_connection_set_on_iceconnectionstatechange(g_peer_connection, &on_iceconnectionstatechange, NULL);
  peer_connection_create_answer(g_peer_connection);

  g_cond_wait(&g_cond, &g_mutex);
  peer_connection_set_remote_description(g_peer_connection, offer);
  g_mutex_unlock(&g_mutex);

  return g_sdp;
}

static GstFlowReturn new_sample(GstElement *sink, void *data) {

  static uint8_t rtp_packet[MTU] = {0};
  int bytes;

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    memset(rtp_packet, 0, sizeof(rtp_packet));
    memcpy(rtp_packet, info.data, info.size);
    bytes = info.size;

    peer_connection_send_rtp_packet(g_peer_connection, rtp_packet, bytes);

    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }
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
  options_t options = {8080, "0.0.0.0", "root"};
  parse_argv(argc, argv, &options);

  GstElement *pear_sink;

  gst_init(&argc, &argv);

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_sink = gst_bin_get_by_name(GST_BIN(gst_element), "pear-sink");
  g_signal_connect(pear_sink, "new-sample", G_CALLBACK(new_sample), NULL);
  g_object_set(pear_sink, "emit-signals", TRUE, NULL);

  if(signal_service_create(&signal_service, options)) {
    exit(1);
  }
  signal_service_on_offer_get(&signal_service, &on_offer_get_cb, NULL);
  signal_service_dispatch(&signal_service);

  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}
