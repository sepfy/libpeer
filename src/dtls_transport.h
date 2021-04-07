#ifndef DTLS_TRANSPORT_H_
#define DTLS_TRANSPORT_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <srtp.h> 

typedef struct dtls_transport_t {

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

  char fingerprint[160];

} dtls_transport_t;

int dtls_transport_is_dtls(char *buf);
int dtls_transport_init(dtls_transport_t *dtls_transport, BIO *agent_write_bio);
void dtls_transport_incomming_msg(dtls_transport_t *dtls_transport, char *buf, int len);
void dtls_transport_do_handshake(dtls_transport_t *dtls_transport);
void dtls_transport_encrypt_rtp_packet(dtls_transport_t *dtls_transport, uint8_t *packet, int *bytes);

#endif // DTLS_TRANSPORT_H_
