#ifndef NICE_AGENT_BIO_H_
#define NICE_AGENT_BIO_H_

#include <agent.h>
#include <openssl/ssl.h>

typedef struct NiceAgentBio NiceAgentBio;

BIO* nice_agent_bio_new(NiceAgent *nice_agent, guint stream_id, guint component_id);

#endif // NICE_AGENT_BIO_H_

