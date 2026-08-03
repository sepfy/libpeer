#include "utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mbedtls/version.h"
#if MBEDTLS_VERSION_MAJOR >= 4
#include "psa/crypto.h"
#else
#include "mbedtls/md.h"
#endif

void utils_random_string(char* s, const int len) {
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

void utils_get_hmac_sha1(const char* input, size_t input_len, const char* key, size_t key_len, unsigned char* output) {
#if MBEDTLS_VERSION_MAJOR >= 4
  psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
  mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
  size_t output_len = 0;

  psa_status_t status = psa_crypto_init();
  if (status != PSA_SUCCESS) {
    LOGE("psa_crypto_init failed (%d)", (int)status);
    memset(output, 0, 20);
    return;
  }

  psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
  psa_set_key_bits(&attributes, key_len * 8);
  psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
  psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_1));

  status = psa_import_key(&attributes, (const unsigned char*)key, key_len, &key_id);
  psa_reset_key_attributes(&attributes);
  if (status == PSA_SUCCESS) {
    status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_1),
                             (const unsigned char*)input, input_len,
                             output, 20, &output_len);
    psa_destroy_key(key_id);
  }
  if (status != PSA_SUCCESS || output_len != 20) {
    LOGE("PSA HMAC-SHA1 failed (%d)", (int)status);
    memset(output, 0, 20);
  }
#else
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key, key_len);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)input, input_len);
  mbedtls_md_hmac_finish(&ctx, output);
  mbedtls_md_free(&ctx);
#endif
}

void utils_get_md5(const char* input, size_t input_len, unsigned char* output) {
#if MBEDTLS_VERSION_MAJOR >= 4
  size_t output_len = 0;
  psa_status_t status = psa_crypto_init();
  if (status == PSA_SUCCESS) {
    status = psa_hash_compute(PSA_ALG_MD5, (const unsigned char*)input, input_len,
                              output, 16, &output_len);
  }
  if (status != PSA_SUCCESS || output_len != 16) {
    LOGE("PSA MD5 failed (%d)", (int)status);
    memset(output, 0, 16);
  }
#else
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char*)input, input_len);
  mbedtls_md_finish(&ctx, output);
  mbedtls_md_free(&ctx);
#endif
}
