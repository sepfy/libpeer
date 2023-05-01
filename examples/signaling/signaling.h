#ifndef SIGNALING_H_
#define SIGNALING_H_

#include <stdlib.h>
#include <stdio.h>

void signaling_dispatch(int port, const char *index_html, void (*on_offersdp_get)(char *offersdp, void *data));

void signaling_set_answer(const char *answer);

#endif // SIGNALING_H_
