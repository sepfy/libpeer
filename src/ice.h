#ifndef ICE_H_
#define ICE_H_

#include <stdlib.h>
#include <time.h>

#include "address.h"
#include "stun.h"

#define ICE_UFRAG_LENGTH 256
#define ICE_UPWD_LENGTH 256

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
  ICE_CANDIDATE_TYPE_PRFLX,
  ICE_CANDIDATE_TYPE_RELAY,

} IceCandidateType;

typedef struct IceCandidate IceCandidate;

struct IceCandidate {
  char foundation[32 + 1];
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
  IceCandidateState state;
  IceCandidate* local;
  IceCandidate* remote;
  int conncheck;
  uint64_t priority;
};

void ice_candidate_create(IceCandidate* ice_candidate, int foundation, IceCandidateType type, Address* addr);

void ice_candidate_to_description(IceCandidate* candidate, char* description, int length);

int ice_candidate_from_description(IceCandidate* candidate, char* description, char* end);

int ice_candidate_get_local_address(IceCandidate* candidate, Address* address);

#endif  // ICE_H_
