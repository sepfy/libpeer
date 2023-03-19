#include "gtest/gtest.h"

extern "C" {
#include  "utils.h"
}

TEST(utils, test1) {

  uint8_t data_in[128];
  uint8_t data_out[128];
  int sum;

  for(int i = 0; i < sizeof(data_in); i++) {

    data_in[i] = i;
  }

  Buffer *rb = utils_buffer_new(128);

  utils_buffer_push(rb, data_in, 64);

  sum = 0;

  memset(data_out, 0, 32);

  utils_buffer_pop(rb, data_out, 32);

  for(int i = 0; i < 32; i++) {

    sum += data_out[i];
  }

  EXPECT_EQ(sum, (0 + 31)*32/2);

  utils_buffer_push(rb, data_in, 64);

  sum = 0;

  memset(data_out, 0, 64);

  utils_buffer_pop(rb, data_out, 64);

  for(int i = 0; i < 64; i++) {

    sum += data_out[i];
  }

  EXPECT_EQ(sum, (0 + 63)*64/2);


}

GTEST_API_ int main(int argc, char *argv[]) {

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
