#ifndef SDP_ATTRIBUTE_H_
#define SDP_ATTRIBUTE_H_

#include <stdlib.h>

#define SDP_ATTIBUTES_MAX 10240

typedef enum transceiver_direction_t {

  SENDRECV,
  RECVONLY,
  SENDONLY,

} transceiver_direction_t;

typedef struct transceiver_t {

  transceiver_direction_t audio;
  transceiver_direction_t video;

} transceiver_t;

typedef struct sdp_attribute_t {

  size_t size;
  char attributes[SDP_ATTIBUTES_MAX];
  char answer[SDP_ATTIBUTES_MAX + 30];
} sdp_attribute_t;


sdp_attribute_t* sdp_attribute_create(void);

void sdp_attribute_destroy(sdp_attribute_t *sdp_attribute);

int sdp_attribute_append(sdp_attribute_t *sdp_attribute, const char *attribute, ...);

int sdp_attribute_update_mdns_of_candidate(char *candidate_src, char *candidate_dst, size_t size);

char* sdp_attribute_get_answer(sdp_attribute_t *sdp_attribute);

#endif // SDP_ATTRIBUTE_H_
