#include <stdlib.h>

void base64_encode(const unsigned char* input, int input_len, char* output, int output_len);

int base64_decode(const char* input, int input_len, unsigned char* output, int output_len);
