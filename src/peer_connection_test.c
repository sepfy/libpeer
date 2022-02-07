#include "gtest/gtest.h"

extern "C" {
#include "peer_connection.h"
}

TEST(peer_connection, peer_connection_create) {

  PeerConnection *pc = peer_connection_create();
  if(pc)
    peer_connection_destroy(pc);
}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
