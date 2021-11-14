#include "signaling.h"
#include "signaling_http.h"
#include "utils.h"

typedef struct Signaling {

  SignalingOption option;
  void *impl;
  void (*on_channel_event)(SignalingEvent signal_event, char *data, void *userdata);
  void *on_channel_event_data;

} Signaling;

Signaling* signaling_create_service(SignalingOption signaling_option) {

  Signaling *signaling = (Signaling*)malloc(sizeof(Signaling));

  gboolean impl_created = FALSE;

  if(!signaling)
    return NULL;

  signaling->option = signaling_option;


  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      {
        SignalingHttp *signaling_http = signaling_http_create(signaling->option.host,
         signaling->option.port, signaling->option.channel, signaling->option.index_html, signaling);

        if(signaling_http) {
          signaling->impl = (void*)signaling_http;
          impl_created = TRUE;
        }
      }
      break;
    default:
      break;
  }


  if(!impl_created) {
    free(signaling);
    return NULL;
  }

  return signaling;
}

void signaling_destroy(Signaling *signaling) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      {
        SignalingHttp *signaling_http = (SignalingHttp*)signaling->impl;
        signaling_http_destroy(signaling_http);
      }
      break;
    default:
      break;
  }

}

void signaling_dispatch(Signaling *signaling) {

  switch(signaling->option.protocol) {
    case SIGNALING_PROTOCOL_HTTP:
      {
        SignalingHttp *signaling_http = (SignalingHttp*)signaling->impl;
        signaling_http_dispatch(signaling_http);
      }
      break;
    default:
      break;
  }

}

void signaling_send_channel_message(Signaling *signaling, char *message) {

}

