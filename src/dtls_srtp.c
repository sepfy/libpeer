#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "address.h"
#include "config.h"
#include "dtls_srtp.h"
#if CONFIG_MBEDTLS_DEBUG
#include "mbedtls/debug.h"
#endif
#include "mbedtls/md.h"
#include "mbedtls/ssl.h"
#include "mbedtls/version.h"
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
#include "psa/crypto.h"
#endif
#include "ports.h"
#include "socket.h"
#include "utils.h"

int dtls_srtp_udp_send(void* ctx, const uint8_t* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  UdpSocket* udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret = udp_socket_sendto(udp_socket, dtls_srtp->remote_addr, buf, len);

  return ret;
}

int dtls_srtp_udp_recv(void* ctx, uint8_t* buf, size_t len) {
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)ctx;
  UdpSocket* udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret;

  while ((ret = udp_socket_recvfrom(udp_socket, &udp_socket->bind_addr, buf, len)) <= 0) {
    ports_sleep_ms(1);
  }

  return ret;
}

static void dtls_srtp_x509_digest(const mbedtls_x509_crt* crt, char* buf) {
  int i;
  unsigned char digest[32];
  const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md_info == NULL || mbedtls_md(md_info, crt->raw.p, crt->raw.len, digest) != 0) {
    memset(digest, 0, sizeof(digest));
  }

  for (i = 0; i < 32; i++) {
    snprintf(buf, 4, "%.2X:", digest[i]);
    buf += 3;
  }

  *(--buf) = '\0';
}

// Do not verify CA
static int dtls_srtp_cert_verify(void* data, mbedtls_x509_crt* crt, int depth, uint32_t* flags) {
  *flags &= ~(MBEDTLS_X509_BADCERT_NOT_TRUSTED | MBEDTLS_X509_BADCERT_CN_MISMATCH | MBEDTLS_X509_BADCERT_BAD_KEY);
  return 0;
}

static int dtls_srtp_generate_keypair(DtlsSrtp* dtls_srtp) {
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  int ret;
  mbedtls_svc_key_id_t key_id = MBEDTLS_SVC_KEY_ID_INIT;
  psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

  psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_VERIFY_HASH);
#if CONFIG_DTLS_USE_ECDSA
  psa_set_key_algorithm(&attr, MBEDTLS_PK_ALG_ECDSA(PSA_ALG_ANY_HASH));
  psa_set_key_type(&attr, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
  psa_set_key_bits(&attr, 256);
#else
  psa_set_key_algorithm(&attr, PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_ANY_HASH));
  psa_set_key_type(&attr, PSA_KEY_TYPE_RSA_KEY_PAIR);
  psa_set_key_bits(&attr, RSA_KEY_LENGTH);
#endif
  if (psa_generate_key(&attr, &key_id) != PSA_SUCCESS) {
    psa_reset_key_attributes(&attr);
    LOGE("psa_generate_key failed");
    return -1;
  }
  psa_reset_key_attributes(&attr);

  ret = mbedtls_pk_wrap_psa(&dtls_srtp->pkey, key_id);
  if (ret != 0) {
    psa_destroy_key(key_id);
    LOGE("mbedtls_pk_wrap_psa failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  dtls_srtp->psa_key_id = key_id;
#endif
  return 0;
#else
#if CONFIG_DTLS_USE_ECDSA
  int ret = mbedtls_pk_setup(&dtls_srtp->pkey, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
  if (ret != 0) {
    return ret;
  }
  return mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,
                             mbedtls_pk_ec(dtls_srtp->pkey),
                             mbedtls_ctr_drbg_random,
                             &dtls_srtp->ctr_drbg);
#else
  int ret = mbedtls_pk_setup(&dtls_srtp->pkey, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    return ret;
  }
  return mbedtls_rsa_gen_key(mbedtls_pk_rsa(dtls_srtp->pkey),
                             mbedtls_ctr_drbg_random,
                             &dtls_srtp->ctr_drbg,
                             RSA_KEY_LENGTH,
                             65537);
#endif
#endif
}

static int dtls_srtp_selfsign_cert(DtlsSrtp* dtls_srtp) {
  int ret;
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  mbedtls_x509write_cert crt;
  unsigned char* cert_buf = NULL;
  unsigned char serial_raw[16];

  cert_buf = (unsigned char*)malloc(RSA_KEY_LENGTH * 2);
  if (cert_buf == NULL) {
    LOGE("malloc failed");
    return -1;
  }

  ret = dtls_srtp_generate_keypair(dtls_srtp);
  if (ret != 0) {
    free(cert_buf);
    return ret;
  }

  mbedtls_x509write_crt_init(&crt);
  mbedtls_x509write_crt_set_subject_key(&crt, &dtls_srtp->pkey);
  mbedtls_x509write_crt_set_issuer_key(&crt, &dtls_srtp->pkey);
  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
  ret = mbedtls_x509write_crt_set_subject_name(&crt, "CN=dtls_srtp");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_subject_name failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }
  ret = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=dtls_srtp");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_issuer_name failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  if (psa_generate_random(serial_raw, sizeof(serial_raw)) != PSA_SUCCESS) {
    memset(serial_raw, 0xA5, sizeof(serial_raw));
  }
  ret = mbedtls_x509write_crt_set_serial_raw(&crt, serial_raw, sizeof(serial_raw));
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_serial_raw failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "20360101000000");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_validity failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509write_crt_pem(&crt, cert_buf, 2 * RSA_KEY_LENGTH);
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_pem failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509_crt_parse(&dtls_srtp->cert, cert_buf, strlen((char*)cert_buf) + 1);
  mbedtls_x509write_crt_free(&crt);
  free(cert_buf);
  if (ret != 0) {
    LOGE("mbedtls_x509_crt_parse failed -0x%.4x", (unsigned int)-ret);
  }
  return ret;
#else

  mbedtls_x509write_cert crt;

  unsigned char* cert_buf = NULL;
#if CONFIG_MBEDTLS_2_X
  mbedtls_mpi serial;
#else
  const char* serial = "peer";
#endif
  const char* pers = "dtls_srtp";

  cert_buf = (unsigned char*)malloc(RSA_KEY_LENGTH * 2);
  if (cert_buf == NULL) {
    LOGE("malloc failed");
    return -1;
  }

  ret = mbedtls_ctr_drbg_seed(&dtls_srtp->ctr_drbg,
                              mbedtls_entropy_func,
                              &dtls_srtp->entropy,
                              (const unsigned char*)pers,
                              strlen(pers));
  if (ret != 0) {
    free(cert_buf);
    LOGE("mbedtls_ctr_drbg_seed failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = dtls_srtp_generate_keypair(dtls_srtp);
  if (ret != 0) {
    free(cert_buf);
    return ret;
  }

  mbedtls_x509write_crt_init(&crt);

  mbedtls_x509write_crt_set_subject_key(&crt, &dtls_srtp->pkey);

  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);

  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

  mbedtls_x509write_crt_set_subject_key(&crt, &dtls_srtp->pkey);

  mbedtls_x509write_crt_set_issuer_key(&crt, &dtls_srtp->pkey);

  ret = mbedtls_x509write_crt_set_subject_name(&crt, "CN=dtls_srtp");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_subject_name failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509write_crt_set_issuer_name(&crt, "CN=dtls_srtp");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_issuer_name failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

#if CONFIG_MBEDTLS_2_X
  mbedtls_mpi_init(&serial);
  mbedtls_mpi_fill_random(&serial, 16, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);
  ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
  if (ret < 0) {
    LOGE("mbedtls_x509write_crt_set_serial failed -0x%.4x", (unsigned int)-ret);
  }
#else
  ret = mbedtls_x509write_crt_set_serial_raw(&crt, (unsigned char*)serial, strlen(serial));
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_serial_raw failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }
#endif

  ret = mbedtls_x509write_crt_set_validity(&crt, "20260101000000", "20360101000000");
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_set_validity failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509write_crt_pem(&crt, cert_buf, 2 * RSA_KEY_LENGTH, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);

  if (ret < 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509write_crt_pem failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  ret = mbedtls_x509_crt_parse(&dtls_srtp->cert, cert_buf, strlen((char*)cert_buf) + 1);
  if (ret != 0) {
    mbedtls_x509write_crt_free(&crt);
    free(cert_buf);
    LOGE("mbedtls_x509_crt_parse failed -0x%.4x", (unsigned int)-ret);
    return ret;
  }

  mbedtls_x509write_crt_free(&crt);

  free(cert_buf);

  return ret;
#endif
}

#if CONFIG_MBEDTLS_DEBUG
static void dtls_srtp_debug(void* ctx, int level, const char* file, int line, const char* str) {
  LOGD("%s:%04d: %s", file, line, str);
}
#endif

int dtls_srtp_init(DtlsSrtp* dtls_srtp, DtlsSrtpRole role, void* user_data) {
  int ret;
  static const mbedtls_ssl_srtp_profile default_profiles[] = {
      MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
      MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32,
      MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_80,
      MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_32,
      MBEDTLS_TLS_SRTP_UNSET};

  dtls_srtp->role = role;
  dtls_srtp->state = DTLS_SRTP_STATE_INIT;
  dtls_srtp->user_data = user_data;
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  dtls_srtp->psa_key_id = MBEDTLS_SVC_KEY_ID_INIT;
#endif
  dtls_srtp->udp_send = dtls_srtp_udp_send;
  dtls_srtp->udp_recv = dtls_srtp_udp_recv;

  mbedtls_ssl_config_init(&dtls_srtp->conf);
  mbedtls_ssl_init(&dtls_srtp->ssl);

  mbedtls_x509_crt_init(&dtls_srtp->cert);
  mbedtls_pk_init(&dtls_srtp->pkey);
  mbedtls_entropy_init(&dtls_srtp->entropy);
  mbedtls_ctr_drbg_init(&dtls_srtp->ctr_drbg);
  dtls_srtp->initialized = 1;

#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  if (psa_crypto_init() != PSA_SUCCESS) {
    LOGE("psa_crypto_init failed");
    return -1;
  }
#endif

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {
    ret = mbedtls_ssl_config_defaults(&dtls_srtp->conf,
                                      MBEDTLS_SSL_IS_SERVER,
                                      MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
      LOGE("mbedtls_ssl_config_defaults(server) failed -0x%.4x", (unsigned int)-ret);
      return -1;
    }

    mbedtls_ssl_cookie_init(&dtls_srtp->cookie_ctx);
#if MBEDTLS_VERSION_NUMBER >= 0x04000000
    ret = mbedtls_ssl_cookie_setup(&dtls_srtp->cookie_ctx);
#else
    ret = mbedtls_ssl_cookie_setup(&dtls_srtp->cookie_ctx, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);
#endif
    if (ret != 0) {
      LOGE("mbedtls_ssl_cookie_setup failed -0x%.4x", (unsigned int)-ret);
      return -1;
    }

    mbedtls_ssl_conf_dtls_cookies(&dtls_srtp->conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &dtls_srtp->cookie_ctx);

  } else {
    ret = mbedtls_ssl_config_defaults(&dtls_srtp->conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
      LOGE("mbedtls_ssl_config_defaults(client) failed -0x%.4x", (unsigned int)-ret);
      return -1;
    }
  }

#if CONFIG_MBEDTLS_DEBUG
  mbedtls_debug_set_threshold(3);
  mbedtls_ssl_conf_dbg(&dtls_srtp->conf, dtls_srtp_debug, NULL);
#endif

  ret = dtls_srtp_selfsign_cert(dtls_srtp);
  if (ret != 0) {
    LOGE("dtls_srtp_selfsign_cert failed -0x%.4x", (unsigned int)-ret);
    return -1;
  }

  mbedtls_ssl_conf_verify(&dtls_srtp->conf, dtls_srtp_cert_verify, NULL);
  /*
   * WebRTC peers use self-signed certificates and authenticate them with the
   * fingerprint carried in SDP.  VERIFY_REQUIRED makes Mbed TLS 4.x require a
   * TLS hostname as well, which does not exist for DTLS-SRTP peers.  Request
   * and retain the peer certificate here, then verify its SHA-256 fingerprint
   * after the handshake below.
   */
  mbedtls_ssl_conf_authmode(&dtls_srtp->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_ca_chain(&dtls_srtp->conf, &dtls_srtp->cert, NULL);

  ret = mbedtls_ssl_conf_own_cert(&dtls_srtp->conf, &dtls_srtp->cert, &dtls_srtp->pkey);
  if (ret != 0) {
    LOGE("mbedtls_ssl_conf_own_cert failed -0x%.4x", (unsigned int)-ret);
    return -1;
  }

#if MBEDTLS_VERSION_NUMBER < 0x04000000
  mbedtls_ssl_conf_rng(&dtls_srtp->conf, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);
#endif
  mbedtls_ssl_conf_read_timeout(&dtls_srtp->conf, 1000);

  dtls_srtp_x509_digest(&dtls_srtp->cert, dtls_srtp->local_fingerprint);

  LOGD("local fingerprint: %s", dtls_srtp->local_fingerprint);

  ret = mbedtls_ssl_conf_dtls_srtp_protection_profiles(&dtls_srtp->conf, default_profiles);
  if (ret != 0) {
    LOGE("mbedtls_ssl_conf_dtls_srtp_protection_profiles failed -0x%.4x", (unsigned int)-ret);
    return -1;
  }

  mbedtls_ssl_conf_srtp_mki_value_supported(&dtls_srtp->conf, MBEDTLS_SSL_DTLS_SRTP_MKI_UNSUPPORTED);

  mbedtls_ssl_conf_cert_req_ca_list(&dtls_srtp->conf, MBEDTLS_SSL_CERT_REQ_CA_LIST_DISABLED);

  ret = mbedtls_ssl_setup(&dtls_srtp->ssl, &dtls_srtp->conf);
  if (ret != 0) {
    LOGE("mbedtls_ssl_setup failed -0x%.4x", (unsigned int)-ret);
    return -1;
  }

  return 0;
}

void dtls_srtp_deinit(DtlsSrtp* dtls_srtp) {
  if (!dtls_srtp->initialized) {
    return;
  }

  mbedtls_ssl_free(&dtls_srtp->ssl);
  mbedtls_ssl_config_free(&dtls_srtp->conf);

  mbedtls_x509_crt_free(&dtls_srtp->cert);
  mbedtls_pk_free(&dtls_srtp->pkey);
  mbedtls_entropy_free(&dtls_srtp->entropy);
  mbedtls_ctr_drbg_free(&dtls_srtp->ctr_drbg);

#if MBEDTLS_VERSION_NUMBER >= 0x04000000
  if (dtls_srtp->psa_key_id != MBEDTLS_SVC_KEY_ID_INIT) {
    psa_destroy_key(dtls_srtp->psa_key_id);
    dtls_srtp->psa_key_id = MBEDTLS_SVC_KEY_ID_INIT;
  }
#endif

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {
    mbedtls_ssl_cookie_free(&dtls_srtp->cookie_ctx);
  }

  if (dtls_srtp->state == DTLS_SRTP_STATE_CONNECTED) {
    srtp_dealloc(dtls_srtp->srtp_in);
    srtp_dealloc(dtls_srtp->srtp_out);
  }

  dtls_srtp->initialized = 0;
}

static int dtls_srtp_key_derivation(DtlsSrtp* dtls_srtp, const unsigned char* master_secret, size_t secret_len, const unsigned char* randbytes, size_t randbytes_len, mbedtls_tls_prf_types tls_prf_type) {
  int ret;
  const char* dtls_srtp_label = "EXTRACTOR-dtls_srtp";
  uint8_t key_material[DTLS_SRTP_KEY_MATERIAL_LENGTH];
  // Export keying material
  if ((ret = mbedtls_ssl_tls_prf(tls_prf_type, master_secret, secret_len, dtls_srtp_label,
                                 randbytes, randbytes_len, key_material, sizeof(key_material))) != 0) {
    LOGE("mbedtls_ssl_tls_prf failed(%d)", ret);
    return ret;
  }

#if 0
  int i, j;
  printf("    DTLS-SRTP key material is:");
  for (j = 0; j < sizeof(key_material); j++) {
    if (j % 8 == 0) {
      printf("\n    ");
    }
    printf("%02x ", key_material[j]);
  }
  printf("\n");

  /* produce a less readable output used to perform automatic checks
   * - compare client and server output
   * - interop test with openssl which client produces this kind of output
   */
  printf("    Keying material: ");
  for (j = 0; j < sizeof(key_material); j++) {
    printf("%02X", key_material[j]);
  }
  printf("\n");
#endif

  const uint8_t* client_key = key_material;
  const uint8_t* server_key = client_key + SRTP_MASTER_KEY_LENGTH;
  const uint8_t* client_salt = server_key + SRTP_MASTER_KEY_LENGTH;
  const uint8_t* server_salt = client_salt + SRTP_MASTER_SALT_LENGTH;
  const uint8_t *local_key, *remote_key, *local_salt, *remote_salt;
  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {
    local_key = server_key;
    local_salt = server_salt;
    remote_key = client_key;
    remote_salt = client_salt;
  } else {
    local_key = client_key;
    local_salt = client_salt;
    remote_key = server_key;
    remote_salt = server_salt;
  }
  // derive inbounds keys

  memset(&dtls_srtp->remote_policy, 0, sizeof(dtls_srtp->remote_policy));

  srtp_crypto_policy_set_rtp_default(&dtls_srtp->remote_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&dtls_srtp->remote_policy.rtcp);

  memcpy(dtls_srtp->remote_policy_key, remote_key, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_srtp->remote_policy_key + SRTP_MASTER_KEY_LENGTH, remote_salt, SRTP_MASTER_SALT_LENGTH);

  dtls_srtp->remote_policy.ssrc.type = ssrc_any_inbound;
  dtls_srtp->remote_policy.key = dtls_srtp->remote_policy_key;
  dtls_srtp->remote_policy.next = NULL;

  if (srtp_create(&dtls_srtp->srtp_in, &dtls_srtp->remote_policy) != srtp_err_status_ok) {
    LOGD("Error creating inbound SRTP session for component");
    return -1;
  }

  LOGI("Created inbound SRTP session");

  // derive outbounds keys
  memset(&dtls_srtp->local_policy, 0, sizeof(dtls_srtp->local_policy));

  srtp_crypto_policy_set_rtp_default(&dtls_srtp->local_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&dtls_srtp->local_policy.rtcp);

  memcpy(dtls_srtp->local_policy_key, local_key, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_srtp->local_policy_key + SRTP_MASTER_KEY_LENGTH, local_salt, SRTP_MASTER_SALT_LENGTH);

  dtls_srtp->local_policy.ssrc.type = ssrc_any_outbound;
  dtls_srtp->local_policy.key = dtls_srtp->local_policy_key;
  dtls_srtp->local_policy.next = NULL;

  if (srtp_create(&dtls_srtp->srtp_out, &dtls_srtp->local_policy) != srtp_err_status_ok) {
    LOGE("Error creating outbound SRTP session");
    return -1;
  }

  LOGI("Created outbound SRTP session");
  return 0;
}

#if CONFIG_MBEDTLS_2_X
static int dtls_srtp_key_derivation_cb(void* context,
                                       const unsigned char* ms,
                                       const unsigned char* kb,
                                       size_t maclen,
                                       size_t keylen,
                                       size_t ivlen,
                                       const unsigned char client_random[32],
                                       const unsigned char server_random[32],
                                       mbedtls_tls_prf_types tls_prf_type) {
#else
static void dtls_srtp_key_derivation_cb(void* context,
                                        mbedtls_ssl_key_export_type secret_type,
                                        const unsigned char* secret,
                                        size_t secret_len,
                                        const unsigned char client_random[32],
                                        const unsigned char server_random[32],
                                        mbedtls_tls_prf_types tls_prf_type) {
#endif
  DtlsSrtp* dtls_srtp = (DtlsSrtp*)context;

  unsigned char master_secret[48];
  unsigned char randbytes[64];

  memcpy(randbytes, client_random, 32);
  memcpy(randbytes + 32, server_random, 32);

#if CONFIG_MBEDTLS_2_X
  memcpy(master_secret, ms, sizeof(master_secret));
  return dtls_srtp_key_derivation(dtls_srtp, master_secret, sizeof(master_secret), randbytes, sizeof(randbytes), tls_prf_type);
#else
  memcpy(master_secret, secret, sizeof(master_secret));
  dtls_srtp_key_derivation(dtls_srtp, master_secret, sizeof(master_secret), randbytes, sizeof(randbytes), tls_prf_type);
#endif
}

static int dtls_srtp_do_handshake(DtlsSrtp* dtls_srtp) {
  int ret;

  static mbedtls_timing_delay_context timer;

  mbedtls_ssl_set_timer_cb(&dtls_srtp->ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

#if CONFIG_MBEDTLS_2_X
  mbedtls_ssl_conf_export_keys_ext_cb(&dtls_srtp->conf, dtls_srtp_key_derivation_cb, dtls_srtp);
#else
  mbedtls_ssl_set_export_keys_cb(&dtls_srtp->ssl, dtls_srtp_key_derivation_cb, dtls_srtp);
#endif

  mbedtls_ssl_set_bio(&dtls_srtp->ssl, dtls_srtp, dtls_srtp->udp_send, dtls_srtp->udp_recv, NULL);

  do {
    ret = mbedtls_ssl_handshake(&dtls_srtp->ssl);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  return ret;
}

static int dtls_srtp_handshake_server(DtlsSrtp* dtls_srtp) {
  int ret;

  while (1) {
    unsigned char client_ip[] = "test";

    ret = mbedtls_ssl_session_reset(&dtls_srtp->ssl);
    if (ret != 0) {
      LOGE("mbedtls_ssl_session_reset failed -0x%.4x", (unsigned int)-ret);
      break;
    }

    ret = mbedtls_ssl_set_client_transport_id(&dtls_srtp->ssl, client_ip, sizeof(client_ip));
    if (ret != 0) {
      LOGE("mbedtls_ssl_set_client_transport_id failed -0x%.4x", (unsigned int)-ret);
      break;
    }

    ret = dtls_srtp_do_handshake(dtls_srtp);

    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {
      LOGD("DTLS hello verification requested");

    } else if (ret != 0) {
      LOGE("failed! mbedtls_ssl_handshake returned -0x%.4x", (unsigned int)-ret);

      break;

    } else {
      break;
    }
  }

  LOGD("DTLS server handshake done");

  return ret;
}

static int dtls_srtp_handshake_client(DtlsSrtp* dtls_srtp) {
  int ret;

  ret = dtls_srtp_do_handshake(dtls_srtp);
  if (ret != 0) {
    LOGE("failed! mbedtls_ssl_handshake returned -0x%.4x\n\n", (unsigned int)-ret);
  }

  LOGD("DTLS client handshake done");

  return ret;
}

int dtls_srtp_handshake(DtlsSrtp* dtls_srtp, Address* addr, const char* remote_fingerprint) {
  int ret;
  dtls_srtp->remote_addr = addr;
  if (dtls_srtp->state != DTLS_SRTP_STATE_INIT) {
    LOGE("DTLS-SRTP already initialized");
    return -1;
  }

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {
    ret = dtls_srtp_handshake_server(dtls_srtp);
  } else {
    ret = dtls_srtp_handshake_client(dtls_srtp);
  }

  if (ret != 0) {
    return ret;
  }

  const mbedtls_x509_crt* remote_crt;
  if ((remote_crt = mbedtls_ssl_get_peer_cert(&dtls_srtp->ssl)) != NULL) {
    dtls_srtp_x509_digest(remote_crt, dtls_srtp->actual_remote_fingerprint);

    if (strncmp(remote_fingerprint, dtls_srtp->actual_remote_fingerprint,
                DTLS_SRTP_FINGERPRINT_LENGTH) != 0) {
      LOGE("Actual and Expected Fingerprint mismatch: %s %s",
           remote_fingerprint,
           dtls_srtp->actual_remote_fingerprint);
      return -1;
    }

  } else {
    LOGE("no remote fingerprint");
    return -1;
  }

  dtls_srtp->state = DTLS_SRTP_STATE_CONNECTED;

  mbedtls_dtls_srtp_info dtls_srtp_negotiation_result;
  mbedtls_ssl_get_dtls_srtp_negotiation_result(&dtls_srtp->ssl, &dtls_srtp_negotiation_result);

  return ret;
}

int dtls_srtp_write(DtlsSrtp* dtls_srtp, const unsigned char* buf, size_t len) {
  int ret;

  do {
    ret = mbedtls_ssl_write(&dtls_srtp->ssl, buf, len);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
  return ret;
}

int dtls_srtp_read(DtlsSrtp* dtls_srtp, unsigned char* buf, size_t len) {
  int ret;

  memset(buf, 0, len);

  do {
    ret = mbedtls_ssl_read(&dtls_srtp->ssl, buf, len);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  return ret;
}

int dtls_srtp_probe(uint8_t* buf) {
  if (buf == NULL)
    return 0;

  LOGD("DTLS content type: %d", buf[0]);
  // only handle application data
  return (buf[0] == 0x17);
}

void dtls_srtp_decrypt_rtp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes) {
  srtp_unprotect(dtls_srtp->srtp_in, packet, bytes);
}

void dtls_srtp_decrypt_rtcp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes) {
  srtp_unprotect_rtcp(dtls_srtp->srtp_in, packet, bytes);
}

int dtls_srtp_encrypt_rtp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes) {
  return (int)srtp_protect(dtls_srtp->srtp_out, packet, bytes);
}

void dtls_srtp_encrypt_rctp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes) {
  srtp_protect_rtcp(dtls_srtp->srtp_out, packet, bytes);
}
