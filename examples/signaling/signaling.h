#ifndef SIGNALING_H_
#define SIGNALING_H_

#include <stdio.h>
#include <stdlib.h>

void signaling_set_answer(const char *sdp);

void signaling_dispatch(int port, const char *index_html,
 void (*signaling_on_offersdp_get)(const char* offersdp, size_t len));

#endif // SIGNALING_H_

