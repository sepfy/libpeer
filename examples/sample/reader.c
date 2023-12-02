#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

static int g_video_size = 0;
static int g_audio_size = 0;
static uint8_t *g_video_buf = NULL;
static uint8_t *g_audio_buf = NULL;

int reader_init() {

  FILE *video_fp = NULL;
  FILE *audio_fp = NULL;
  char videofile[] = "test.264";
  char audiofile[] = "alaw08m.wav";

  video_fp = fopen(videofile, "rb");

  if (video_fp == NULL) {

    printf("open file %s failed\n", videofile);
    return -1;
  }

  fseek(video_fp, 0, SEEK_END);
  g_video_size = ftell(video_fp);
  fseek(video_fp, 0, SEEK_SET);
  g_video_buf = (uint8_t *)calloc(1, g_video_size);
  fread(g_video_buf, g_video_size, 1, video_fp);
  fclose(video_fp);

  audio_fp = fopen(audiofile, "rb");

  if (audio_fp == NULL) {

    printf("open file %s failed\n", audiofile);
    return -1;
  }

  fseek(audio_fp, 0, SEEK_END);
  g_audio_size = ftell(audio_fp);
  fseek(audio_fp, 0, SEEK_SET);
  g_audio_buf = (uint8_t *)calloc(1, g_audio_size);
  fread(g_audio_buf, 1, g_audio_size, audio_fp);
  fclose(audio_fp);

  return 0;
}

uint8_t* reader_h264_find_nalu(uint8_t *buf_start, uint8_t *buf_end) {

  static const uint32_t nalu_start_code = 0x01000000;
  uint8_t *p = buf_start;

  while((p + 4) < buf_end) {

    if (memcmp(p, &nalu_start_code, 4) == 0) {

      return p;
    }

    p++;
  }

  return buf_end;
}

int reader_get_video_frame(uint8_t *buf, int *size) {

  int ret = -1;
  static uint8_t pps_frame[128];
  static uint8_t sps_frame[128];
  static int pps_size = 0;
  static int sps_size = 0;
  uint8_t *buf_end = g_video_buf + g_video_size;

  static uint8_t *pstart = NULL;
  static uint8_t *pend = NULL;
  size_t nalu_size;

  if (!pstart) pstart = g_video_buf;

  pend = reader_h264_find_nalu(pstart + 1, buf_end);

  if (pend == buf_end) {

    pstart = NULL;
    return -1;
  }

  nalu_size = pend - pstart;

  if ((pstart[4] & 0x1f) == 0x07) {

    sps_size = nalu_size;
    memcpy(sps_frame, pstart, nalu_size);

  } else if ((pstart[4] & 0x1f) == 0x08) {

    pps_size = nalu_size;
    memcpy(pps_frame, pstart, nalu_size);

  } else if ((pstart[4] & 0x1f) == 0x05) {

    *size = sps_size + pps_size + nalu_size;
    memcpy(buf, sps_frame, sps_size);
    memcpy(buf + sps_size, pps_frame, pps_size);
    memcpy(buf + sps_size + pps_size, pstart, nalu_size);
    ret = 0;

  } else {

    *size = nalu_size;
    memcpy(buf, pstart, nalu_size);
    ret = 0;
  }

  pstart = pend;

  return ret;
}

int reader_get_audio_frame(uint8_t *buf, int *size) {

  // sample-rate=8000 channels=1 format=S16LE duration=20ms alaw-size=160
  static int pos = 0;
  *size = 160;
  if ((pos + *size) > g_audio_size) {
    pos = 0;
  }

  memcpy(buf, g_audio_buf + pos, *size);
  pos += *size;

  return 0;
}

void reader_deinit() {

  if (g_video_buf != NULL) {
    free(g_video_buf);
    g_video_buf = NULL;
  }

  if (g_audio_buf != NULL) {
    free(g_audio_buf);
    g_audio_buf = NULL;
  }
}
