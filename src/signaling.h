/**
 * @file singaling.h
 * @brief Struct Signaling
 */
#ifndef SIGNALING_H_
#define SIGNALING_H_

#include <stdint.h>

#ifndef INDEX_HTML
#define INDEX_HTML "test"
#endif

typedef enum SignalingProtocol {

  SIGNALING_PROTOCOL_HTTP,

} SignalingProtocol;


typedef enum SignalingEvent {

  SIGNALING_EVENT_CONNECTED,
  SIGNALING_EVENT_GET_OFFER,
  SIGNALING_EVENT_GET_ANSWER,


} SignalingEvent;


typedef struct SignalingOption {

  SignalingProtocol protocol;
  const char *host;
  const char *channel;
  uint16_t port;
  const char *index_html;

} SignalingOption;


typedef struct Signaling Signaling;

/**
 * @brief Create a new Signaling.
 * @param signaling configuration.
 * @return Pointer of struct Signaling.
 */
Signaling* signaling_create_service(SignalingOption signaling_option);

void signaling_destroy(Signaling *signaling);

void signaling_dispatch(Signaling *signaling);

void signaling_send_channel_message(Signaling *signaling, char *message);

void signaling_notify_event(Signaling *signaling, SignalingEvent signaling_event, char *data);

void signaling_on_channel_event(Signaling *signaling, void (*on_channel_event), void *userdata);

#endif // SIGNALING_H_
