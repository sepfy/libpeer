#ifndef SDP_H_
#define SDP_H_

#include <string.h>
#include "config.h"

#define SDP_ATTR_LENGTH 128

#define SDP_MID_MAX_LEN 32
#define SDP_DATACHANNEL_PORT 50712

#ifndef ICE_LITE
#define ICE_LITE 0
#endif

typedef enum SdpDirection {
  SDP_DIR_NONE = 0,
  SDP_DIR_SENDRECV,
  SDP_DIR_SENDONLY,
  SDP_DIR_RECVONLY,
  SDP_DIR_INACTIVE,
} SdpDirection;

typedef enum SdpMediaKind {
  SDP_MEDIA_VIDEO = 0,
  SDP_MEDIA_AUDIO,
  SDP_MEDIA_APPLICATION,
  SDP_MEDIA_COUNT,
} SdpMediaKind;

typedef struct SdpMediaInfo {
  char mid[SDP_MID_MAX_LEN];
  SdpDirection direction;
} SdpMediaInfo;

/* SDP 生成上下文：OFFER 按现状输出（端口 9/9/50712、语义名 mid、恒 sendrecv）；
 * ANSWER 按 RFC 5888/8843/3264 归一：anchor 端口 50712 + 其余 0/a=bundle-only、
 * mid 回显 offer、方向按 offer 取反 */
typedef struct SdpGenerateParam {
  int is_answer;
  int h264_pt;
  int anchor_port;
  SdpMediaInfo remote[SDP_MEDIA_COUNT];  /* offer 的 per-m-line mid/方向 */
  int mline_count;                       /* 生成器内部状态，调用方 memset 清零 */
} SdpGenerateParam;

void sdp_append_h264(char* sdp, SdpGenerateParam* gen);

void sdp_append_pcma(char* sdp, SdpGenerateParam* gen);

void sdp_append_pcmu(char* sdp, SdpGenerateParam* gen);

void sdp_append_opus(char* sdp, SdpGenerateParam* gen);

void sdp_append_datachannel(char* sdp, SdpGenerateParam* gen);

void sdp_create(char* sdp, int b_video, int b_audio, int b_datachannel, SdpGenerateParam* gen);

int sdp_append(char* sdp, const char* format, ...);

void sdp_reset(char* sdp);

#endif  // SDP_H_
