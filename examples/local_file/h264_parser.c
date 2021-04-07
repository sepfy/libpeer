#include <stdio.h>
#include <string.h>

#include "h264_parser.h"

void free_h264_frame(h264_frame_t *h264_frame) {

  if(h264_frame != NULL) {
    if(h264_frame->buf != NULL) {
      free(h264_frame->buf);
      h264_frame->buf = NULL;
    }
    h264_frame = NULL;
  }
}

h264_frame_t* h264_get_next_frame(uint8_t *buf, size_t size) {

  static size_t prev;
  static size_t pos = 0;
  prev = pos;
  pos++;
  while((pos+3) < size) {
    //printf("0x%x, 0x%x, 0x%x, 0x%x\n", buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]);
    if(buf[pos] == 0x00 && buf[pos + 1] == 0x00 && buf[pos + 2] == 0x00 && buf[pos + 3] == 0x01) {
      h264_frame_t *h264_frame = (h264_frame_t*)calloc(1, sizeof(h264_frame_t));
      h264_frame->size = pos - prev;
      h264_frame->buf = (uint8_t*)calloc(size, sizeof(uint8_t));
      memcpy(h264_frame->buf, buf + prev, h264_frame->size);
      return h264_frame;
    }
    pos++;
  }
  return NULL;
}

