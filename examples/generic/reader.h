#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int reader_init();

int reader_get_video_frame(uint8_t *buf, int *size);

int reader_get_audio_frame(uint8_t *buf, int *size);

void reader_deinit();

