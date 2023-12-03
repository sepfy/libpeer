#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"
#include "buffer.h"

Buffer* buffer_new(int size) {

  Buffer *rb;
  rb = (Buffer*)calloc(1, sizeof(Buffer));

  rb->data = (uint8_t*)calloc(1, size);
  rb->size = size;
  rb->head = 0;
  rb->tail = 0;

  return rb;
}

void buffer_clear(Buffer *rb) {

  rb->head = 0;
  rb->tail = 0;
}

void buffer_free(Buffer *rb) {

  if (rb) {

    free(rb->data);
    rb->data = NULL;
    rb = NULL;
  }
}

int buffer_push_tail(Buffer *rb, const uint8_t *data, int size) {

  int free_space = (rb->size + rb->head - rb->tail - 1) % rb->size;

  int align_size = ALIGN32(size + 4);

  if (align_size > free_space) {
    LOGE("no enough space");
    return -1;
  }

  int tail_end = (rb->tail + align_size) % rb->size;

  if (tail_end < rb->tail) {

    if (rb->head < align_size) {

      LOGE("no enough space");
      return -1;
    }

    int *p = (int*)(rb->data + rb->tail);
    *p = size;
    memcpy(rb->data, data, size);
    rb->tail = size;

  } else {

    int *p = (int*)(rb->data + rb->tail);
    *p = size;
    memcpy(rb->data + rb->tail + 4, data, size);
    rb->tail = tail_end;
  }

  return size;
}

uint8_t* buffer_peak_head(Buffer *rb, int *size) {

  if (!rb || rb->head == rb->tail) {

    return NULL;
  }

  *size = *((int*)(rb->data + rb->head));

  int align_size = ALIGN32(*size + 4);

  int head_end = (rb->head + align_size) % rb->size;

  if (head_end < rb->head) {

    return rb->data;

  } else {

    return rb->data + (rb->head + 4);
  }
}

void buffer_pop_head(Buffer *rb) {

  if (!rb || rb->head == rb->tail) {
    return;
  }

  int *size = (int*)(rb->data + rb->head);

  int align_size = ALIGN32(*size + 4);

  int head_end = (rb->head + align_size) % rb->size;

  if (head_end < rb->head) {

    rb->head = *size;

  } else {

    rb->head = rb->head + align_size;
  }
}
