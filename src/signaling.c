#include "signaling.h"
#include "signaling_http.h"
#include "utils.h"

typedef struct Signaling {

  SignalingOption option;
  SignalingObserver observer;
  void *impl;

} Signaling;


Signaling* signaling_create(SignalingOption signaling_option) {

  Signaling *signaling = (Signaling*)malloc(sizeof(Signaling));

  gboolean impl_created = FALSE;

  if(!signaling)
    return NULL;

  signaling->option = signaling_option;


  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      signaling->impl = (void*)signaling_http_create(signaling->option.host, signaling->option.port,
       signaling->option.call, signaling->option.index_html, &signaling->observer);
      break;
    default:
      break;
  }


  if(!signaling->impl) {
    free(signaling);
    return NULL;
  }

  return signaling;
}

void signaling_destroy(Signaling *signaling) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      signaling_http_destroy((SignalingHttp*)signaling->impl);
      break;
    default:
      break;
  }

}

void signaling_dispatch(Signaling *signaling) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      signaling_http_dispatch((SignalingHttp*)signaling->impl);
      break;
    default:
      break;
  }
}

void signaling_shutdown(Signaling *signaling) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      signaling_http_shutdown((SignalingHttp*)signaling->impl);
      break;
    default:
      break;
  }
}

void signaling_on_call_event(Signaling *signaling, void (*on_call_event), void *userdata) {

  signaling->observer.on_call_event = on_call_event;
  signaling->observer.on_call_event_data = userdata;
}

void signaling_send_answer_to_call(Signaling *signaling, char *sdp) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      signaling_http_set_answer((SignalingHttp*)signaling->impl, sdp);
      break;
    default:
      break;
  }
}

