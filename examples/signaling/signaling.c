#include <string.h>
#define HTTPSERVER_IMPL
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <httpserver.h>

#define OFFER_URL "/offer"
#define ANSWER_URL "/answer"

void (*g_signaling_on_offersdp_get)(const char *offersdp, size_t len) = NULL;
const char *g_index_html = NULL;
char *g_answer = NULL;

void signaling_set_answer(const char *sdp) {

  // clear old data
  if(g_answer != NULL) {

    free(g_answer);
    g_answer = NULL;
  }

  g_answer = strdup(sdp);
}

void signaling_handle_request(struct http_request_s* request) {

  struct http_response_s* response = http_response_init();

  http_response_status(response, 200);

  http_string_t url = http_request_target(request);

  if(url.len == strlen(OFFER_URL) && memcmp(url.buf, OFFER_URL, url.len) == 0) {

    http_string_t body = http_request_body(request);
    if(g_signaling_on_offersdp_get != NULL)
      g_signaling_on_offersdp_get(body.buf, body.len);

    http_response_header(response, "Content-Type", "text/plain");
    http_response_body(response, "", 0);
  }
  else if(url.len == strlen(ANSWER_URL) && memcmp(url.buf, ANSWER_URL, url.len) == 0) {

    http_response_header(response, "Content-Type", "text/plain");

    if(g_answer != NULL) {

      http_response_body(response, g_answer, strlen(g_answer));
    }
    else {
      http_response_body(response, "", 0);
    }
  }
  else {

    http_response_header(response, "Content-Type", "text/html");
    if(g_index_html != NULL)
      http_response_body(response, g_index_html, strlen(g_index_html));
    else
      http_response_body(response, "", 0);
  }

  http_respond(request, response);
}

void signaling_dispatch(int port, const char *index_html,
 void (*signaling_on_offersdp_get)(const char* offersdp, size_t len)) {

  struct http_server_s *http_server; 

  g_index_html = index_html;

  g_signaling_on_offersdp_get = signaling_on_offersdp_get;

  http_server = http_server_init(port, signaling_handle_request);

  http_server_listen(http_server);
}

