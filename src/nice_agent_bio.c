#include <stdlib.h>
#include <stdio.h>

#include "nice_agent_bio.h"

struct NiceAgentBio {

  NiceAgent *nice_agent;
  guint stream_id;
  guint component_id;

};

static int nice_agent_bio_create(BIO *bio);

static int nice_agent_bio_write(BIO *bio, const char *buf, int num);

static long nice_agent_bio_ctrl(BIO *bio, int cmd, long arg1, void *arg2);

static int nice_agent_bio_free(BIO *data);

static BIO_METHOD *nice_agent_bio_methods = NULL;

int nice_agent_bio_init(void) {

  nice_agent_bio_methods = BIO_meth_new(BIO_TYPE_BIO, "nice agent bio");
  if(!nice_agent_bio_methods) {
    return -1;
  }

  BIO_meth_set_write(nice_agent_bio_methods, nice_agent_bio_write);
  BIO_meth_set_ctrl(nice_agent_bio_methods, nice_agent_bio_ctrl);
  BIO_meth_set_create(nice_agent_bio_methods, nice_agent_bio_create);
  BIO_meth_set_destroy(nice_agent_bio_methods, nice_agent_bio_free);

  return 0;

}

BIO* nice_agent_bio_new(NiceAgent *nice_agent, guint stream_id, guint component_id) {

  nice_agent_bio_init();

  BIO* bio = BIO_new(nice_agent_bio_methods);
  if(bio == NULL) {
    return NULL;
  }

  NiceAgentBio *nice_agent_bio = (NiceAgentBio*)malloc(sizeof(NiceAgentBio));
  nice_agent_bio->nice_agent = nice_agent;
  nice_agent_bio->stream_id = stream_id;
  nice_agent_bio->component_id = component_id;

  BIO_set_data(bio, nice_agent_bio);
  return bio;

}

static int nice_agent_bio_create(BIO *bio) {

  BIO_set_init(bio, 1);
  BIO_set_data(bio, NULL);
  BIO_set_shutdown(bio, 0);
  return 1;

}

static int nice_agent_bio_free(BIO *bio) {

  if(bio == NULL) {
    return 0;
  }

  NiceAgentBio *nice_agent_bio = (NiceAgentBio*)BIO_get_data(bio);
  if(nice_agent_bio)
    free(nice_agent_bio);

  BIO_set_data(bio, NULL);
  return 1;

}

static int nice_agent_bio_write(BIO *bio, const char *in, int inl) {

  if(inl <= 0) {
    return inl;
  }

  NiceAgentBio *nice_agent_bio = (NiceAgentBio*)BIO_get_data(bio);
  if(nice_agent_bio == NULL) {
    return -1;
  }

  int bytes = nice_agent_send(nice_agent_bio->nice_agent,
   nice_agent_bio->stream_id, nice_agent_bio->component_id, inl, in);

  return bytes;
}

static long nice_agent_bio_ctrl(BIO *bio, int cmd, long num, void *ptr) {

  switch(cmd) {
    case BIO_CTRL_FLUSH:
      return 1;
    case BIO_CTRL_DGRAM_QUERY_MTU:
      return 1200; // mtu
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
      return 0L;
    default:
      break;
  }
  return 0;

}
