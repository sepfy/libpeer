#include "gtest/gtest.h"

#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"
#include "mbedtls/rsa.h"

TEST(DTLS, X509) {

  static const int key_length = 2048;
  static int exponential = 65537;
  static const char subject_name[] = "pear";
  static const char serial_text[] = "123456";

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  mbedtls_mpi serial;

  mbedtls_pk_context pk;
  mbedtls_rsa_context *rsa;

  mbedtls_x509write_cert crt;

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

  mbedtls_x509write_crt_set_version(&crt, 2);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

  mbedtls_mpi_read_string(&serial, 10, serial_text);
  mbedtls_x509write_crt_set_serial(&crt, &serial);

}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
