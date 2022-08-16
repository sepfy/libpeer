#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gst/gst.h>

#include "index_html.h"
#include "rtp_depacketizer.h"
#include "peer_connection.h"
#include "signaling.h"

#define MTU 1400

const char VIDEO_PIPELINE[] = "v4l2src device=/dev/video0 ! videorate ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! queue ! x264enc bitrate=6000 speed-preset=ultrafast tune=zerolatency key-int-max=15 ! video/x-h264,profile=constrained-baseline ! queue ! h264parse ! queue ! rtph264pay config-interval=-1 pt=102 seqnum-offset=0 timestamp-offset=0 mtu=1400 ! appsink name=video-app-sink";

const char AUDIO_PIPELINE[] = "alsasrc ! audioconvert ! audioresample ! alawenc ! rtppcmapay pt=8 ssrc=12 ! appsink name=audio-app-sink";

typedef struct GstRtpPacket {

  uint8_t data[MTU];
  size_t size;
 
} GstRtpPacket;

typedef struct Meeting {

  GstElement *audio_element;
  GstElement *video_element;

  GstElement *audio_sink;
  GstElement *video_sink;

  GAsyncQueue *async_queue;

  GCond cond;
  GMutex mutex;
  pthread_t thread;

  Signaling *signaling;
  PeerConnection *pc;

  RtpDepacketizer *rtp_depacketizer_audio;
  RtpDepacketizer *rtp_depacketizer_video;

  gboolean transporting;

} Meeting;

Meeting g_meeting;

static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    LOG_INFO("Disconnect with browser... Stop streaming");
    gst_element_set_state(g_meeting.audio_element, GST_STATE_PAUSED);
    gst_element_set_state(g_meeting.video_element, GST_STATE_PAUSED);
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_call(g_meeting.signaling, sdp);
  g_cond_signal(&g_meeting.cond);
}

static GstFlowReturn new_sample(GstElement *sink, void *data) {

  GstSample *sample;
  GstBuffer *buffer;
  GstMapInfo info;

  g_signal_emit_by_name (sink, "pull-sample", &sample);

  if(sample) {

    buffer = gst_sample_get_buffer(sample);
    gst_buffer_map(buffer, &info, GST_MAP_READ);

    GstRtpPacket *gst_rtp_packet = g_new(GstRtpPacket, 1);
    gst_rtp_packet->size = info.size;

    memset(gst_rtp_packet->data, 0, MTU);
    memcpy(gst_rtp_packet->data, info.data, info.size);

    g_async_queue_lock(g_meeting.async_queue);
    g_async_queue_push_unlocked(g_meeting.async_queue, gst_rtp_packet); 
    g_async_queue_unlock(g_meeting.async_queue);

    gst_buffer_unmap(buffer, &info);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
  }
  return GST_FLOW_ERROR;
}

void* srtp_transport_thread(void *data) {

  while(g_meeting.transporting) {

    g_async_queue_lock(g_meeting.async_queue);
    GstRtpPacket *gst_rtp_packet = g_async_queue_pop_unlocked(g_meeting.async_queue);
    g_async_queue_unlock(g_meeting.async_queue);

    peer_connection_send_rtp_packet(g_meeting.pc, gst_rtp_packet->data, gst_rtp_packet->size);
    if(gst_rtp_packet) {
      g_free(gst_rtp_packet);
      gst_rtp_packet = NULL;
    }
  }

  pthread_exit(NULL);
}

static void on_connected(void *data) {

  gst_element_set_state(g_meeting.audio_element, GST_STATE_PLAYING);
  gst_element_set_state(g_meeting.video_element, GST_STATE_PLAYING);
}

void on_track(uint8_t *packet, size_t bytes, void *data) {

  RtpDepacketizer *rtp_depacketizer = (RtpDepacketizer*)data;

  uint32_t ssrc = (packet[8] << 24) | (packet[9] << 16) | (packet[10] << 8) | (packet[11]);

  if(ssrc == peer_connection_get_ssrc(g_meeting.pc, "audio")) {
    rtp_depacketizer_recv(g_meeting.rtp_depacketizer_audio, packet, bytes); 
  }
  else if(ssrc == peer_connection_get_ssrc(g_meeting.pc, "video")) {

    static int video_count = 0;
    video_count++;
    if(video_count % 100 == 0) {
      uint32_t rtcp_ssrc;
      memcpy(&rtcp_ssrc, packet + 8, 4);
      peer_connection_send_rtcp_pil(g_meeting.pc, rtcp_ssrc);
    }
    rtp_depacketizer_recv(g_meeting.rtp_depacketizer_video, packet, bytes); 
  }

}

void on_call_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    printf("Get offer from singaling\n");
    g_mutex_lock(&g_meeting.mutex);

    gst_element_set_state(g_meeting.audio_element, GST_STATE_PAUSED);
    gst_element_set_state(g_meeting.video_element, GST_STATE_PAUSED);

    if(g_meeting.pc)
      peer_connection_destroy(g_meeting.pc);

    g_meeting.pc = peer_connection_create(NULL);

    MediaStream *media_stream = media_stream_new();
    media_stream_add_track(media_stream, CODEC_H264);
    media_stream_add_track(media_stream, CODEC_PCMA);
    peer_connection_add_stream(g_meeting.pc, media_stream);

    Transceiver transceiver = {.video = SENDRECV, .audio = SENDRECV};
    peer_connection_add_transceiver(g_meeting.pc, transceiver);

    peer_connection_onicecandidate(g_meeting.pc, on_icecandidate, NULL);
    peer_connection_ontrack(g_meeting.pc, on_track, NULL);
    peer_connection_oniceconnectionstatechange(g_meeting.pc, &on_iceconnectionstatechange, NULL);
    peer_connection_on_connected(g_meeting.pc, &on_connected, NULL);
    peer_connection_create_answer(g_meeting.pc);

    g_cond_wait(&g_meeting.cond, &g_meeting.mutex);
    peer_connection_set_remote_description(g_meeting.pc, msg);
    g_mutex_unlock(&g_meeting.mutex);
  }
}


void signal_handler(int signal) {

  g_meeting.transporting = FALSE;
  pthread_join(g_meeting.thread, NULL);

  gst_element_set_state(g_meeting.audio_element, GST_STATE_NULL);
  gst_element_set_state(g_meeting.video_element, GST_STATE_NULL);
  gst_object_unref(g_meeting.audio_sink);
  gst_object_unref(g_meeting.video_sink);
  gst_object_unref(g_meeting.audio_element);
  gst_object_unref(g_meeting.video_element);

  if(g_meeting.signaling)
    signaling_destroy(g_meeting.signaling);

  if(g_meeting.pc)
    peer_connection_destroy(g_meeting.pc);


  exit(0);
}


int main(int argc, char **argv) {

  signal(SIGINT, signal_handler);
  gst_init(&argc, &argv);

  g_meeting.audio_element = gst_parse_launch(AUDIO_PIPELINE, NULL);
  g_meeting.video_element = gst_parse_launch(VIDEO_PIPELINE, NULL);

  g_meeting.audio_sink = gst_bin_get_by_name(GST_BIN(g_meeting.audio_element), "audio-app-sink");
  g_meeting.video_sink = gst_bin_get_by_name(GST_BIN(g_meeting.video_element), "video-app-sink");

  g_signal_connect(g_meeting.audio_sink, "new-sample", G_CALLBACK(new_sample), NULL);
  g_signal_connect(g_meeting.video_sink, "new-sample", G_CALLBACK(new_sample), NULL);

  g_object_set(g_meeting.audio_sink, "emit-signals", TRUE, NULL);
  g_object_set(g_meeting.video_sink, "emit-signals", TRUE, NULL);

  g_meeting.async_queue = g_async_queue_new();

  g_meeting.transporting = TRUE;
  pthread_create(&g_meeting.thread, NULL, srtp_transport_thread, NULL);

  g_meeting.rtp_depacketizer_audio = rtp_depacketizer_create("PCMA");
  g_meeting.rtp_depacketizer_video = rtp_depacketizer_create("H264");

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_meeting.signaling = signaling_create(signaling_option);
  if(!g_meeting.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_call_event(g_meeting.signaling, &on_call_event, NULL);
  signaling_dispatch(g_meeting.signaling);

  return 0;
}
