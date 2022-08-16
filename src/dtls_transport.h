/**
 * @file dtls_transport.h
 * @brief Struct DtlsTransport
 */

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

typedef struct DtlsTransport DtlsTransport;


DtlsTransport* dtls_transport_create(BIO *agent_write_bio);


void dtls_transport_destroy(DtlsTransport *dtls_transport);


int dtls_transport_validate(char *buf);


void dtls_transport_incomming_msg(DtlsTransport *dtls_transport, char *buf, int len);


void dtls_transport_do_handshake(DtlsTransport *dtls_transport);


void dtls_transport_encrypt_rtp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes);


void dtls_transport_decrypt_rtp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes);


void dtls_transport_decrypt_rtcp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes);


void dtls_transport_encrypt_rctp_packet(DtlsTransport *dtls_transport, uint8_t *packet, int *bytes);


const char* dtls_transport_get_fingerprint(DtlsTransport *dtls_transport);


int dtls_transport_get_srtp_initialized(DtlsTransport *dtls_transport);

int dtls_transport_sctp_to_dtls(DtlsTransport *dtls_transport, uint8_t *data, int bytes);

int dtls_transport_decrypt_data(DtlsTransport *dtls_transport, char *encrypted_data, int encrypted_len, char *decrypted_data, int decrypted_len);

#endif // DTLS_TRANSPORT_H_
