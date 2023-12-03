#ifndef PEER_SIGNALING_H_
#define PEER_SIGNALING_H_

#include "peer_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

int peer_signaling_join_channel(const char *client_id, PeerConnection *pc);

void peer_signaling_leave_channel();

int peer_signaling_loop();

#ifdef __cplusplus
}
#endif

#endif //PEER_SIGNALING_H_
