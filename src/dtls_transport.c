#include <glib.h>

#include "utils.h"
#include "dtls_transport.h"

#define SRTP_MASTER_KEY_LENGTH  16
#define SRTP_MASTER_SALT_LENGTH 14
#define SRTP_MASTER_LENGTH (SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH)

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

void generate_self_certificate(dtls_transport_t *dtls_transport) {

  const int num_bits = 2048;
  BIGNUM *bne = NULL;
  RSA *rsa_key = NULL;
  X509_NAME *cert_name = NULL;
  EC_KEY *ecc_key = NULL;

  dtls_transport->private_key = EVP_PKEY_new();
  if(dtls_transport->private_key == NULL) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }
  bne = BN_new();
  if(!bne) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  if(!BN_set_word(bne, RSA_F4)) {  /* RSA_F4 == 65537 */
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  rsa_key = RSA_new();
  if(!rsa_key) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  if(!RSA_generate_key_ex(rsa_key, num_bits, bne, NULL)) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  if(!EVP_PKEY_assign_RSA(dtls_transport->private_key, rsa_key)) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  rsa_key = NULL;

  dtls_transport->certificate = X509_new();
  if(!dtls_transport->certificate) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  X509_set_version(dtls_transport->certificate, 2);


  X509_gmtime_adj(X509_get_notBefore(dtls_transport->certificate), -1 * 60*60*24*365);  /* -1 year */
  X509_gmtime_adj(X509_get_notAfter(dtls_transport->certificate), 60*60*24*365);  /* 1 year */

  /* Set the public key for the certificate using the key. */
  if(!X509_set_pubkey(dtls_transport->certificate, dtls_transport->private_key)) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  /* Set certificate fields. */
  cert_name = X509_get_subject_name(dtls_transport->certificate);
  if(!cert_name) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }
  X509_NAME_add_entry_by_txt(cert_name, "O", MBSTRING_ASC, (const unsigned char*)"Test", -1, -1, 0);
  X509_NAME_add_entry_by_txt(cert_name, "CN", MBSTRING_ASC, (const unsigned char*)"Test", -1, -1, 0);

  /* It is self-signed so set the issuer name to be the same as the subject. */
  if(!X509_set_issuer_name(dtls_transport->certificate, cert_name)) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  /* Sign the certificate with the private key. */
  if(!X509_sign(dtls_transport->certificate, dtls_transport->private_key, EVP_sha1())) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }

  /* Free stuff and resurn. */
  BN_free(bne);

}

int dtls_transport_init(dtls_transport_t *dtls_transport, BIO *agent_write_bio) {

  dtls_transport->ssl_ctx = SSL_CTX_new(DTLS_method());
  SSL_CTX_set_verify(dtls_transport->ssl_ctx,
   SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, cb_dtls_verify);

  SSL_CTX_set_tlsext_use_srtp(dtls_transport->ssl_ctx, "SRTP_AES128_CM_SHA1_80");

  generate_self_certificate(dtls_transport);
  if(!SSL_CTX_use_certificate(dtls_transport->ssl_ctx, dtls_transport->certificate)) {
    g_error("use certificate failed");
    return -1;
  }

  if(!SSL_CTX_use_PrivateKey(dtls_transport->ssl_ctx, dtls_transport->private_key)) {
    g_error("use private key failed");
    return -1;
  }
  if(!SSL_CTX_check_private_key(dtls_transport->ssl_ctx)) {
    g_error("check preverify key failed");
    return -1;
  }
  SSL_CTX_set_read_ahead(dtls_transport->ssl_ctx, 1);

  unsigned int size;
  unsigned char fingerprint[EVP_MAX_MD_SIZE];
  if(X509_digest(dtls_transport->certificate, EVP_sha256(), (unsigned char *)fingerprint, &size) == 0) {
    g_error("generate fingerprint failed");
    return -1;
  }

  char *lfp = (char *)&dtls_transport->fingerprint;
  unsigned int i = 0;
  for(i = 0; i < size; i++) {
    g_snprintf(lfp, 4, "%.2X:", fingerprint[i]);
    lfp += 3;
  }
  *(lfp-1) = 0;
  //fprintf(stdout, "Fingerprint of our certificate: %s\n", dtls_transport->fingerprint);


  dtls_transport->ssl = SSL_new(dtls_transport->ssl_ctx);

  dtls_transport->read_bio = BIO_new(BIO_s_mem());
  dtls_transport->write_bio = agent_write_bio;
  BIO_set_mem_eof_return(dtls_transport->read_bio, -1);
  SSL_set_bio(dtls_transport->ssl, dtls_transport->read_bio, dtls_transport->write_bio);

  EC_KEY *ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if(ecdh == NULL) {
    g_error("New ecdh curve by name failed"); 
    return -1;
  }

  const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION | SSL_OP_SINGLE_ECDH_USE;
  SSL_set_options(dtls_transport->ssl, flags);
  SSL_set_tmp_ecdh(dtls_transport->ssl, ecdh);
  EC_KEY_free(ecdh);

  return 0;
}

void dtls_transport_do_handshake(dtls_transport_t *dtls_transport) {
  fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  SSL_set_accept_state(dtls_transport->ssl);
  SSL_do_handshake(dtls_transport->ssl);
}

int dtls_transport_is_dtls(char *buf) {
  return ((*buf >= 20) && (*buf <= 64));
}


void dtls_transport_incomming_msg(dtls_transport_t *dtls_transport, char *buf, int len) {

  static int do_handshake = 0;
  int written = BIO_write(dtls_transport->read_bio, buf, len);
  if(written != len) {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  } else {
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
  }
  /* Try to read data */
  char data[3000];      /* FIXME */
  memset(&data, 0, 3000);
  int read = SSL_read(dtls_transport->ssl, &data, 3000);
  if(read < 0) {
    unsigned long err = SSL_get_error(dtls_transport->ssl, read);
    if(err == SSL_ERROR_SSL) {
      /* Ops, something went wrong with the DTLS handshake */
      char error[200];
      ERR_error_string_n(ERR_get_error(), error, 200);
      fprintf(stdout, "[%s][%d] %s\n", __func__, __LINE__, error);
    }
  }

  //    if(do_handshake == 0) {
  //            do_handshake = 1;
  //    }
  if(!SSL_is_init_finished(dtls_transport->ssl)) {
    return;
  }

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
    fprintf(stdout, "[%s][%d]\n", __func__, __LINE__);
    rcert = NULL;
    unsigned int i = 0;
    for(int i = 0; i < rsize; i++) {
      g_snprintf(rfp, 4, "%.2X:", rfingerprint[i]);
      rfp += 3;
    }
    *(rfp-1) = 0;

    LOG_INFO("Remote fingerprint %s", remote_fingerprint);
    LOG_INFO("Local fingerprint %s", dtls_transport->fingerprint);
  }

  if(srtp_init() != srtp_err_status_ok) {
    LOG_ERROR("libsrtp init failed");
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
}

void dtls_transport_encrypt_rtp_packet(dtls_transport_t *dtls_transport, uint8_t *packet, int *bytes) {
  srtp_protect(dtls_transport->srtp_out, packet, bytes);
}
