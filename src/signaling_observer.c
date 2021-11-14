#include "signaling_observer.h"

typedef struct SignalingObserver SignalingObserver;

void signaling_observer_notify(SignalingObserver *signaling_observer, SignalingEvent signaling_event, char *msg) {



}

void signaling_notify_event(SignalingObserver *signaling_observer, SignalingEvent signaling_event, char *data) {
#if 0
  if(signaling->on_channel_event)
    signaling->on_channel_event(signaling_event, data, signaling->on_channel_event_data);
#endif
}

void signaling_on_channel_event(SignalingObserver *signaling_observer, void (*on_channel_event), void *userdata) {
#if 0
  signaling->on_channel_event = on_channel_event;
  signaling->on_channel_event_data = userdata;
#endif
}


