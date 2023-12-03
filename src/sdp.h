#ifndef SDP_H_
#define SDP_H_

#include <string.h>

#define SDP_CONTENT_LENGTH 10240
#define SDP_ATTR_LENGTH 128

typedef struct Sdp {

  char content[SDP_CONTENT_LENGTH];

} Sdp;

void sdp_append_h264(Sdp *sdp);

void sdp_append_pcma(Sdp *sdp);

void sdp_append_pcmu(Sdp *sdp);

void sdp_append_opus(Sdp *sdp);

void sdp_append_datachannel(Sdp *sdp);

void sdp_create(Sdp *sdp, int b_video, int b_audio, int b_datachannel);

int sdp_append(Sdp *sdp, const char *format, ...);

void sdp_reset(Sdp *sdp);

#endif // SDP_H_
