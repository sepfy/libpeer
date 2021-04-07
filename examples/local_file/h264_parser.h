#ifndef H264_FRAME_PARSE_H_
#define H264_FRAME_PARSE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct h264_frame_t {
  uint8_t *buf;
  size_t size;
} h264_frame_t;

void free_h264_frame(h264_frame_t *h264_frame);

h264_frame_t* h264_get_next_frame(uint8_t *buf, size_t size);

#endif // H264_FRAME_PARSE_H_
