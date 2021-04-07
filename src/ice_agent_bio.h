#ifndef ICE_AGENT_BIO_H_
#define ICE_AGENT_BIO_H_

#include <openssl/ssl.h>
#include "ice_agent.h"

BIO* ice_agent_bio_new(ice_agent_t *ice_agent);
static int ice_agent_bio_create(BIO *bio);
static int ice_agent_bio_write(BIO *bio, const char *buf, int num);
static long ice_agent_bio_ctrl(BIO *bio, int cmd, long arg1, void *arg2);
static int ice_agent_bio_free(BIO *data);

#endif // ICE_AGENT_BIO_H_

