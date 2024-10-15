#ifndef DTLS_SRTP_H_
#define DTLS_SRTP_H_

#include <stdio.h>
#include <stdlib.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_cookie.h>
#include <mbedtls/timing.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509_csr.h>

#include <srtp2/srtp.h>

#include "address.h"

#define SRTP_MASTER_KEY_LENGTH 16
#define SRTP_MASTER_SALT_LENGTH 14
#define DTLS_SRTP_KEY_MATERIAL_LENGTH 60
#define DTLS_SRTP_FINGERPRINT_LENGTH 160

typedef enum DtlsSrtpRole {

  DTLS_SRTP_ROLE_CLIENT,
  DTLS_SRTP_ROLE_SERVER

} DtlsSrtpRole;

typedef enum DtlsSrtpState {

  DTLS_SRTP_STATE_INIT,
  DTLS_SRTP_STATE_HANDSHAKE,
  DTLS_SRTP_STATE_CONNECTED

} DtlsSrtpState;

typedef struct DtlsSrtp {
  // MbedTLS
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_ssl_cookie_ctx cookie_ctx;
  mbedtls_x509_crt cert;
  mbedtls_pk_context pkey;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;

  // SRTP
  srtp_policy_t remote_policy;
  srtp_policy_t local_policy;
  srtp_t srtp_in;
  srtp_t srtp_out;
  unsigned char remote_policy_key[SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH];
  unsigned char local_policy_key[SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH];

  int (*udp_send)(void* ctx, const unsigned char* buf, size_t len);
  int (*udp_recv)(void* ctx, unsigned char* buf, size_t len);

  Address* remote_addr;

  DtlsSrtpRole role;
  DtlsSrtpState state;

  char local_fingerprint[DTLS_SRTP_FINGERPRINT_LENGTH];
  char remote_fingerprint[DTLS_SRTP_FINGERPRINT_LENGTH];
  char actual_remote_fingerprint[DTLS_SRTP_FINGERPRINT_LENGTH];

  void* user_data;

} DtlsSrtp;

int dtls_srtp_init(DtlsSrtp* dtls_srtp, DtlsSrtpRole role, void* user_data);

void dtls_srtp_deinit(DtlsSrtp* dtls_srtp);

int dtls_srtp_create_cert(DtlsSrtp* dtls_srtp);

int dtls_srtp_handshake(DtlsSrtp* dtls_srtp, Address* addr);

void dtls_srtp_reset_session(DtlsSrtp* dtls_srtp);

int dtls_srtp_write(DtlsSrtp* dtls_srtp, const uint8_t* buf, size_t len);

int dtls_srtp_read(DtlsSrtp* dtls_srtp, uint8_t* buf, size_t len);

void dtls_srtp_sctp_to_dtls(DtlsSrtp* dtls_srtp, uint8_t* packet, int bytes);

int dtls_srtp_probe(uint8_t* buf);

void dtls_srtp_decrypt_rtp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes);

void dtls_srtp_decrypt_rtcp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes);

void dtls_srtp_encrypt_rtp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes);

void dtls_srtp_encrypt_rctp_packet(DtlsSrtp* dtls_srtp, uint8_t* packet, int* bytes);

#endif  // DTLS_SRTP_H_
