#ifndef SIGNALING_HTTP_H_
#define SIGNALING_HTTP_H_

#include <glib.h>

#include "signaling.h"

typedef struct SignalingHttp SignalingHttp;

SignalingHttp* signaling_http_create(const char *host, int port, const char *channel, const char *html, Signaling *signaling);

void signaling_http_destroy(SignalingHttp *signaling_http);

void signaling_http_dispatch(SignalingHttp *signaling_http);

gboolean signaling_http_get_answer(SignalingHttp *signaling_http, char *answer, size_t size);

gboolean signaling_http_set_answer(SignalingHttp *signaling_http, const char *answer);

#endif // SIGNALING_HTTP_H_
