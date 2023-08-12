#ifndef PEER_SIGNALING_H_
#define PEER_SIGNALING_H_

#include "peer_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc, const char *cacert);

void peer_signaling_leave_channel();

int peer_signaling_loop();

void peer_signaling_process_request(const char *msg, size_t size);

char* peer_signaling_create_offer(char *description);

#ifdef __cplusplus
} 
#endif

#endif //PEER_SIGNALING_H_

