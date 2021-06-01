#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "h264_packetizer.h"
#include "h264_parser.h"

const char* FILE_NAME = "test.264";

int g_transport_ready = 0;

static void on_icecandidate(char *sdp, void *data) {

  printf("%s\n", g_base64_encode((const char *)sdp, strlen(sdp)));
}

static void on_transport_ready(void *data) {

  g_transport_ready = 1;
}

void* send_video_thread(void *data) {

  peer_connection_t *peer_connection = (peer_connection_t*)data;
  static h264_frame_t sps_frame;
  static h264_frame_t pps_frame;
  int ret = 0;
  FILE *fp = NULL;

  rtp_encode_context_t *rtp_encode_context;
  rtp_encode_context = create_rtp_encode_context(peer_connection);
  struct h264_frame_t *h264_frame;

  fp = fopen(FILE_NAME, "rb");
  if(fp == NULL) {
    printf("Cannot open %s\n", FILE_NAME);
    pthread_exit(NULL);
  }

  uint8_t buf[1024*1024*5] = {0};
  fread(buf, 1, sizeof(buf), fp);
  fclose(fp);
  static unsigned long timestamp = 0;
  while(1) {
    h264_frame = h264_get_next_frame(buf, sizeof(buf));
    if(h264_frame == NULL) break;

   if(h264_frame->buf[4] == 0x67) {
      sps_frame.size = h264_frame->size;
      sps_frame.buf = (uint8_t*)malloc(h264_frame->size);
      memcpy(sps_frame.buf, h264_frame->buf, h264_frame->size);
      continue;
    }
    else if(h264_frame->buf[4] == 0x68) {
      pps_frame.size = h264_frame->size;
      pps_frame.buf = (uint8_t*)malloc(h264_frame->size);
      memcpy(pps_frame.buf, h264_frame->buf, h264_frame->size);
      continue;
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

    free_h264_frame(h264_frame);
    usleep(33000);
  }
  printf("End of send video thread\n");
  pthread_exit(NULL);
}

int main(int argv, char *argc[]) {

  char remote_sdp[10240] = {0};

  pthread_t send_video_thread_id = -1;

  peer_connection_t peer_connection;
  peer_connection_init(&peer_connection);

  peer_connection_add_stream(&peer_connection, "H264");
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

  pthread_create(&send_video_thread_id, NULL, send_video_thread, &peer_connection);
  pthread_join(send_video_thread_id, NULL);

  return 0;
}
