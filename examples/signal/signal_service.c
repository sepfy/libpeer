#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <glib.h>
#include <cjson/cJSON.h>

#include "utils.h"
#include "signal_service.h"

static void api_request_cb(struct evhttp_request *req, void *arg) {

  signal_service_t *signal_service = (signal_service_t*)arg;

  struct evbuffer *evb = NULL;
  struct evbuffer *buf = NULL;
  cJSON *json = NULL;
  cJSON *params = NULL;
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

  json = cJSON_Parse(payload);
  params = cJSON_GetObjectItemCaseSensitive(json, "params");
  if(cJSON_IsString(params) && (params->valuestring != NULL)) {
    if(signal_service->on_offer_get != NULL) {
      anwser = signal_service->on_offer_get(params->valuestring, signal_service->data);
    }
  }

  if(payload != NULL)
    free(payload);

  evb = evbuffer_new();
  evbuffer_add_printf(evb, "{\"jsonrpc\": \"2.0\", \"result\": \"%s\"}", anwser);

  evhttp_send_reply(req, HTTP_OK, "OK", evb);
}

static void index_request_cb(struct evhttp_request *req, void *arg) {

  struct evbuffer *evb = NULL;
  int fd = -1;
  struct stat st;

  if(evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    evhttp_send_error(req, HTTP_BADMETHOD, 0);
    return;
  }

  if((fd = open("index.html", O_RDONLY)) < 0) {
    LOG_ERROR("%s", strerror(errno));
    evhttp_send_error(req, HTTP_NOTFOUND, "Document was not found");
    return;
  }

  if(fstat(fd, &st) < 0) {
    LOG_ERROR("%s", strerror(errno));
    evhttp_send_error(req, HTTP_NOTFOUND, "Document was not found");
    return;
  }

  evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
  evb = evbuffer_new();
  evbuffer_add_file(evb, fd, 0, st.st_size);
  evhttp_send_reply(req, HTTP_OK, "OK", evb);

  if(evb)
    evbuffer_free(evb);
}

int signal_service_create(signal_service_t *signal_service, options_t options) {

  signal_service->options = options;

  signal_service->base = event_base_new();
  if(!signal_service->base) {
    LOG_ERROR("Couldn't create an event_signal_service->base: exiting");
    return 1;
  }

  signal_service->http = evhttp_new(signal_service->base);
  if(!signal_service->http) {
    LOG_ERROR("Couldn't create evhttp. Exiting.");
    return 1;
  }

  evhttp_set_cb(signal_service->http, "/api", api_request_cb, signal_service);
  evhttp_set_cb(signal_service->http, "/", index_request_cb, signal_service);

  signal_service->handle = evhttp_bind_socket_with_handle(signal_service->http,
   signal_service->options.host, signal_service->options.port);
  if(!signal_service->handle) {
    LOG_ERROR("couldn't bind to %s:%d. Exiting.\n", signal_service->options.host,
     signal_service->options.port);
    return 1;
  }

  return 0;
}

void signal_service_dispatch(signal_service_t *signal_service) {

  printf("Listening %s:%d\n", signal_service->options.host, signal_service->options.port); 
  event_base_dispatch(signal_service->base);
}

void signal_service_on_offer_get(signal_service_t *signal_service,
 char* (*on_offer_get)(char *offer, void *data), void *data) {
  signal_service->data = data;
  signal_service->on_offer_get = on_offer_get;
}
