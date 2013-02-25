#include "TestHarness.hpp"
#include "Common.hpp"

using namespace t2;

BEGIN_TEST_CASE("PopLsb")
{
  ASSERT_EQUAL(CountTrailingZeroes(0), 32);

  ASSERT_EQUAL(CountTrailingZeroes(2), 1);

  ASSERT_EQUAL(CountTrailingZeroes(1), 0);

  ASSERT_EQUAL(CountTrailingZeroes(0xffffffff), 0);
  ASSERT_EQUAL(CountTrailingZeroes(0xfffffffe), 1);
  ASSERT_EQUAL(CountTrailingZeroes(0x80000000), 31);
}
END_TEST_CASE
