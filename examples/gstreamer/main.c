#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <gst/gst.h>

#include "pear.h"

#define MTU 1400

const char PIPE_LINE[] = "v4l2src ! videorate ! video/x-raw,width=640,height=360,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=pear-sink";

int g_transport_ready = 0;

static void on_icecandidate(char *sdp, void *data) {

  printf("%s\n", g_base64_encode((const char *)sdp, strlen(sdp)));
}

static void on_transport_ready(void *data) {

  g_transport_ready = 1;
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

int main (int argc, char *argv[]) {

  GstElement *gst_element;
  GMainLoop *gloop;
  GThread *gthread;
  GstElement *pear_sink;
  char remote_sdp[10240] = {0};

  peer_connection_t peer_connection;
  peer_connection_init(&peer_connection);

  peer_connection_set_on_icecandidate(&peer_connection, on_icecandidate, NULL);
  peer_connection_set_on_transport_ready(&peer_connection, &on_transport_ready, NULL);

  peer_connection_create_answer(&peer_connection);

  FILE *fp = fopen("remote_sdp.txt", "r");
  if(fp != NULL ) {
    fread(remote_sdp, sizeof(remote_sdp), 1, fp);
    peer_connection_set_remote_description(&peer_connection, remote_sdp);
    fclose(fp);
  }
  else {
    printf("Cannot open sdp\n");
    return 1;
  }

  while(!g_transport_ready) {
    sleep(1);
  }

  gst_init(&argc, &argv);

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_sink = gst_bin_get_by_name(GST_BIN(gst_element), "pear-sink");
  g_signal_connect(pear_sink, "new-sample", G_CALLBACK(new_sample), &peer_connection);
  g_object_set(pear_sink, "emit-signals", TRUE, NULL);

  gst_element_set_state(gst_element, GST_STATE_PLAYING);

  gloop = g_main_loop_new(NULL, FALSE);

  g_main_loop_run(gloop);

  g_main_loop_unref(gloop);
  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}
