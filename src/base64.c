#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(const unsigned char *input, int input_len, char *output, int output_len) {

  int i, j;
  unsigned char buf[3];
  int buf_len;
  int output_index = 0;

  for (i = 0; i < input_len; i += 3) {
    buf_len = 0;
    for (j = 0; j < 3; j++) {
      if (i + j < input_len) {
        buf[j] = input[i + j];
        buf_len++;
      } else {
        buf[j] = 0;
      }
    }

    if (output_index + 4 > output_len) {
      return;
    }

    output[output_index++] = base64_table[(buf[0] & 0xFC) >> 2];
    output[output_index++] = base64_table[((buf[0] & 0x03) << 4) | ((buf[1] & 0xF0) >> 4)];
    output[output_index++] = (buf_len > 1) ? base64_table[((buf[1] & 0x0F) << 2) | ((buf[2] & 0xC0) >> 6)] : '=';
    output[output_index++] = (buf_len > 2) ? base64_table[buf[2] & 0x3F] : '=';
  }

  output[output_index] = '\0';

}

int base64_decode(const char *input, int input_len, unsigned char *output, int output_len) {

  int i, j;
  unsigned char buf[4];
  int buf_len;
  int output_index = 0;

  for (i = 0; i < input_len; i += 4) {
    buf_len = 0;
    for (j = 0; j < 4; j++) {
    if (i + j < input_len) {
        if (input[i + j] != '=') {
          buf[j] = strchr(base64_table, input[i + j]) - base64_table;
          buf_len++;
        } else {
          buf[j] = 0;
        }
      }
    }

    if (output_index + buf_len > output_len) {
      return -1;
    }

    output[output_index++] = (buf[0] << 2) | ((buf[1] & 0x30) >> 4);
    if (buf_len > 2) {
      output[output_index++] = ((buf[1] & 0x0F) << 4) | ((buf[2] & 0x3C) >> 2);
    }
    if (buf_len > 3) {
      output[output_index++] = ((buf[2] & 0x03) << 6) | buf[3];
    }
  }

  return output_index;

}
