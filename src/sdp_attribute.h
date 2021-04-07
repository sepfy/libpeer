#ifndef SDP_ATTRIBUTE_H_
#define SDP_ATTRIBUTE_H_

#include <stdlib.h>

#define SDP_ATTIBUTES_MAX 10240

typedef struct sdp_attribute_t {

  size_t size;
  char attributes[SDP_ATTIBUTES_MAX];
  char answer[SDP_ATTIBUTES_MAX + 30];
} sdp_attribute_t;


sdp_attribute_t* sdp_attribute_create(void);

void sdp_attribute_free(sdp_attribute_t *sdp_attribute);

int sdp_attribute_append(sdp_attribute_t *sdp_attribute, const char *attribute, ...);

char* sdp_attribute_get_answer(sdp_attribute_t *sdp_attribute);

#endif // SDP_ATTRIBUTE_H_
