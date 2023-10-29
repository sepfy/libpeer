#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <gst/gst.h>

#include "peer.h"

const char CAMERA_PIPELINE[] = "libcamerasrc ! video/x-raw, format=(string)NV12, width=(int)1280, height=(int)960, framerate=(fraction)30/1, interlace-mode=(string)progressive, colorimetry=(string)bt709 ! v4l2h264enc capture-io-mode=4 output-io-mode=4 ! video/x-h264, stream-format=(string)byte-stream, level=(string)4, alighnment=(string)au ! h264parse config-interval=-1 ! appsink name=camera-sink";

const char MIC_PIPELINE[] = "alsasrc latency-time=20000 device=plughw:seeed2micvoicec,0 ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alawenc ! appsink name=mic-sink";

const char SPK_PIPELINE[] = "appsrc name=spk-src format=time ! alawdec ! audio/x-raw,format=S16LE,rate=8000,channels=1 ! alsasink sync=false device=plughw:seeed2micvoicec,0";

int g_interrupted = 0;
PeerConnection *g_pc = NULL;
PeerConnectionState g_state;

typedef struct Media {

  // Camera elements
  GstElement *camera_pipeline;
  GstElement *camera_sink;

  // Microphone elements
  GstElement *mic_pipeline;
  GstElement *mic_sink;

  // Speaker elements
  GstElement *spk_pipeline;
  GstElement *spk_src;

} Media;

Media g_media;

static void onconnectionstatechange(PeerConnectionState state, void *data) {

  printf("state is changed: %d\n", state);
  g_state = state;
  if (g_state == PEER_CONNECTION_COMPLETED) {

    gst_element_set_state(g_media.camera_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(g_media.mic_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(g_media.spk_pipeline, GST_STATE_PLAYING);
  }
}

static GstFlowReturn on_video_data(GstElement *sink, void *data) {

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    peer_connection_send_video(g_pc, info.data, info.size);

    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}

static GstFlowReturn on_audio_data(GstElement *sink, void *data) {

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name(sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);
    peer_connection_send_audio(g_pc, info.data, info.size);
    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);

    return GST_FLOW_OK;
  }

  return GST_FLOW_ERROR;
}


static void onopen(void *user_data) {

}

static void onclose(void *user_data) {

}

static void onmessasge(char *msg, size_t len, void *user_data) {

  printf("on message: %s", msg);

  if (strncmp(msg, "ping", 4) == 0) {
    printf(", send pong\n");
    peer_connection_datachannel_send(g_pc, "pong", 4);
  }
}

static void on_request_keyframe() {

  printf("request keyframe\n");
}

static void signal_handler(int signal) {

  g_interrupted = 1;
}

static void* peer_singaling_task(void *data) {

  while (!g_interrupted) {

    peer_signaling_loop();
    usleep(1000);
  }

  pthread_exit(NULL);
}

static void* peer_connection_task(void *data) {

  while (!g_interrupted) {

    peer_connection_loop(g_pc);
    usleep(1000); 
  }

  pthread_exit(NULL); 
}

void get_hwaddr(char *buf, size_t size) {

  uint8_t *ptr;
  int fd;
  struct ifreq ifr;

  memset(buf, 0, size);
  fd = socket(AF_INET, SOCK_DGRAM, 0);

  if(fd < 0)
    return;

  ifr.ifr_addr.sa_family = AF_INET;

  strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);

  if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
    ptr = (uint8_t*)&ifr.ifr_ifru.ifru_hwaddr.sa_data[0];
    snprintf(buf, size, "%02x%02x%02x%02x%02x%02x", *ptr, *(ptr + 1), *(ptr + 2), *(ptr + 3), *(ptr + 4), *(ptr + 5));

  }

  close(fd);
}


int main(int argc, char *argv[]) {

  char device_id[64];
  pthread_t peer_singaling_thread;
  pthread_t peer_connection_thread;

  signal(SIGINT, signal_handler);

  PeerConfiguration config = {
   .ice_servers = {
    { .urls = "stun:stun.l.google.com:19302" },
   },
   .datachannel = DATA_CHANNEL_STRING,
   .video_codec = CODEC_H264,
   .audio_codec = CODEC_PCMA,
   .on_request_keyframe = on_request_keyframe
  };

  strcpy(device_id, "rpi-");
  get_hwaddr(device_id + 4, sizeof(device_id) - 4);
  printf("open https://sepfy.github.io/webrtc?deviceId=%s\n", device_id);

  gst_init(&argc, &argv);

  g_media.camera_pipeline = gst_parse_launch(CAMERA_PIPELINE, NULL);
  g_media.camera_sink = gst_bin_get_by_name(GST_BIN(g_media.camera_pipeline), "camera-sink");
  g_signal_connect(g_media.camera_sink, "new-sample", G_CALLBACK(on_video_data), NULL);
  g_object_set(g_media.camera_sink, "emit-signals", TRUE, NULL);

  g_media.mic_pipeline = gst_parse_launch(MIC_PIPELINE, NULL);
  g_media.mic_sink = gst_bin_get_by_name(GST_BIN(g_media.mic_pipeline), "mic-sink");
  g_signal_connect(g_media.mic_sink, "new-sample", G_CALLBACK(on_audio_data), NULL);
  g_object_set(g_media.mic_sink, "emit-signals", TRUE, NULL);

  g_media.spk_pipeline = gst_parse_launch(SPK_PIPELINE, NULL);
  g_media.spk_src = gst_bin_get_by_name(GST_BIN(g_media.spk_pipeline), "spk-src");
  g_object_set(g_media.spk_src, "emit-signals", TRUE, NULL);

  peer_init();

  g_pc = peer_connection_create(&config);
  peer_connection_oniceconnectionstatechange(g_pc, onconnectionstatechange);
  peer_connection_ondatachannel(g_pc, onmessasge, onopen, onclose);

  peer_signaling_join_channel(device_id, g_pc);

  pthread_create(&peer_connection_thread, NULL, peer_connection_task, NULL);
  pthread_create(&peer_singaling_thread, NULL, peer_singaling_task, NULL);

  while (!g_interrupted) {

    sleep(1);
  }

  gst_element_set_state(g_media.camera_pipeline, GST_STATE_NULL);
  gst_element_set_state(g_media.mic_pipeline, GST_STATE_NULL);
  gst_element_set_state(g_media.spk_pipeline, GST_STATE_NULL);

  pthread_join(peer_singaling_thread, NULL);
  pthread_join(peer_connection_thread, NULL);

  peer_signaling_leave_channel();
  peer_connection_destroy(g_pc);
  peer_deinit();

  return 0;
}
