#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/socket.h"
#include "platform/misc.h"

#include "mbedtls/ssl.h"
#include "dtls_srtp.h"
#include "address.h"
#include "udp.h"
#include "config.h"
#include "utils.h"

typedef struct DtlsHeader DtlsHeader;

struct DtlsHeader {

  uint8_t content_type;
  uint16_t version;
  uint16_t epoch;
  uint32_t seqnum;
  uint16_t seqnum_hi;
  uint16_t length;

}__attribute__((packed));


int dtls_srtp_udp_send(void *ctx, const uint8_t *buf, size_t len) {

  DtlsSrtp *dtls_srtp = (DtlsSrtp *) ctx;
  UdpSocket *udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret = udp_socket_sendto(udp_socket, dtls_srtp->remote_addr, buf, len);

  LOGD("dtls_srtp_udp_send (%d)", ret);

  return ret;
}

int dtls_srtp_udp_recv(void *ctx, uint8_t *buf, size_t len) {

  DtlsSrtp *dtls_srtp = (DtlsSrtp *) ctx;
  UdpSocket *udp_socket = (UdpSocket*)dtls_srtp->user_data;

  int ret;

  while ((ret = udp_socket_recvfrom(udp_socket, &udp_socket->bind_addr, buf, len)) <= 0) {

    platform_sleep_ms(1);
  }

  LOGD("dtls_srtp_udp_recv (%d)", ret);

  return ret;
}

static void dtls_srtp_x509_digest(const mbedtls_x509_crt *crt, char *buf) {

  int i;
  unsigned char digest[32];

  mbedtls_sha256_context sha256_ctx;
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts(&sha256_ctx, 0);
  mbedtls_sha256_update(&sha256_ctx, crt->raw.p, crt->raw.len);
  mbedtls_sha256_finish(&sha256_ctx, (unsigned char *) digest);
  mbedtls_sha256_free(&sha256_ctx);

  for(i = 0; i < 32; i++) {

    snprintf(buf, 4, "%.2X:", digest[i]);
    buf += 3;
  }

  *(--buf) = '\0';
}

// Do not verify CA
static int dtls_srtp_cert_verify(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {

  *flags &= ~(MBEDTLS_X509_BADCERT_NOT_TRUSTED | MBEDTLS_X509_BADCERT_CN_MISMATCH);
  return 0;
}

static int dtls_srtp_selfsign_cert(DtlsSrtp *dtls_srtp) {

  int ret;

  mbedtls_x509write_cert crt;

  mbedtls_mpi serial;

  unsigned char *cert_buf = (unsigned char *) malloc(RSA_KEY_LENGTH * 2);

  const char *pers = "dtls_srtp";

  mbedtls_ctr_drbg_seed(&dtls_srtp->ctr_drbg, mbedtls_entropy_func, &dtls_srtp->entropy, (const unsigned char *) pers, strlen(pers));

  mbedtls_pk_setup(&dtls_srtp->pkey, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));

  mbedtls_rsa_gen_key(mbedtls_pk_rsa(dtls_srtp->pkey), mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg, RSA_KEY_LENGTH, 65537);

  mbedtls_x509write_crt_init(&crt);

  mbedtls_x509write_crt_set_subject_key(&crt, &dtls_srtp->pkey);

  mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);

  mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);

  mbedtls_x509write_crt_set_subject_key(&crt, &dtls_srtp->pkey);

  mbedtls_x509write_crt_set_issuer_key(&crt, &dtls_srtp->pkey);

  mbedtls_x509write_crt_set_subject_name(&crt, "CN=dtls_srtp");

  mbedtls_x509write_crt_set_issuer_name(&crt, "CN=dtls_srtp");

  mbedtls_mpi_init(&serial);

  mbedtls_mpi_fill_random(&serial, 16, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);

  mbedtls_x509write_crt_set_serial(&crt, &serial);

  mbedtls_x509write_crt_set_validity(&crt, "20180101000000", "20280101000000");

  ret = mbedtls_x509write_crt_pem(&crt, cert_buf, 2*RSA_KEY_LENGTH, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);

  if (ret < 0) {

    printf("mbedtls_x509write_crt_pem failed\n");
  }

  mbedtls_x509_crt_parse(&dtls_srtp->cert, cert_buf, 2*RSA_KEY_LENGTH);

  mbedtls_x509write_crt_free(&crt);

  mbedtls_mpi_free(&serial);

  free(cert_buf);

  return ret;
}

#ifdef DTLS_SRTP_DEBUG_MBEDTLS
static void dtls_srtp_mbedtls_debug_cb(void* ctx, int level, const char* file, int line, const char* str) {
    const char *p, *basename;
    (void) ctx;

    /* Extract basename from file */
    for(p = basename = file; *p != '\0'; p++) {
        if(*p == '/' || *p == '\\') {
            basename = p + 1;
        }
    }

    printf("mbedtls %s:%04d: |%d| %s", basename, line, level, str);
}
#endif

int dtls_srtp_init(DtlsSrtp *dtls_srtp, DtlsSrtpRole role, void *user_data) {

  static const mbedtls_ssl_srtp_profile default_profiles[] = {
   MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
   MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32,
   MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_80,
   MBEDTLS_TLS_SRTP_NULL_HMAC_SHA1_32,
   MBEDTLS_TLS_SRTP_UNSET
  };

  dtls_srtp->role = role;
  dtls_srtp->state = DTLS_SRTP_STATE_INIT;
  dtls_srtp->user_data = user_data;
  dtls_srtp->udp_send = dtls_srtp_udp_send;
  dtls_srtp->udp_recv = dtls_srtp_udp_recv;

  mbedtls_ssl_config_init(&dtls_srtp->conf);
  mbedtls_ssl_init(&dtls_srtp->ssl);

  mbedtls_x509_crt_init(&dtls_srtp->cert);
  mbedtls_pk_init(&dtls_srtp->pkey);
  mbedtls_entropy_init(&dtls_srtp->entropy);
  mbedtls_ctr_drbg_init(&dtls_srtp->ctr_drbg);

  dtls_srtp_selfsign_cert(dtls_srtp);

#ifdef DTLS_SRTP_DEBUG_MBEDTLS
  mbedtls_ssl_conf_dbg(&dtls_srtp->conf, dtls_srtp_mbedtls_debug_cb, NULL);
#endif

// XXX: Not sure if this is needed
#if 0
  mbedtls_ssl_conf_verify(&dtls_srtp->conf, dtls_srtp_cert_verify, NULL);

  mbedtls_ssl_conf_authmode(&dtls_srtp->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
#endif

  mbedtls_ssl_conf_ca_chain(&dtls_srtp->conf, &dtls_srtp->cert, NULL);

  mbedtls_ssl_conf_own_cert(&dtls_srtp->conf, &dtls_srtp->cert, &dtls_srtp->pkey);

  mbedtls_ssl_conf_rng(&dtls_srtp->conf, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);

  mbedtls_ssl_conf_read_timeout(&dtls_srtp->conf, 1000);

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {

    mbedtls_ssl_config_defaults(&dtls_srtp->conf,
     MBEDTLS_SSL_IS_SERVER,
     MBEDTLS_SSL_TRANSPORT_DATAGRAM,
     MBEDTLS_SSL_PRESET_DEFAULT);

    mbedtls_ssl_cookie_init(&dtls_srtp->cookie_ctx);

    mbedtls_ssl_cookie_setup(&dtls_srtp->cookie_ctx, mbedtls_ctr_drbg_random, &dtls_srtp->ctr_drbg);

    mbedtls_ssl_conf_dtls_cookies(&dtls_srtp->conf, mbedtls_ssl_cookie_write, mbedtls_ssl_cookie_check, &dtls_srtp->cookie_ctx);

  } else {

    mbedtls_ssl_config_defaults(&dtls_srtp->conf,
     MBEDTLS_SSL_IS_CLIENT,
     MBEDTLS_SSL_TRANSPORT_DATAGRAM,
     MBEDTLS_SSL_PRESET_DEFAULT);
  }

  dtls_srtp_x509_digest(&dtls_srtp->cert, dtls_srtp->local_fingerprint);

  LOGD("local fingerprint: %s", dtls_srtp->local_fingerprint);

  mbedtls_ssl_conf_dtls_srtp_protection_profiles(&dtls_srtp->conf, default_profiles);

  mbedtls_ssl_conf_srtp_mki_value_supported(&dtls_srtp->conf, MBEDTLS_SSL_DTLS_SRTP_MKI_UNSUPPORTED);

  mbedtls_ssl_setup(&dtls_srtp->ssl, &dtls_srtp->conf);

  return 0;
}

void dtls_srtp_deinit(DtlsSrtp *dtls_srtp) {

  mbedtls_ssl_free(&dtls_srtp->ssl);
  mbedtls_ssl_config_free(&dtls_srtp->conf);

  mbedtls_x509_crt_free(&dtls_srtp->cert);
  mbedtls_pk_free(&dtls_srtp->pkey);
  mbedtls_entropy_free(&dtls_srtp->entropy);
  mbedtls_ctr_drbg_free(&dtls_srtp->ctr_drbg);

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {

    mbedtls_ssl_cookie_free(&dtls_srtp->cookie_ctx);
  }

  if (dtls_srtp->state == DTLS_SRTP_STATE_CONNECTED) {

    srtp_dealloc(dtls_srtp->srtp_in);
    srtp_dealloc(dtls_srtp->srtp_out);
  }
}

static void dtls_srtp_key_derivation(void *context, mbedtls_ssl_key_export_type secret_type,
 const unsigned char *secret, size_t secret_len,
 const unsigned char client_random[32],
 const unsigned char server_random[32],
 mbedtls_tls_prf_types tls_prf_type) {

  DtlsSrtp *dtls_srtp = (DtlsSrtp *) context;

  int ret;

  const char *dtls_srtp_label = "EXTRACTOR-dtls_srtp";

  unsigned char randbytes[64];

  uint8_t key_material[DTLS_SRTP_KEY_MATERIAL_LENGTH];

  memcpy(randbytes, client_random, 32);
  memcpy(randbytes + 32, server_random, 32);

  // Export keying material
  if ((ret = mbedtls_ssl_tls_prf(tls_prf_type, secret, secret_len, dtls_srtp_label,
   randbytes, sizeof(randbytes), key_material, sizeof(key_material))) != 0) {

    LOGE("mbedtls_ssl_tls_prf failed(%d)", ret);
    return;
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

  // derive inbounds keys

  memset(&dtls_srtp->remote_policy, 0, sizeof(dtls_srtp->remote_policy));

  srtp_crypto_policy_set_rtp_default(&dtls_srtp->remote_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&dtls_srtp->remote_policy.rtcp);

  memcpy(dtls_srtp->remote_policy_key, key_material, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_srtp->remote_policy_key + SRTP_MASTER_KEY_LENGTH, key_material + SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_KEY_LENGTH, SRTP_MASTER_SALT_LENGTH);

  dtls_srtp->remote_policy.ssrc.type = ssrc_any_inbound;
  dtls_srtp->remote_policy.key = dtls_srtp->remote_policy_key;
  dtls_srtp->remote_policy.next = NULL;

  if (srtp_create(&dtls_srtp->srtp_in, &dtls_srtp->remote_policy) != srtp_err_status_ok) {

    LOGD("Error creating inbound SRTP session for component");
    return;
  }

  LOGI("Created inbound SRTP session");

  // derive outbounds keys
  memset(&dtls_srtp->local_policy, 0, sizeof(dtls_srtp->local_policy));

  srtp_crypto_policy_set_rtp_default(&dtls_srtp->local_policy.rtp);
  srtp_crypto_policy_set_rtcp_default(&dtls_srtp->local_policy.rtcp);

  memcpy(dtls_srtp->local_policy_key, key_material + SRTP_MASTER_KEY_LENGTH, SRTP_MASTER_KEY_LENGTH);
  memcpy(dtls_srtp->local_policy_key + SRTP_MASTER_KEY_LENGTH, key_material + SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH, SRTP_MASTER_SALT_LENGTH);

  dtls_srtp->local_policy.ssrc.type = ssrc_any_outbound;
  dtls_srtp->local_policy.key = dtls_srtp->local_policy_key;
  dtls_srtp->local_policy.next = NULL;

  if (srtp_create(&dtls_srtp->srtp_out, &dtls_srtp->local_policy) != srtp_err_status_ok) {

    LOGE("Error creating outbound SRTP session");
    return;
  }

  LOGI("Created outbound SRTP session");
  dtls_srtp->state = DTLS_SRTP_STATE_CONNECTED;
}

static int dtls_srtp_do_handshake(DtlsSrtp *dtls_srtp) {

  int ret;

  static mbedtls_timing_delay_context timer;

  mbedtls_ssl_set_timer_cb(&dtls_srtp->ssl, &timer, mbedtls_timing_set_delay, mbedtls_timing_get_delay);

  mbedtls_ssl_set_export_keys_cb(&dtls_srtp->ssl, dtls_srtp_key_derivation, dtls_srtp);

  mbedtls_ssl_set_bio(&dtls_srtp->ssl, dtls_srtp, dtls_srtp->udp_send, dtls_srtp->udp_recv, NULL);

  do {

    ret = mbedtls_ssl_handshake(&dtls_srtp->ssl);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  return ret;
}

static int dtls_srtp_handshake_server(DtlsSrtp *dtls_srtp) {

  int ret;

  while (1) {

    unsigned char client_ip[] = "test";

    mbedtls_ssl_session_reset(&dtls_srtp->ssl);

    mbedtls_ssl_set_client_transport_id(&dtls_srtp->ssl, client_ip, sizeof(client_ip));

    ret = dtls_srtp_do_handshake(dtls_srtp);

    if (ret == MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED) {

      LOGD("DTLS hello verification requested");

    } else if (ret != 0) {

      LOGE("failed! mbedtls_ssl_handshake returned -0x%.4x", (unsigned int) -ret);

      break;

    } else {

      break;
    }
  }

  LOGD("DTLS server handshake done");

  return ret;
}

static int dtls_srtp_handshake_client(DtlsSrtp *dtls_srtp) {

  int ret;

  ret = dtls_srtp_do_handshake(dtls_srtp);

  if (ret != 0) {

    LOGE("failed! mbedtls_ssl_handshake returned -0x%.4x\n\n", (unsigned int) -ret);
  }

  int flags;

  if ((flags = mbedtls_ssl_get_verify_result(&dtls_srtp->ssl)) != 0) {
#if !defined(MBEDTLS_X509_REMOVE_INFO)
    char vrfy_buf[512];
#endif

    printf(" failed\n");

#if !defined(MBEDTLS_X509_REMOVE_INFO)
    mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);

    printf("%s\n", vrfy_buf);
#endif
  }

  LOGD("DTLS client handshake done");

  return ret;
}


int dtls_srtp_handshake(DtlsSrtp *dtls_srtp, Address *addr) {

  int ret;

  dtls_srtp->remote_addr = addr;

  if (dtls_srtp->role == DTLS_SRTP_ROLE_SERVER) {

    ret = dtls_srtp_handshake_server(dtls_srtp);

  } else {

    ret = dtls_srtp_handshake_client(dtls_srtp);

  }

// XXX: Not sure if this is needed
#if 0
  const mbedtls_x509_crt *remote_crt;
  if ((remote_crt = mbedtls_ssl_get_peer_cert(&dtls_srtp->ssl)) != NULL) {

    dtls_srtp_x509_digest(remote_crt, dtls_srtp->remote_fingerprint);

    LOGD("remote fingerprint: %s", dtls_srtp->remote_fingerprint);

  } else {

    LOGE("no remote fingerprint");
  }
#endif

  mbedtls_dtls_srtp_info dtls_srtp_negotiation_result;
  mbedtls_ssl_get_dtls_srtp_negotiation_result(&dtls_srtp->ssl, &dtls_srtp_negotiation_result);

  return ret;
}

void dtls_srtp_reset_session(DtlsSrtp *dtls_srtp) {

  if (dtls_srtp->state == DTLS_SRTP_STATE_CONNECTED) {

    srtp_dealloc(dtls_srtp->srtp_in);
    srtp_dealloc(dtls_srtp->srtp_out);
    mbedtls_ssl_session_reset(&dtls_srtp->ssl);
  }

  dtls_srtp->state = DTLS_SRTP_STATE_INIT;
}

int dtls_srtp_write(DtlsSrtp *dtls_srtp, const unsigned char *buf, size_t len) {

  int ret;

  do {

    ret = mbedtls_ssl_write(&dtls_srtp->ssl, buf, len);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);
  return ret;
}

int dtls_srtp_read(DtlsSrtp *dtls_srtp, unsigned char *buf, size_t len) {

  int ret;

  memset(buf, 0, len);

  do {

    ret = mbedtls_ssl_read(&dtls_srtp->ssl, buf, len);

  } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

  return ret;
}

int dtls_srtp_probe(uint8_t *buf) {

  if(buf == NULL)
    return 0;

  //LOGD("DTLS content type: %d, version: %d, epoch: %d, sequence: %d, length: %d (%.4x)", header->content_type, header->version, header->epoch, ntohs(header->seqnum_hi), ntohs(header->length), header->length);

  return ((*buf >= 20) && (*buf <= 64));
}

void dtls_srtp_decrypt_rtp_packet(DtlsSrtp *dtls_srtp, uint8_t *packet, int *bytes) {

  srtp_unprotect(dtls_srtp->srtp_in, packet, bytes);
}

void dtls_srtp_decrypt_rtcp_packet(DtlsSrtp *dtls_srtp, uint8_t *packet, int *bytes) {

  srtp_unprotect_rtcp(dtls_srtp->srtp_in, packet, bytes);
}

void dtls_srtp_encrypt_rtp_packet(DtlsSrtp *dtls_srtp, uint8_t *packet, int *bytes) {

  srtp_protect(dtls_srtp->srtp_out, packet, bytes);
}

void dtls_srtp_encrypt_rctp_packet(DtlsSrtp *dtls_srtp, uint8_t *packet, int *bytes) {

  srtp_protect_rtcp(dtls_srtp->srtp_out, packet, bytes);
}
