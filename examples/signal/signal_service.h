#include <stdio.h>
#include <stdlib.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>


typedef struct options_t {

  int port;
  const char *host;
  const char *root;

} options_t;

typedef struct signal_service_t {

  struct event_base *base;
  struct evhttp *http;
  struct evhttp_bound_socket *handle;
  options_t options;
  char* (*on_offer_get)(char *offer, void *data);
  void *data;

} signal_service_t;

int signal_service_create(signal_service_t *signal_service, options_t options);

void signal_service_dispatch(signal_service_t *signal_service);

void signal_service_on_offer_get(signal_service_t *signal_service,
 char* (*on_offer_get)(char *offer, void *data), void *data);
