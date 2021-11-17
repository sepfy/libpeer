#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "index_html.h"
#include "signaling.h"
#include "h264_packetizer.h"
#include "h264_parser.h"

#define FILE_MAX_SIZE 5*1024*1024
const char* FILE_NAME = "test.264";

typedef struct FileSend {

  GCond cond;
  GMutex mutex;

  PeerConnection *pc;
  Signaling *signaling;

  pthread_t thread;
  gboolean sending_data;

} FileSend;

FileSend g_file_send;


void* send_h264_thread(void *data) {

  rtp_encode_context_t *rtp_encode_context;
  rtp_encode_context = create_rtp_encode_context(g_file_send.pc);

  uint8_t buf[FILE_MAX_SIZE] = {0};
  FILE *fp = NULL;

  fp = fopen(FILE_NAME, "rb");
  if(fp == NULL) {
    printf("Cannot open %s\n", FILE_NAME);
    pthread_exit(NULL);
  }

  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);

  static unsigned long timestamp = 0;
  static H264Frame sps_frame;
  static H264Frame pps_frame;
  H264Frame *h264_frame;

  while(g_file_send.sending_data) {

    h264_frame = h264_parser_get_next_frame(buf, sizeof(buf));
    if(h264_frame == NULL) break;

    if(h264_frame->buf[4] == 0x67) {
      sps_frame.size = h264_frame->size;
      sps_frame.buf = (uint8_t*)malloc(h264_frame->size);
      memcpy(sps_frame.buf, h264_frame->buf, h264_frame->size);
    }
    else if(h264_frame->buf[4] == 0x68) {
      pps_frame.size = h264_frame->size;
      pps_frame.buf = (uint8_t*)malloc(h264_frame->size);
      memcpy(pps_frame.buf, h264_frame->buf, h264_frame->size);
    }
    else if(h264_frame->buf[4] == 0x65) {
      int size_tmp = h264_frame->size;
      uint8_t *buf_tmp = NULL;
      buf_tmp = (uint8_t*)malloc(size_tmp);
      memcpy(buf_tmp, h264_frame->buf, size_tmp);

      h264_frame->size = size_tmp + sps_frame.size + pps_frame.size;
      h264_frame->buf = (uint8_t*)realloc(h264_frame->buf, size_tmp + sps_frame.size + pps_frame.size);
      memcpy(h264_frame->buf, sps_frame.buf, sps_frame.size);
      memcpy(h264_frame->buf + sps_frame.size, pps_frame.buf, pps_frame.size);
      memcpy(h264_frame->buf + sps_frame.size + pps_frame.size, buf_tmp, size_tmp);
      free(buf_tmp);
    }
    rtp_encode_frame(rtp_encode_context, h264_frame->buf, h264_frame->size, timestamp);
    timestamp += 33000;

    h264_frame_free(h264_frame);
    usleep(33000);
  }

  printf("End of send video thread\n");

  pthread_exit(NULL);
}


static void on_iceconnectionstatechange(IceConnectionState state, void *data) {

  if(state == FAILED) {
    printf("Disconnect with browser... Stop streaming");
  }
}

static void on_icecandidate(char *sdp, void *data) {

  signaling_send_answer_to_channel(g_file_send.signaling, sdp);
  g_cond_signal(&g_file_send.cond);
}

static void on_transport_ready(void *data) {

  if(g_file_send.sending_data == FALSE) {
    g_file_send.sending_data = TRUE;
    pthread_create(&g_file_send.thread, NULL, send_h264_thread, NULL); 
  }
}

void on_channel_event(SignalingEvent signaling_event, char *msg, void *data) {

  if(signaling_event == SIGNALING_EVENT_GET_OFFER) {

    g_mutex_lock(&g_file_send.mutex);
    g_file_send.sending_data = FALSE;
    pthread_join(g_file_send.thread, NULL);

    if(g_file_send.pc)
      peer_connection_destroy(g_file_send.pc);
    g_file_send.pc = peer_connection_create();

    MediaStream *media_stream = media_stream_new();
    media_stream_add_track(media_stream, CODEC_H264);

    peer_connection_add_stream(g_file_send.pc, media_stream);

    peer_connection_onicecandidate(g_file_send.pc, on_icecandidate, NULL);
    peer_connection_oniceconnectionstatechange(g_file_send.pc, &on_iceconnectionstatechange, NULL);
    peer_connection_set_on_transport_ready(g_file_send.pc, &on_transport_ready, NULL);
    peer_connection_create_answer(g_file_send.pc);

    g_cond_wait(&g_file_send.cond, &g_file_send.mutex);
    peer_connection_set_remote_description(g_file_send.pc, msg);
    g_mutex_unlock(&g_file_send.mutex);
  }
}

int main(int argv, char *argc[]) {

  SignalingOption signaling_option = {SIGNALING_PROTOCOL_HTTP, "0.0.0.0", "demo", 8000, index_html};

  g_file_send.signaling = signaling_create(signaling_option);

  if(!g_file_send.signaling) {
    printf("Create signaling service failed\n");
    return 0;
  }

  signaling_on_channel_event(g_file_send.signaling, &on_channel_event, NULL);
  signaling_dispatch(g_file_send.signaling);

  return 0;
}
