#include "Common.hpp"
#include "TestHarness.hpp"

using namespace t2;

BEGIN_TEST_CASE("Pow2")
{
  ASSERT_EQUAL(NextPowerOfTwo(0), 0);
  ASSERT_EQUAL(NextPowerOfTwo(1), 1);
  ASSERT_EQUAL(NextPowerOfTwo(2), 2);
  ASSERT_EQUAL(NextPowerOfTwo(3), 4);
  ASSERT_EQUAL(NextPowerOfTwo(4), 4);
  ASSERT_EQUAL(NextPowerOfTwo(5), 8);
  ASSERT_EQUAL(NextPowerOfTwo(7), 8);
  ASSERT_EQUAL(NextPowerOfTwo(8), 8);
  ASSERT_EQUAL(NextPowerOfTwo(126), 128);
  ASSERT_EQUAL(NextPowerOfTwo(65500), 65536);
  ASSERT_EQUAL(NextPowerOfTwo(0x78a1987a), 0x80000000);
}
END_TEST_CASE

