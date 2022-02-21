/**
 * @file singaling.h
 * @brief Struct Signaling.
 */
#ifndef SIGNALING_H_
#define SIGNALING_H_

#include <stdint.h>
#include "signaling_observer.h"
#include "signaling_http.h"

typedef enum SignalingProtocol {

  SIGNALING_PROTOCOL_HTTP,
  SIGNALING_PROTOCOL_MQTT,

} SignalingProtocol;


typedef struct SignalingOption {

  SignalingProtocol protocol;
  const char *host;
  const char *call;
  uint16_t port;
  const char *index_html;

  const char *tls;
  const char *username;
  const char *password;

} SignalingOption;


typedef struct Signaling Signaling;

/**
 * @brief Create a new Signaling.
 * @param Signaling option.
 * @return Pointer of struct Signaling.
 */
Signaling* signaling_create(SignalingOption signaling_option);

/**
 * @brief Destroy a Signaling.
 * @param Struct Signaling.
 */
void signaling_destroy(Signaling *signaling);

/**
 * @brief Dispatch Signaling service.
 * @param Struct Signaling.
 */
void signaling_dispatch(Signaling *signaling);

/**
 * @brief Shutdown Signaling service.
 * @param Struct Signaling.
 */
void signaling_shutdown(Signaling *signaling);

/**
 * @brief Request signaling send an answer to call.
 * @param Struct Signaling.
 * @param Session description of answer.
 */
void signaling_send_answer_to_call(Signaling *signaling, char *sdp);

/**
 * @brief Register event of signaling service.
 * @param Struct Signaling.
 * @param Callback function of signaling if call has new event.
 * @param Userdata of signaling call event.
 */
void signaling_on_call_event(Signaling *signaling, void (*on_call_event), void *userdata);

#endif // SIGNALING_H_
