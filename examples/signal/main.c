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

const char PIPE_LINE[] = "videotestsrc is-live=true pattern=ball ! videorate ! video/x-raw,width=640,height=360,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink";

int g_transport_ready = 0;

static void on_icecandidate(char *sdp, void *data) {

  g_sdp = g_base64_encode((const char *)sdp, strlen(sdp));
}

static void on_transport_ready(void *data) {

  gst_element_set_state(gst_element, GST_STATE_PLAYING);
}


static GstFlowReturn new_sample(GstElement *sink, void *data) {

  static uint8_t rtp_packet[MTU] = {0};
  int bytes;
  peer_connection_t *peer_connection = (peer_connection_t*)data;

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

    peer_connection_send_rtp_packet(peer_connection, rtp_packet, bytes);

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

char* get_offer_cb(char *offer, void *data) {

  peer_connection_t *peer_connection = (peer_connection_t*)data;
  peer_connection_set_remote_description(peer_connection, offer);
  return g_sdp;
}

int main(int argc, char **argv) {

  signal_service_t signal_service;
  options_t options = {8080, "0.0.0.0", "root"};
  parse_argv(argc, argv, &options);

  GThread *gthread;
  GstElement *pear_sink;

  peer_connection_t peer_connection;
  peer_connection_init(&peer_connection);

  peer_connection_set_on_icecandidate(&peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(&peer_connection, &on_transport_ready, NULL);
  peer_connection_create_answer(&peer_connection);

  gst_init(&argc, &argv);

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_sink = gst_bin_get_by_name(GST_BIN(gst_element), "pear-sink");
  g_signal_connect(pear_sink, "new-sample", G_CALLBACK(new_sample), &peer_connection);
  g_object_set(pear_sink, "emit-signals", TRUE, NULL);

  signal_service_create(&signal_service, options);
  signal_service_on_offer_get(&signal_service, &get_offer_cb, &peer_connection);
  signal_service_dispatch(&signal_service);

  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}
