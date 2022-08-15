#ifndef SCTP_H_
#define SCTP_H_

#include "dtls_transport.h"

typedef struct Sctp Sctp;

Sctp* sctp_create(DtlsTransport *dtls_transport);

void sctp_destroy(Sctp *sctp);

void sctp_incoming_data(Sctp *sctp, char *buf, int len);

#endif // SCTP_H_
