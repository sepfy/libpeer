#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif
#include "mbedtls/md.h"
#include "mbedtls/version.h"
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include "psa/crypto.h"
#endif

static unsigned int utils_random_seed(void) {
  unsigned int seed;

#if defined(__linux__) || defined(__APPLE__)
  FILE* f = fopen("/dev/urandom", "rb");

  if (f != NULL) {
    size_t n = fread(&seed, sizeof(seed), 1, f);
    fclose(f);

    if (n == 1) {
      return seed;
    }
  }

  return (unsigned int)time(NULL) ^ (unsigned int)getpid();
#else
  return (unsigned int)time(NULL);
#endif
}

void utils_random_string(char* s, const int len) {
  int i;
  static int seeded = 0;

  static const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  // Seeding on every call from time(NULL) (1-second resolution) made this
  // fully determined by wall-clock time: any two callers (different
  // processes, different devices) invoked within the same second produced
  // byte-identical output, which breaks ICE's requirement (RFC 8445 15.4)
  // that ufrag/pwd be unguessable. Seed once, from real entropy where
  // available.
  if (!seeded) {
    srand(utils_random_seed());
    seeded = 1;
  }

  for (i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  s[len] = '\0';
}

void utils_get_hmac_sha1(const char* input, size_t input_len, const char* key, size_t key_len, unsigned char* output) {
  memset(output, 0, 20);
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
  mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
  size_t out_len = 0;

  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
  psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_1));
  psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
  psa_set_key_bits(&attr, key_len * 8);

  if (psa_crypto_init() == PSA_SUCCESS &&
      psa_import_key(&attr, (const uint8_t*)key, key_len, &key_id) == PSA_SUCCESS) {
    if (psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_1),
                        (const uint8_t*)input, input_len, output, 20, &out_len) != PSA_SUCCESS ||
        out_len != 20) {
      memset(output, 0, 20);
    }
    psa_destroy_key(key_id);
  }
  psa_reset_key_attributes(&attr);
#else
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (md_info != NULL) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    if (mbedtls_md_setup(&ctx, md_info, 1) == 0) {
      mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key, key_len);
      mbedtls_md_hmac_update(&ctx, (const unsigned char*)input, input_len);
      mbedtls_md_hmac_finish(&ctx, output);
    }
    mbedtls_md_free(&ctx);
  }
#endif
}

void utils_get_md5(const char* input, size_t input_len, unsigned char* output) {
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_MD5);
  if (md_info != NULL) {
    mbedtls_md(md_info, (const unsigned char*)input, input_len, output);
  }
}
