#include "platform/misc.h"

#include <stdint.h>
#include <string.h>
#include "utils.h"
#include "mbedtls/md.h"

// http://haoyuanliu.github.io/2017/01/16/%E5%9C%B0%E5%9D%80%E6%9F%A5%E8%AF%A2%E5%87%BD%E6%95%B0gethostbyname-%E5%92%8Cgetaddrinfo/
int utils_get_ipv4addr(char *hostname, char *ipv4addr, size_t size) {
#if 0
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
#endif
  return -1;
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

void utils_get_hmac_sha1(const char *input, size_t input_len, const char *key, size_t key_len, unsigned char *output) {

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) key, key_len);
  mbedtls_md_hmac_update(&ctx, (const unsigned char *) input, input_len);
  mbedtls_md_hmac_finish(&ctx, output);
  mbedtls_md_free(&ctx);
}

void utils_get_md5(const char *input, size_t input_len, unsigned char *output) {

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) input, input_len);
  mbedtls_md_finish(&ctx, output);
  mbedtls_md_free(&ctx);
}

uint32_t utils_get_timestamp() {

  struct timeval tv;
  platform_gettimeofday(&tv, NULL);
  return (uint32_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

