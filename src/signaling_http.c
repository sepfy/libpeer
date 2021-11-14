#include <string.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <cjson/cJSON.h>

#include "signaling.h"
#include "signaling_http.h"
#include "session_description.h"
#include "utils.h"

struct SignalingHttp {

  Signaling *signaling;

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;

  char answer[SDP_MAX_SIZE];
  gboolean answer_used;

};

gboolean signaling_http_get_answer(SignalingHttp *signaling_http, char *answer, size_t size) {

  if(signaling_http->answer_used)
    return FALSE;

  memset(answer, 0, size);
  snprintf(answer, size, "%s", signaling_http->answer);
  return TRUE;
}

gboolean signaling_http_set_answer(SignalingHttp *signaling_http, const char *answer) {

  memset(signaling_http->answer, 0, SDP_MAX_SIZE);
  snprintf(signaling_http->answer, SDP_MAX_SIZE, "%s", answer);
  signaling_http->answer_used = FALSE;
  return TRUE;
}

void signaling_http_index_request(struct evhttp_request *req, void *arg) {

  const char *index_html = (const char*)arg;

  struct evbuffer *evb = NULL;

  if(evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    evhttp_send_error(req, HTTP_BADMETHOD, 0);
    return;
  }

  evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
  evb = evbuffer_new();
  evbuffer_add(evb, index_html, strlen(index_html));
  evhttp_send_reply(req, HTTP_OK, "OK", evb);

  if(evb)
    evbuffer_free(evb);
}

void signaling_http_channel_request(struct evhttp_request *req, void *arg) {

  SignalingHttp *signaling_http = (SignalingHttp*)arg;
  Signaling *signaling = signaling_http->signaling;

  struct evbuffer *evb = NULL;
  struct evbuffer *buf = NULL;
  cJSON *json = NULL;
  cJSON *type = NULL;
  cJSON *sdp = NULL;
  char *payload = NULL;
  char *anwser = NULL;
  size_t len;

  if(evhttp_request_get_command(req) != EVHTTP_REQ_POST) {
    evhttp_send_error(req, HTTP_BADMETHOD, 0);
    return;
  }

  buf = evhttp_request_get_input_buffer(req);

  len = evbuffer_get_length(buf);

  payload = (char*)malloc(len);

  if(payload == NULL) {
    evhttp_send_error(req, HTTP_INTERNAL, 0);
    return;
  }

  evbuffer_remove(buf, payload, len);

  do {

    json = cJSON_Parse(payload);

    type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if(!cJSON_IsString(type) || (type->valuestring == NULL)) {
      break;
    }

    sdp = cJSON_GetObjectItemCaseSensitive(json, "sdp");
    if(!cJSON_IsString(sdp) || (sdp->valuestring == NULL)) {
      break;
    }

    else if(strcmp(type->valuestring, "offer") == 0) {
printf("%s\n", sdp->valuestring);
      signaling_notify_event(signaling, SIGNALING_EVENT_GET_OFFER, sdp->valuestring);
    }

  } while(0);


  if(payload)
    free(payload);

  cJSON_Delete(json);

  evb = evbuffer_new();

  // HTTP only support answer
  char answer[SDP_MAX_SIZE] = {0};
  if(signaling_http_get_answer(signaling_http, answer, SDP_MAX_SIZE)) {
    evbuffer_add_printf(evb, "{\"type\": \"answer\", \"sdp\": \"%s\"}", answer);
  }
  else {
    evbuffer_add_printf(evb, "{}");
  }

  evhttp_send_reply(req, HTTP_OK, "OK", evb);

  if(evb)
    evbuffer_free(evb);
}

SignalingHttp* signaling_http_create(const char *host, int port, const char *channel, const char *html, Signaling *signaling) {

  SignalingHttp *signaling_http = (SignalingHttp*)malloc(sizeof(SignalingHttp));

  signaling_http->answer_used = TRUE;
  signaling_http->signaling = signaling;

  char http_channel[128] = {0};
  snprintf(http_channel, sizeof(http_channel), "/channel/%s", channel);

  if(signaling_http == NULL)
    return NULL;

  do {

    signaling_http->base = event_base_new();
    if(!signaling_http->base) {
      LOG_ERROR("Couldn't create http event base.");
      break;
    }

    signaling_http->http = evhttp_new(signaling_http->base);
    if(!signaling_http->http) {
      LOG_ERROR("Couldn't create evhttp. Exiting.");
      break;
    }

    if(html)
      evhttp_set_cb(signaling_http->http, "/", signaling_http_index_request, (void*)html);

    evhttp_set_cb(signaling_http->http, http_channel, signaling_http_channel_request, signaling_http);

    signaling_http->handle = evhttp_bind_socket_with_handle(signaling_http->http, host, port);

    if(!signaling_http->handle) {
      LOG_ERROR("couldn't bind to %s:%d. Exiting.\n", host, port);
      break;
    }


    return signaling_http;

  } while(0);

  free(signaling_http);
  return NULL;

}

void signaling_http_destroy(SignalingHttp *signaling_http) {

  if(!signaling_http)
    return;

  if(signaling_http->http)
    evhttp_free(signaling_http->http);

  if(signaling_http->base)
    event_base_free(signaling_http->base);

  free(signaling_http);
  signaling_http = NULL;
}

void signaling_http_dispatch(SignalingHttp *signaling_http) {

  event_base_dispatch(signaling_http->base);
}

