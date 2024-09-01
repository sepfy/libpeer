#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Buffer {
  uint8_t* data;
  int size;
  int head;
  int tail;

} Buffer;

Buffer* buffer_new(int size);

void buffer_free(Buffer* rb);

uint8_t* buffer_peak_head(Buffer* rb, int* size);

int buffer_push_tail(Buffer* rb, const uint8_t* data, int size);

void buffer_pop_head(Buffer* rb);

void buffer_clear(Buffer* rb);

#endif  // BUFFER_H_
