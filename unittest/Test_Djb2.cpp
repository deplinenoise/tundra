#include "Common.hpp"
#include "TestHarness.hpp"
#include <cstring>

using namespace t2;

BEGIN_TEST_CASE("Djb2")
{
  ASSERT_EQUAL(Djb2Hash("FooBar"), 3007235198);
}
END_TEST_CASE

BEGIN_TEST_CASE("Djb2NoCase")
{
  ASSERT_EQUAL(Djb2HashNoCase("FooBar"), Djb2Hash("foobar"));
  ASSERT_EQUAL(Djb2HashNoCase("foobar"), Djb2Hash("foobar"));
}
END_TEST_CASE

