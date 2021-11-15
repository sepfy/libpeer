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

} SignalingProtocol;


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
 * @brief Request signaling send an answer to channel.
 * @param Struct Signaling.
 * @param Session description of answer.
 */
void signaling_send_answer_to_channel(Signaling *signaling, char *sdp);

/**
 * @brief Register event of signaling service.
 * @param Struct Signaling.
 * @param Callback function of signaling if channel has new event.
 * @param Userdata of signaling channel event.
 */
void signaling_on_channel_event(Signaling *signaling, void (*on_channel_event), void *userdata);

#endif // SIGNALING_H_
