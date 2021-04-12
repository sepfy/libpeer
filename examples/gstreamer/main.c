#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <gst/gst.h>

#include "pear.h"

#define UDP_MAX_SIZE 65535

//const char PIPE_LINE[] = "filesrc location=test.264 ! h264parse ! rtph264pay config-interval=1 ! application/x-rtp,media=video,encoding-name=H264,payload=102 ! udpsink host=127.0.0.1 port=8004";
//const char PIPE_LINE[] = "filesrc location=test.264 ! appsink name=pear-sink";
const char PIPE_LINE[] = "filesrc location=test.264 ! h264parse  config-interval=-1 ! queue ! rtph264pay config-interval=-1 ! application/x-rtp,media=video,encoding-name=H264,payload=102 ! appsink name=pear-sink";

int g_transport_ready = 0;

static void on_icecandidate(char *sdp, void *data) {

  printf("%s\n", g_base64_encode((const char *)sdp, strlen(sdp)));
}

static void on_transport_ready(void *data) {

  g_transport_ready = 1;
}

void *gst_rtp_forwarding_thread(void *data) {

  peer_connection_t *peer_connection = (peer_connection_t*)data;
  int fd;
  struct sockaddr_in server_addr, client_addr;
  int len, n;
  char buf[UDP_MAX_SIZE];

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if(fd < 0) {
    perror("socket creation failed");
    return NULL;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  memset(&client_addr, 0, sizeof(client_addr));

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(8004);

  if(bind(fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind failed");
    return NULL;
  }
 

  while(1) {
    len = sizeof(client_addr);
    n = recvfrom(fd, buf, sizeof(buf), MSG_WAITALL, (struct sockaddr *)&client_addr, &len);
    peer_connection_send_rtp_packet(peer_connection, (uint8_t*)buf, n);
int i = 0;
for(i = 0; i < 16; ++i) {
  printf("%.2X ", (uint8_t)buf[i]);
}
printf("\n");
    usleep(33000);
  }

  return NULL;
}


static GstFlowReturn new_sample(GstElement *sink, void *data) {

  peer_connection_t *peer_connection = (peer_connection_t*)data;

  static uint8_t rtp_packet[1400] = {0};
  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;
  uint8_t *data1;
  /* Retrieve the buffer */
printf("HAHA\n");
  int i = 0;
  g_signal_emit_by_name (sink, "pull-sample", &sample);
  if (sample) {
    /* The only thing we do in this example is print a * to indicate a received buffer */

   buffer = gst_sample_get_buffer(sample);
   gst_buffer_map(buffer, &info, GST_MAP_READ);
   data1 = info.data;
    for(i = 0; i< 16; ++i) {
      printf("%.2X ", data1[i]);
    }
    memset(rtp_packet, 0, sizeof(rtp_packet));
    memcpy(rtp_packet, info.data, info.size);
    int len = info.size;
    printf(", %ld \n", info.size);
    peer_connection_send_rtp_packet(peer_connection, rtp_packet, len);
    gst_sample_unref (sample);
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
#if 0
  g_thread_new("rtp-forwarding", gst_rtp_forwarding_thread, &peer_connection);
#endif
  gst_init(&argc, &argv);

  gst_element = gst_parse_launch(PIPE_LINE, NULL);
  pear_sink = gst_bin_get_by_name(GST_BIN(gst_element), "pear-sink");
  g_signal_connect(pear_sink, "new-sample", G_CALLBACK(new_sample), &peer_connection);
  g_object_set(pear_sink, "emit-signals", TRUE, NULL);

  gst_element_set_state(gst_element, GST_STATE_PLAYING);

  gloop = g_main_loop_new(NULL, FALSE);

  g_main_loop_run(gloop);

  g_thread_join(gthread);

  g_main_loop_unref(gloop);
  gst_element_set_state(gst_element, GST_STATE_NULL);
  gst_object_unref(gst_element);

  return 0;
}
