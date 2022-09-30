#include <glib.h>
#include <stdio.h>

#include "sctp.h"
#include "srtp.h"
#include "utils.h"
#include "dtls_transport.h"

#include "mbedtls/ssl_cookie.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ssl.h"
#include "mbedtls/debug.h"
#include "mbedtls/timing.h"

#define SRTP_MASTER_KEY_LENGTH  16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_MASTER_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)

#define MBEDTLS_CHECK(mbedtls_function) if(mbedtls_function != 0) { printf("failed! errno: %d\n", mbedtls_function); }

struct DtlsTransport {

  SSL *ssl;
  SSL_CTX *ssl_ctx;
  X509 *certificate;
  EVP_PKEY *private_key;
  BIO *read_bio;
  BIO *write_bio;

  srtp_policy_t remote_policy;
  srtp_policy_t local_policy;
  srtp_t srtp_in;
  srtp_t srtp_out;

  Sctp *sctp;

  char fingerprint[160];

  mbedtls_x509_crt x509_crt;
  mbedtls_ssl_context ssl_mbedtls;

  gboolean handshake_done;
  gboolean srtp_init_done;
};


int cb_dtls_verify(int preverify_ok, X509_STORE_CTX *ctx) {

  gboolean dtls_selfsigned_certs_ok = TRUE;
  int err = X509_STORE_CTX_get_error(ctx);
  if(err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT || err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) {
    if(!dtls_selfsigned_certs_ok) {
      return 0;
    }
  }

  if(err == X509_V_ERR_CERT_HAS_EXPIRED)
    return 0;

  return dtls_selfsigned_certs_ok ? 1 : (err == X509_V_OK);
}
static void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
    ((void) level);
printf("[[[[%s][[[[%d]]]]\n", __func__, __LINE__);
    fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
    fflush(  (FILE *) ctx  );
}


void generate_self_certificate(DtlsTransport *dtls_transport) {

  const int num_bits = 2048;
  BIGNUM *bne = NULL;
  RSA *rsa_key = NULL;
  X509_NAME *cert_name = NULL;
  EC_KEY *ecc_key = NULL;

  dtls_transport->private_key = EVP_PKEY_new();
  if(dtls_transport->private_key == NULL) {
    LOG_ERROR();
  }
  bne = BN_new();
  if(!bne) {
    LOG_ERROR();
  }

  if(!BN_set_word(bne, RSA_F4)) {  /* RSA_F4 == 65537 */
    LOG_ERROR();
  }

  rsa_key = RSA_new();
  if(!rsa_key) {
    LOG_ERROR();
  }

  if(!RSA_generate_key_ex(rsa_key, num_bits, bne, NULL)) {
    LOG_ERROR();
  }

  if(!EVP_PKEY_assign_RSA(dtls_transport->private_key, rsa_key)) {
    LOG_ERROR();
  }

  rsa_key = NULL;

  dtls_transport->certificate = X509_new();
  if(!dtls_transport->certificate) {
    LOG_ERROR();
  }

  X509_set_version(dtls_transport->certificate, 2);


  X509_gmtime_adj(X509_get_notBefore(dtls_transport->certificate), -1 * 60*60*24*365);  /* -1 year */
  X509_gmtime_adj(X509_get_notAfter(dtls_transport->certificate), 60*60*24*365);  /* 1 year */

  if(!X509_set_pubkey(dtls_transport->certificate, dtls_transport->private_key)) {
    LOG_ERROR();
  }

  cert_name = X509_get_subject_name(dtls_transport->certificate);
  if(!cert_name) {
    LOG_ERROR();
  }
  X509_NAME_add_entry_by_txt(cert_name, "O", MBSTRING_ASC, (const unsigned char*)"Test", -1, -1, 0);
  X509_NAME_add_entry_by_txt(cert_name, "CN", MBSTRING_ASC, (const unsigned char*)"Test", -1, -1, 0);

  if(!X509_set_issuer_name(dtls_transport->certificate, cert_name)) {
    LOG_ERROR();
  }

  if(!X509_sign(dtls_transport->certificate, dtls_transport->private_key, EVP_sha1())) {
    LOG_ERROR();
  }

  BN_free(bne);

}

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

  *(digest_text - 1) = '\0';
}

int dtls_mbedtls_ice_recv_timeout( void *ctx,
                                        unsigned char *buf,
                                        size_t len,
                                        uint32_t timeout ){
printf("recv timeout...\n");
  LOG_INFO("");
}

int dtls_mbedtls_ice_send(void *ctx, const unsigned char *buf, size_t len) {
LOG_INFO("");
}

int dtls_mbedtls_ice_recv(void *ctx, unsigned char *buf, size_t len) {
LOG_INFO("");
}

void dtls_get_x509_selfsign_crt(DtlsTransport *dtls) {

  static const int key_length = 2048;
  static int exponential = 65537;
  static const char subject_name[] = "C=UK,O=ARM,CN=mbed TLS CA";
  static const char serial_text[] = "1";
  unsigned char output_buf[4096];

  static mbedtls_entropy_context entropy;
  static mbedtls_ctr_drbg_context ctr_drbg;
  static mbedtls_mpi serial;

  static mbedtls_ssl_config conf;
  static mbedtls_pk_context pk;
  static mbedtls_rsa_context *rsa;

  static mbedtls_x509write_cert crt;
  static mbedtls_sha256_context sha256;

  //mbedtls_ssl_conf_dbg( &conf, my_debug, stdout );
  //mbedtls_debug_set_threshold(3);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mbedtls_entropy_init(&entropy);
  mbedtls_mpi_init(&serial);
  mbedtls_pk_init(&pk);
  mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  mbedtls_x509write_crt_init(&crt);
  mbedtls_x509_crt_init(&dtls->x509_crt);
  mbedtls_ssl_init(&dtls->ssl_mbedtls);

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

  memset(output_buf, 0, sizeof(output_buf));
  mbedtls_x509write_crt_pem(&crt, output_buf, sizeof(output_buf), mbedtls_ctr_drbg_random, &ctr_drbg);

  mbedtls_x509_crt_parse(&dtls->x509_crt, output_buf, 4096);

  dtls_get_x509_digest_text(&dtls->x509_crt, dtls->fingerprint, sizeof(dtls->fingerprint));
  printf("%s\n", dtls->fingerprint);


  // Setup DTLS
  MBEDTLS_CHECK(mbedtls_ssl_config_defaults(&conf,
   MBEDTLS_SSL_IS_SERVER,
   MBEDTLS_SSL_TRANSPORT_DATAGRAM,
   MBEDTLS_SSL_PRESET_DEFAULT));

  mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
  mbedtls_ssl_conf_read_timeout(&conf, 1000);

  mbedtls_ssl_conf_ca_chain(&conf, &dtls->x509_crt, NULL );
  MBEDTLS_CHECK(mbedtls_ssl_conf_own_cert(&conf, &dtls->x509_crt, &pk));

    mbedtls_ssl_cookie_ctx cookie_ctx;
    mbedtls_ssl_cookie_init( &cookie_ctx );
  mbedtls_ssl_cookie_setup( &cookie_ctx, mbedtls_ctr_drbg_random, &ctr_drbg );

    mbedtls_ssl_conf_dtls_cookies( &conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check,
                               &cookie_ctx );
mbedtls_timing_delay_context timer;
    mbedtls_ssl_set_timer_cb(&dtls->ssl_mbedtls, &timer, mbedtls_timing_set_delay,
                                            mbedtls_timing_get_delay );

  MBEDTLS_CHECK(mbedtls_ssl_setup(&dtls->ssl_mbedtls, &conf));

  mbedtls_ssl_set_bio(&dtls->ssl_mbedtls, NULL, dtls_mbedtls_ice_send, dtls_mbedtls_ice_recv, dtls_mbedtls_ice_recv_timeout);
  printf("set bio...\n");


}


int dtls_transport_init(DtlsTransport *dtls_transport, BIO *agent_write_bio) {

  dtls_transport->ssl_ctx = SSL_CTX_new(DTLS_method());
  SSL_CTX_set_verify(dtls_transport->ssl_ctx,
   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, cb_dtls_verify);

  SSL_CTX_set_tlsext_use_srtp(dtls_transport->ssl_ctx, "SRTP_AES128_CM_SHA1_80");

  generate_self_certificate(dtls_transport);
  if(!SSL_CTX_use_certificate(dtls_transport->ssl_ctx, dtls_transport->certificate)) {
    LOG_ERROR("use certificate failed");
    return -1;
  }

  if(!SSL_CTX_use_PrivateKey(dtls_transport->ssl_ctx, dtls_transport->private_key)) {
    LOG_ERROR("use private key failed");
    return -1;
  }
  if(!SSL_CTX_check_private_key(dtls_transport->ssl_ctx)) {
    LOG_ERROR("check preverify key failed");
    return -1;
  }
  SSL_CTX_set_read_ahead(dtls_transport->ssl_ctx, 1);

  unsigned int size;
  unsigned char fingerprint[EVP_MAX_MD_SIZE];
  if(X509_digest(dtls_transport->certificate, EVP_sha256(), (unsigned char *)fingerprint, &size) == 0) {
    LOG_ERROR("generate fingerprint failed");
    return -1;
  }

  char *lfp = (char *)&dtls_transport->fingerprint;
  unsigned int i = 0;
  for(i = 0; i < size; i++) {
    g_snprintf(lfp, 4, "%.2X:", fingerprint[i]);
    lfp += 3;
  }
  *(lfp-1) = 0;

  dtls_transport->ssl = SSL_new(dtls_transport->ssl_ctx);

  dtls_transport->read_bio = BIO_new(BIO_s_mem());
  dtls_transport->write_bio = agent_write_bio;
  BIO_set_mem_eof_return(dtls_transport->read_bio, -1);
  SSL_set_bio(dtls_transport->ssl, dtls_transport->read_bio, dtls_transport->write_bio);

  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if(ecdh == NULL) {
    LOG_ERROR("New ecdh curve by name failed"); 
    return -1;
  }

  const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION | SSL_OP_SINGLE_ECDH_USE;
  SSL_set_options(dtls_transport->ssl, flags);
  SSL_set_tmp_ecdh(dtls_transport->ssl, ecdh);
  EC_KEY_free(ecdh);

  dtls_transport->handshake_done = FALSE;
  dtls_transport->srtp_init_done = FALSE;

  if(srtp_init() != srtp_err_status_ok) {
    LOG_ERROR("libsrtp init failed");
  }

  dtls_get_x509_selfsign_crt(dtls_transport);
printf("p = %p\n", &dtls_transport->ssl_mbedtls);

  return 0;
}

DtlsTransport* dtls_transport_create(BIO *agent_write_bio) {

  DtlsTransport *dtls_transport = NULL;
  dtls_transport = (DtlsTransport*)calloc(1, sizeof(DtlsTransport));
  if(dtls_transport == NULL)
    return dtls_transport;

  dtls_transport_init(dtls_transport, agent_write_bio);


  return dtls_transport;
}

void dtls_transport_destroy(DtlsTransport *dtls_transport) {

  if(dtls_transport == NULL)
    return;

  SSL_CTX_free(dtls_transport->ssl_ctx);
  SSL_free(dtls_transport->ssl);
  X509_free(dtls_transport->certificate);
  EVP_PKEY_free(dtls_transport->private_key);

  if(dtls_transport->srtp_in)
    srtp_dealloc(dtls_transport->srtp_in);
  if(dtls_transport->srtp_out)
    srtp_dealloc(dtls_transport->srtp_out);

  srtp_shutdown();

  free(dtls_transport);
}


void* mythread(void *ctx) {
  DtlsTransport *dtls_transport = (DtlsTransport*)ctx;

  pthread_exit(NULL);
}

void dtls_transport_do_handshake(DtlsTransport *dtls_transport) {

//  SSL_set_accept_state(dtls_transport->ssl);
//  SSL_do_handshake(dtls_transport->ssl);
  printf("do handshaking\n");
  pthread_t t;
  pthread_create(&t, NULL, mythread, dtls_transport);
  dtls_transport->handshake_done = TRUE;

}

int dtls_transport_validate(char *buf) {

  if(buf == NULL)
    return 0;

  return ((*buf >= 19) && (*buf <= 64));
}

int dtls_transport_get_srtp_initialized(DtlsTransport *dtls_transport) {

  if(dtls_transport) {
    return dtls_transport->srtp_init_done;
  }

  return 0;
}

int dtls_transport_sctp_to_dtls(DtlsTransport *dtls_transport, uint8_t *data, int bytes) {

  uint8_t buf[2048] = {0};
  if(SSL_write(dtls_transport->ssl, data, bytes) != bytes) {
    //LOG_WARN("");
  }

}

int dtls_transport_decrypt_data(DtlsTransport *dtls_transport, char *encrypted_data, int encrypted_len,
 char *decrypted_data, int decrypted_len) {

  if(!dtls_transport || !dtls_transport->handshake_done) {
    LOG_WARN("dtls_transport is not initialized");
    return -1;
  }

  int written = BIO_write(dtls_transport->read_bio, encrypted_data, encrypted_len);
  if(written != encrypted_len) {
    LOG_WARN("decrypt data error");
  }

  memset(decrypted_data, 0, decrypted_len);

  int read = SSL_read(dtls_transport->ssl, decrypted_data, decrypted_len);
  if(read < 0) {
    unsigned long err = SSL_get_error(dtls_transport->ssl, read);
    if(err == SSL_ERROR_SSL) {
      char error[200];
      ERR_error_string_n(ERR_get_error(), error, 200);
      LOG_ERROR("%s", error);
    }
  }

  return read;
}

void dtls_transport_incomming_msg(DtlsTransport *dtls_transport, char *buf, int len) {

  static int handshake = 0;
  printf("incoming...\n");
  int ret;


#if 0
  int written = BIO_write(dtls_transport->read_bio, buf, len);
  if(written != len) {
    LOG_ERROR();
  }
  else {

  }


  if(!dtls_transport->handshake_done)
    return;

  char data[3000];
  memset(data, 0, 3000);
  int read = SSL_read(dtls_transport->ssl, data, 3000);
  if(read < 0) {
    unsigned long err = SSL_get_error(dtls_transport->ssl, read);
    if(err == SSL_ERROR_SSL) {
      char error[200];
      ERR_error_string_n(ERR_get_error(), error, 200);
      LOG_ERROR("%s", error);
    }
  }

  if(!SSL_is_init_finished(dtls_transport->ssl)) {
    return;
  }

/*
  if(!dtls_transport->sctp) {
    dtls_transport->sctp = sctp_create(dtls_transport);
  }

  if(dtls_transport->sctp)
    sctp_incoming_data(dtls_transport->sctp, data, read);
*/
  if(dtls_transport->srtp_init_done)
    return;


  X509 *rcert = SSL_get_peer_certificate(dtls_transport->ssl);
  if(!rcert) {
    LOG_ERROR("%s", ERR_reason_error_string(ERR_get_error()));
  }
  else {
    unsigned int rsize;
    unsigned char rfingerprint[EVP_MAX_MD_SIZE];
    X509_digest(rcert, EVP_sha256(), (unsigned char *)rfingerprint, &rsize);
    char remote_fingerprint[160];
    char *rfp = (char *)&remote_fingerprint;
    X509_free(rcert);
    rcert = NULL;
    unsigned int i = 0;
    for(i = 0; i < rsize; i++) {
      g_snprintf(rfp, 4, "%.2X:", rfingerprint[i]);
      rfp += 3;
    }
    *(rfp-1) = 0;

    LOG_INFO("Remote fingerprint %s", remote_fingerprint);
    LOG_INFO("Local fingerprint %s", dtls_transport->fingerprint);
  }

  memset(&dtls_transport->remote_policy, 0x0, sizeof(dtls_transport->remote_policy));
  memset(&dtls_transport->local_policy, 0x0, sizeof(dtls_transport->local_policy));
  unsigned char material[SRTP_MASTER_LENGTH*2];
  unsigned char *local_key, *local_salt, *remote_key, *remote_salt;

  if (!SSL_export_keying_material(dtls_transport->ssl, material,
   SRTP_MASTER_LENGTH*2, "EXTRACTOR-dtls_srtp", 19, NULL, 0, 0)) {
    LOG_ERROR("Couldn't extract SRTP keying material(%s)", ERR_reason_error_string(ERR_get_error()));
  }

  /* Key derivation (http://tools.ietf.org/html/rfc5764#section-4.2) */
  remote_key = material;
  local_key = remote_key + SRTP_MASTER_KEY_LENGTH;
  remote_salt = local_key + SRTP_MASTER_KEY_LENGTH;
  local_salt = remote_salt + SRTP_MASTER_SALT_LENGTH;
  srtp_crypto_policy_set_rtp_default(&(dtls_transport->remote_policy.rtp));
  srtp_crypto_policy_set_rtcp_default(&(dtls_transport->remote_policy.rtcp));
  dtls_transport->remote_policy.ssrc.type = ssrc_any_inbound;
  unsigned char remote_policy_key[SRTP_MASTER_LENGTH];
  dtls_transport->remote_policy.key = (unsigned char *)&remote_policy_key;
  memcpy(dtls_transport->remote_policy.key, remote_key, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_transport->remote_policy.key + SRTP_MASTER_KEY_LENGTH, remote_salt, SRTP_MASTER_SALT_LENGTH);
  dtls_transport->remote_policy.next = NULL;
  srtp_crypto_policy_set_rtp_default(&(dtls_transport->local_policy.rtp));
  srtp_crypto_policy_set_rtcp_default(&(dtls_transport->local_policy.rtcp));
  dtls_transport->local_policy.ssrc.type = ssrc_any_outbound;
  unsigned char local_policy_key[SRTP_MASTER_LENGTH];
  dtls_transport->local_policy.key = (unsigned char *)&local_policy_key;
  memcpy(dtls_transport->local_policy.key, local_key, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_transport->local_policy.key + SRTP_MASTER_KEY_LENGTH, local_salt, SRTP_MASTER_SALT_LENGTH);
  dtls_transport->local_policy.next = NULL;

  srtp_err_status_t res = srtp_create(&dtls_transport->srtp_in, &dtls_transport->remote_policy);
  if(res != srtp_err_status_ok) {
    LOG_ERROR("Error creating inbound SRTP session for component");
  }
  LOG_INFO("Created inbound SRTP session");

  res = srtp_create(&(dtls_transport->srtp_out), &(dtls_transport->local_policy));
  if(res != srtp_err_status_ok) {
    LOG_ERROR("Error creating outbound SRTP session");
  }
  LOG_INFO("Created outbound SRTP session");
  dtls_transport->srtp_init_done = TRUE;

//  dtls_transport->sctp = sctp_create(dtls_transport);

#endif
}

void dtls_transport_decrypt_rtp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes) {

  srtp_unprotect(dtls_transport->srtp_in, packet, bytes);
}

void dtls_transport_decrypt_rtcp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes) {

  srtp_unprotect_rtcp(dtls_transport->srtp_in, packet, bytes);
}

void dtls_transport_encrypt_rtp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes) {

  srtp_protect(dtls_transport->srtp_out, packet, bytes);
}

void dtls_transport_encrypt_rctp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes) {

  srtp_protect_rtcp(dtls_transport->srtp_out, packet, bytes);
}

const char* dtls_transport_get_fingerprint(DtlsTransport *dtls_transport) {

  return dtls_transport->fingerprint;
}
