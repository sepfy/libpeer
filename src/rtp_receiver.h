#ifndef RTP_RECEIVER_H_
#define RTP_RECEIVER_H_

#include <stdint.h>
#include <endian.h>

typedef struct rtp_header_t {
#if __BYTE_ORDER == __BIG_ENDIAN
        uint16_t version:2;
        uint16_t padding:1;
        uint16_t extension:1;
        uint16_t csrccount:4;
        uint16_t markerbit:1;
        uint16_t type:7;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
        uint16_t csrccount:4;
        uint16_t extension:1;
        uint16_t padding:1;
        uint16_t version:2;
        uint16_t type:7;
        uint16_t markerbit:1;
#endif
        uint16_t seq_number;
        uint32_t timestamp;
        uint32_t ssrc;
        uint32_t csrc[16];
} rtp_header_t;


int rtp_receiver_is_rtp(char *buf, size_t size);

#endif // RTP_RECEIVER_H_
