#include <string.h>
#define HTTPSERVER_IMPL
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <glib.h>
#include <httpserver.h>

#define SDP_EXCHANGE_URL "/sdpexchange"

#include "signaling.h"

typedef struct Signaling {

  struct http_server_s *server; 

  char *answer;

  const char *index_html;

  GCond cond;

  GMutex mutex;

  void (*on_offersdp_get)(char *offersdp, void *data);

} Signaling;


Signaling g_signaling;


void signaling_set_answer(const char *sdp) {

  if(g_signaling.answer) {

    free(g_signaling.answer);
  }

  g_signaling.answer = strdup(sdp);
  g_cond_signal(&g_signaling.cond);
}

void signaling_sdpexchange_request(const char *body, size_t len) {

  char *offersdp = strndup(body, len);

  if (offersdp) {

   g_signaling.on_offersdp_get(offersdp, NULL);
   free(offersdp);
  }
}

void signaling_handle_request(struct http_request_s* request) {

  struct http_response_s* response = http_response_init();

  http_response_status(response, 200);

  http_string_t url = http_request_target(request);

  if(url.len == strlen(SDP_EXCHANGE_URL) && memcmp(url.buf, SDP_EXCHANGE_URL, url.len) == 0) {

    http_response_header(response, "Content-Type", "text/plain");

    http_string_t body = http_request_body(request);

    signaling_sdpexchange_request(body.buf, body.len);

    // Block until answer is set
    g_mutex_lock(&g_signaling.mutex);
    g_cond_wait(&g_signaling.cond, &g_signaling.mutex);
    g_mutex_unlock(&g_signaling.mutex);

    http_response_body(response, g_signaling.answer, strlen(g_signaling.answer));

  }
  else {

    // index
    http_response_header(response, "Content-Type", "text/html");

    if(g_signaling.index_html != NULL)
      http_response_body(response, g_signaling.index_html, strlen(g_signaling.index_html));
    else
      http_response_body(response, "", 0);
  }

  http_respond(request, response);
}

void signaling_dispatch(int port, const char *index_html, void (*on_offersdp_get)(char *msg, void *data)) {

  g_signaling.index_html = index_html;

  g_signaling.server = http_server_init(port, signaling_handle_request);

  g_signaling.on_offersdp_get = on_offersdp_get;

  http_server_listen(g_signaling.server);
}

