#ifndef SCTP_H_
#define SCTP_H_

#include "dtls_transport.h"

typedef struct Sctp Sctp;

Sctp* sctp_create(DtlsTransport *dtls_transport);

void sctp_destroy(Sctp *sctp);

int sctp_do_connect(Sctp *sctp);

int sctp_is_connected(Sctp *sctp);

void sctp_incoming_data(Sctp *sctp, char *buf, int len);

void sctp_onmessage(Sctp *sctp, void (*onmessasge)(char *msg, size_t len, void *userdata));

void sctp_onopen(Sctp *sctp, void (*onopen)(void *userdata));

void sctp_onclose(Sctp *sctp, void (*onclose)(void *userdata));

#endif // SCTP_H_
