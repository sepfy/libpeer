#include <stdlib.h>
#include <stdio.h>
#include <openssl/ssl.h>
#include <agent.h>

#include "ice_agent_bio.h"
#include "ice_agent.h"

static int mtu = 1200;
static BIO_METHOD *ice_agent_bio_methods = NULL;

int ice_agent_bio_init(void) {
  ice_agent_bio_methods = BIO_meth_new(BIO_TYPE_BIO, "nice agent bio");
  if(!ice_agent_bio_methods) {
    return -1;
  }
  BIO_meth_set_write(ice_agent_bio_methods, ice_agent_bio_write);
  BIO_meth_set_ctrl(ice_agent_bio_methods, ice_agent_bio_ctrl);
  BIO_meth_set_create(ice_agent_bio_methods, ice_agent_bio_create);
  BIO_meth_set_destroy(ice_agent_bio_methods, ice_agent_bio_free);
  return 0;
}

BIO* ice_agent_bio_new(ice_agent_t *ice_agent) {
  ice_agent_bio_init();
  BIO* bio = BIO_new(ice_agent_bio_methods);
  if(bio == NULL) {
    return NULL;
  }
  BIO_set_data(bio, ice_agent);
  return bio;
}

static int ice_agent_bio_create(BIO *bio) {
  BIO_set_init(bio, 1);
  BIO_set_data(bio, NULL);
  BIO_set_shutdown(bio, 0);
  return 1;
}

static int ice_agent_bio_free(BIO *bio) {
  if(bio == NULL) {
    return 0;
  }
  BIO_set_data(bio, NULL);
  return 1;
}

static int ice_agent_bio_write(BIO *bio, const char *in, int inl) {
  if(inl <= 0) {
    return inl;
  }
  ice_agent_t *ice_agent;
  ice_agent = (ice_agent_t*)BIO_get_data(bio);
  if(ice_agent == NULL) {
    return -1;
  }

  int bytes = nice_agent_send(ice_agent->nice_agent,
   ice_agent->stream_id, ice_agent->component_id, inl, in);

  return bytes;
}

static long ice_agent_bio_ctrl(BIO *bio, int cmd, long num, void *ptr) {
  switch(cmd) {
    case BIO_CTRL_FLUSH:
      return 1;
    case BIO_CTRL_DGRAM_QUERY_MTU:
      return mtu;
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
      return 0L;
    default:
      break;
  }
  return 0;
}
