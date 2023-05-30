#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"
#include "mbedtls/md.h"


// http://haoyuanliu.github.io/2017/01/16/%E5%9C%B0%E5%9D%80%E6%9F%A5%E8%AF%A2%E5%87%BD%E6%95%B0gethostbyname-%E5%92%8Cgetaddrinfo/
int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size) {

  struct addrinfo *ai, *aip;
  struct addrinfo hint;
  struct sockaddr_in *sinp;
  int err;

  hint.ai_flags = AI_CANONNAME;
  hint.ai_family = AF_INET;
  hint.ai_socktype = 0;
  hint.ai_protocol = SOCK_DGRAM;
  hint.ai_addrlen = 0;
  hint.ai_canonname = NULL;
  hint.ai_addr = NULL;
  hint.ai_next = NULL;

  if((err = getaddrinfo(hostname, NULL, &hint, &ai)) != 0) {
    printf("ERROR: getaddrinfo error: %s\n", gai_strerror(err));
    return -1;
  }

  for(aip = ai; aip != NULL; aip = aip->ai_next) {
    if(aip->ai_family == AF_INET) {
      sinp = (struct sockaddr_in *)aip->ai_addr;
      inet_ntop(AF_INET, &sinp->sin_addr, ipv4addr, size);
      return 0;
    }
  }

  return -1;
}

int utils_is_valid_ip_address(char *ip_address) {
  struct sockaddr_in sa;
  int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
  return result == 0;
}

Buffer* utils_buffer_new(int size) {

  Buffer *rb;
  rb = (Buffer*)malloc(sizeof(Buffer));

  rb->data = (uint8_t*)malloc(size);
  rb->size = size;
  rb->head = 0;
  rb->tail = 0;

  return rb;
}

void utils_buffer_free(Buffer *rb) {

  if (rb) {

    free(rb->data);
    rb->data = NULL;
    rb = NULL;
  }
}

int utils_buffer_push(Buffer *rb, uint8_t *data, int size) {

  int free_space = (rb->size + rb->head - rb->tail - 1) % rb->size;
  if (size > free_space) {
    return -1;
  }
  int tail_end = (rb->tail + size) % rb->size;
  if (tail_end < rb->tail) {
      int first_size = rb->size - rb->tail;
      memcpy(rb->data + rb->tail, data, first_size);
      memcpy(rb->data, data + first_size, size - first_size);
  } else {
      memcpy(rb->data + rb->tail, data, size);
  }
  rb->tail = tail_end;
}

int utils_buffer_pop(Buffer *rb, uint8_t *data, int size) {

  int used_space = (rb->size + rb->tail - rb->head) % rb->size;
  if (size > used_space) {
    return -1;
  }
  int head_end = (rb->head + size) % rb->size;
  if (head_end < rb->head) {
      int first_size = rb->size - rb->head;
      memcpy(data, rb->data + rb->head, first_size);
      memcpy(data + first_size, rb->data, size - first_size);
  } else {
      memcpy(data, rb->data + rb->head, size);
  }
  rb->head = head_end;
  return size;
}

void utils_random_string(char *s, const int len) {

  int i;

  static const char alphanum[] =
   "0123456789"
   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
   "abcdefghijklmnopqrstuvwxyz";

  srand(time(NULL));

  for (i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  s[len] = '\0';
}

void utils_get_sha1(const char *input, size_t input_len, const char *key, unsigned char *output) {

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) key, strlen(key));
  mbedtls_md_hmac_update(&ctx, (const unsigned char *) input, input_len);
  mbedtls_md_hmac_finish(&ctx, output);
  mbedtls_md_free(&ctx);

}


