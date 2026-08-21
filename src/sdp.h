#ifndef SDP_H_
#define SDP_H_

#include <stdint.h>
#include <string.h>
#include "config.h"

#define SDP_ATTR_LENGTH 128

#ifndef ICE_LITE
#define ICE_LITE 0
#endif

void sdp_append_h264(char* sdp, uint8_t payload_type, uint32_t ssrc,
                     const char* mid, const char* fmtp, int send_only,
                     const char* stream_id, const char* track_id);

void sdp_append_vp8(char* sdp);

void sdp_append_pcma(char* sdp);

void sdp_append_pcmu(char* sdp);

void sdp_append_opus(char* sdp);

void sdp_append_datachannel(char* sdp);

void sdp_create(char* sdp, int b_video, int b_audio, int b_datachannel,
                const char* video_mid);

int sdp_append(char* sdp, const char* format, ...);

void sdp_reset(char* sdp);

#endif  // SDP_H_
