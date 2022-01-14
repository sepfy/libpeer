#include <stdio.h>
#include <string.h>

#include "h264_parser.h"

H264Frame* h264_frame_new(uint8_t *data, size_t size) {

  H264Frame *h264_frame = (H264Frame*)calloc(1, sizeof(H264Frame));
  if(!h264_frame)
    return h264_frame;

  h264_frame->size = size;
  h264_frame->buf = (uint8_t*)calloc(size, sizeof(uint8_t));
  memcpy(h264_frame->buf, data, h264_frame->size);
  return h264_frame;
}

void h264_frame_free(H264Frame *h264_frame) {

  if(h264_frame) {
    if(h264_frame->buf) {
      free(h264_frame->buf);
      h264_frame->buf = NULL;
    }
    free(h264_frame);
    h264_frame = NULL;
  }
}

H264Frame* h264_parser_get_next_frame(uint8_t *buf, size_t size, size_t *prev) {

  size_t pos = *prev;
  while((pos + 3) < size) {
    pos++;
    //printf("0x%x, 0x%x, 0x%x, 0x%x\n", buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]);
    if(buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x00 && buf[pos + 3] == 0x01) {
      H264Frame *h264_frame = h264_frame_new(buf + *prev, pos - *prev);
      *prev = pos;
      return h264_frame;
    }
  }
  return NULL;
}

