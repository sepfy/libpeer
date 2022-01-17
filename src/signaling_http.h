#ifndef SIGNALING_HTTP_H_
#define SIGNALING_HTTP_H_

#include <glib.h>

#include "signaling_observer.h"

typedef struct SignalingHttp SignalingHttp;

SignalingHttp* signaling_http_create(const char *host, int port, const char *call, const char *index_html, SignalingObserver *signaling_observer);

void signaling_http_destroy(SignalingHttp *signaling_http);

void signaling_http_dispatch(SignalingHttp *signaling_http);

void signaling_http_shutdown(SignalingHttp *signaling_http);

size_t signaling_http_get_answer(SignalingHttp *signaling_http, char *answer, size_t size);

void signaling_http_set_answer(SignalingHttp *signaling_http, const char *answer);

#endif // SIGNALING_HTTP_H_
