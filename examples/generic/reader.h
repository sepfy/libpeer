#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int reader_init();

uint8_t* reader_get_video_frame(int* size);

uint8_t* reader_get_audio_frame(int* size);

void reader_deinit();
