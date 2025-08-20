#include <gtest/gtest.h>
#include <discovery.hpp>
#include <vector>
#include <array>

using namespace rlp;

TEST(PeerDiscovery, RunTest) {
  // TODO add test condition
  test_ping();
  EXPECT_EQ(1, 1);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}