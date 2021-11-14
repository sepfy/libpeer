#ifndef SIGNALING_OBSERVER_H_
#define SIGNALING_OBSERVER_H_

typedef enum SignalingEvent {

  SIGNALING_EVENT_GET_OFFER,
  SIGNALING_EVENT_GET_ANSWER,

} SignalingEvent;


typedef struct SignalingObserver SignalingObserver;

void signaling_observer_notify(SignalingObserver *signaling_observer, SignalingEvent signaling_event, char *msg);

#endif // SIGNALING_OBSERVER_H_
