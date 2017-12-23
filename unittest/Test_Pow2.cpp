#include "Common.hpp"
#include "TestHarness.hpp"

using namespace t2;

TEST(Pow2, Basic)
{
  EXPECT_EQ(0, NextPowerOfTwo(0));
  EXPECT_EQ(1, NextPowerOfTwo(1));
  EXPECT_EQ(2, NextPowerOfTwo(2));
  EXPECT_EQ(4, NextPowerOfTwo(3));
  EXPECT_EQ(4, NextPowerOfTwo(4));
  EXPECT_EQ(8, NextPowerOfTwo(5));
  EXPECT_EQ(8, NextPowerOfTwo(7));
  EXPECT_EQ(8, NextPowerOfTwo(8));
  EXPECT_EQ(128, NextPowerOfTwo(126));
  EXPECT_EQ(65536, NextPowerOfTwo(65500));
  EXPECT_EQ(0x80000000, NextPowerOfTwo(0x78a1987a));
}

