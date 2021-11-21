#ifndef H264_PARSER_H_
#define H264_PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct H264Frame {

  uint8_t *buf;
  size_t size;

} H264Frame;

H264Frame* h264_frame_new(uint8_t *data, size_t size);

void h264_frame_free(H264Frame *h264_frame);

H264Frame* h264_parser_get_next_frame(uint8_t *buf, size_t size, size_t *prev);

#endif // H264_PARSE_H_
