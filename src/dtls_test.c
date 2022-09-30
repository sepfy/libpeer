#include "gtest/gtest.h"

#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"

void dtls_get_x509_digest_text(mbedtls_x509_crt *crt, char *digest_text, size_t size) {

  unsigned char digest[32];
  int i;  

  mbedtls_sha256_context sha256;
  mbedtls_sha256_init(&sha256);
  mbedtls_sha256_starts(&sha256, 0);
  mbedtls_sha256_update(&sha256, crt->raw.p, crt->raw.len);
  mbedtls_sha256_finish(&sha256, digest);

  memset(digest_text, 0, size);

  for(i = 0; i < 32; i++) {
    snprintf(digest_text, 4, "%.2X:", digest[i]);
    digest_text += 3;
  }
}

TEST(DTLS, X509) {

  static const int key_length = 2048;
  static int exponential = 65537;
  static const char subject_name[] = "C=UK,O=ARM,CN=mbed TLS CA";
  static const char serial_text[] = "1";

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_mpi serial;

  mbedtls_pk_context pk;
  mbedtls_rsa_context *rsa;

  mbedtls_x509write_cert crt;

  mbedtls_sha256_context sha256;


  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_entropy_init(&entropy);
  mbedtls_mpi_init(&serial);
  mbedtls_pk_init(&pk);
  mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  mbedtls_x509write_crt_init(&crt);

  mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);

  rsa = mbedtls_pk_rsa(pk);
   
  mbedtls_rsa_gen_key(rsa, mbedtls_ctr_drbg_random, &ctr_drbg, key_length, exponential);

  mbedtls_x509write_crt_set_subject_key(&crt, &pk);
  mbedtls_x509write_crt_set_issuer_key(&crt, &pk); 

  mbedtls_x509write_crt_set_subject_name(&crt, subject_name);
  mbedtls_x509write_crt_set_issuer_name(&crt, subject_name);

  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

  mbedtls_mpi_read_string(&serial, 10, serial_text);
  mbedtls_x509write_crt_set_serial(&crt, &serial);
  mbedtls_x509write_crt_set_validity(&crt, "20131231235959", "20301231235959");
  mbedtls_x509write_crt_set_subject_key_identifier(&crt);
  mbedtls_x509write_crt_set_authority_key_identifier(&crt);

  mbedtls_x509write_crt_set_basic_constraints(&crt, 1, -1);

  unsigned char output_buf[4096];
  memset(output_buf, 0, sizeof(output_buf));
  mbedtls_x509write_crt_pem(&crt, output_buf, 4096, mbedtls_ctr_drbg_random, &ctr_drbg);

  mbedtls_x509_crt x509_crt;
  mbedtls_x509_crt_init(&x509_crt);

  int ret;
  ret = mbedtls_x509_crt_parse(&x509_crt, (const unsigned char*)output_buf, 4096);
 
  printf("ret = %d, %s\n", ret, output_buf);
  char fingerprint[160];
  dtls_get_x509_digest_text(&x509_crt, fingerprint, sizeof(fingerprint));

  printf("==>%s\n", fingerprint);
}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
