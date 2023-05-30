#ifndef ICE_H_
#define ICE_H_

#include <stdlib.h>

#include "address.h"
#include "stun.h"

#define ICE_UFRAG_LENGTH 4
#define ICE_UPWD_LENGTH 22

typedef enum IceCandidateState {

  ICE_CANDIDATE_STATE_FROZEN,
  ICE_CANDIDATE_STATE_WAITING,
  ICE_CANDIDATE_STATE_INPROGRESS,
  ICE_CANDIDATE_STATE_SUCCEEDED,
  ICE_CANDIDATE_STATE_FAILED,

} IceCandidateState;

typedef enum IceCandidateType {

  ICE_CANDIDATE_TYPE_HOST,
  ICE_CANDIDATE_TYPE_SRFLX,

} IceCandidateType;

typedef struct IceCandidate IceCandidate;

struct IceCandidate {
   
  int foundation;

  int component;

  uint32_t priority;
 
  char transport[32 + 1];

  IceCandidateType type;

  IceCandidateState state;

  Address addr;

  Address raddr;

};

typedef struct IceCandidatePair IceCandidatePair;

struct IceCandidatePair {

  IceCandidate *local;

  IceCandidate *remote;

};

void ice_candidate_create(IceCandidate *ice_candidate, IceCandidateType type, Address *addr);

void ice_candidate_to_description(IceCandidate *candidate, char *description, int length);

int ice_candidate_from_description(IceCandidate *candidate, char *description);

int ice_candidate_get_local_address(IceCandidate *candidate, Address *address);

#endif // ICE_H_
