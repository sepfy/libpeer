#include "signaling_observer.h"

void signaling_observer_notify_event(SignalingObserver *signaling_observer, SignalingEvent signaling_event, char *msg) {

  switch(signaling_event) {
    case SIGNALING_EVENT_GET_OFFER:
      if(signaling_observer->on_call_event) {
        signaling_observer->on_call_event(signaling_event, msg,
         signaling_observer->on_call_event_data);
      }
      break;
    case SIGNALING_EVENT_GET_ANSWER:
      break;
    default:
      break;
  }
}

