#include <string.h>
#include <cjson/cJSON.h>
#define HTTPSERVER_IMPL
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#include <httpserver.h>

#include "signaling_observer.h"
#include "signaling_http.h"
#include "session_description.h"
#include "utils.h"

struct SignalingHttp {

  SignalingObserver *signaling_observer;

  char channel[128];
  struct http_server_s *server; 
  char *answer;

  const char *index_html;

};

size_t signaling_http_get_answer(SignalingHttp *signaling_http, char *answer, size_t size) {

  size_t len;
  memset(answer, 0, size);

  if(signaling_http->answer) {
    len = strlen(signaling_http->answer);
    snprintf(answer, size, "%s", signaling_http->answer);
    free(signaling_http->answer);
    signaling_http->answer = NULL;
  }
  else {
    snprintf(answer, size, "{}");
    len = strlen(answer);
  }
  
  return len;
}

void signaling_http_set_answer(SignalingHttp *signaling_http, const char *sdp) {

  char answer[SDP_MAX_SIZE] = {0};
  snprintf(answer, SDP_MAX_SIZE, "{\"type\": \"answer\", \"sdp\": \"%s\"}", sdp);

  if(signaling_http->answer) {
    free(signaling_http->answer);
  }

  signaling_http->answer = g_base64_encode(answer, strlen(answer));

}

void signaling_http_channel_request(SignalingObserver *signaling_observer, const char *body, size_t len) {

  cJSON *json = NULL;
  cJSON *type = NULL;
  cJSON *sdp = NULL;

  guchar *offer = g_base64_decode(body, &len);
  if(!offer) return;

  do {

    json = cJSON_Parse(offer);
    if(!json) return;

    type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if(!cJSON_IsString(type) || (type->valuestring == NULL)) {
      break;
    }

    sdp = cJSON_GetObjectItemCaseSensitive(json, "sdp");
    if(!cJSON_IsString(sdp) || (sdp->valuestring == NULL)) {
      break;
    }

    else if(strcmp(type->valuestring, "offer") == 0) {
      signaling_observer_notify_event(signaling_observer, SIGNALING_EVENT_GET_OFFER, sdp->valuestring);
    }

  } while(0);


  cJSON_Delete(json);
  free(offer);
}

void signaling_http_handle_request(struct http_request_s* request) {

  SignalingHttp *signaling_http = request->server->data;

  struct http_response_s* response = http_response_init();
  http_response_status(response, 200);

  http_string_t url = http_request_target(request);
  if(url.len == strlen(signaling_http->channel)
   && memcmp(url.buf, signaling_http->channel, url.len) == 0) {
    // channel
    http_string_t body = http_request_body(request);
    signaling_http_channel_request(signaling_http->signaling_observer, body.buf, body.len);
    http_response_header(response, "Content-Type", "text/plain");
    char answer[SDP_MAX_SIZE] = {0};
    int len = signaling_http_get_answer(signaling_http, answer, SDP_MAX_SIZE);
    http_response_body(response, answer, len);
  }
  else {
    // index
    http_response_header(response, "Content-Type", "text/html");
    if(signaling_http->index_html != NULL)
      http_response_body(response, signaling_http->index_html, strlen(signaling_http->index_html));
    else
      http_response_body(response, "", 0);
  }

  http_respond(request, response);
}

SignalingHttp* signaling_http_create(const char *host, int port, const char *channel,
 const char *index_html, SignalingObserver *signaling_observer) {

  SignalingHttp *signaling_http = (SignalingHttp*)malloc(sizeof(SignalingHttp));
  if(!signaling_http)
    return NULL;

  snprintf(signaling_http->channel, sizeof(signaling_http->channel), "/channel/%s", channel);
  signaling_http->index_html = index_html;
  signaling_http->server = http_server_init(8000, signaling_http_handle_request);
  http_server_set_userdata(signaling_http->server, signaling_http);

  signaling_http->signaling_observer = signaling_observer;
  return signaling_http;
}

void signaling_http_destroy(SignalingHttp *signaling_http) {

  if(!signaling_http)
    return;

  if(signaling_http->server) {
    free(signaling_http->server);
  }

  free(signaling_http);
  signaling_http = NULL;
}

void signaling_http_dispatch(SignalingHttp *signaling_http) {

  http_server_listen(signaling_http->server);
}

void signaling_http_shutdown(SignalingHttp *signaling_http) {

}
