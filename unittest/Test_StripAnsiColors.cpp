#include "Common.hpp"
#include "TestHarness.hpp"
#include <cstring>
#include "NodeResultPrinting.hpp"

using namespace t2;

TEST(StripAnsi, CarrotAtVeryEnd)
{
  char buffer[] = "hey sailor ^";
  StripAnsiColors(buffer);
  const char* expectation = "hey sailor ^";
  ASSERT_STREQ(expectation, buffer);
}

TEST(StripAnsi, WithoutTerminator)
{
  char buffer[] = "hey sailor \x1B[3";
  StripAnsiColors(buffer);
  const char* expectation = "hey sailor \x1B[3";
  ASSERT_STREQ(expectation, buffer);
}


TEST(StripAnsi, TwoAdjecentCodes)
{
  char buffer[] ="hallo\x1B[1m\x1B[0mthere";
  StripAnsiColors(buffer);
  ASSERT_STREQ("hallothere", buffer);
}

TEST(StripAnsi, Non_m_Terminator)
{
  char buffer[] ="hallo\x1B[1bthere";
  StripAnsiColors(buffer);
  ASSERT_STREQ("hallothere", buffer);
}

TEST(StripAnsi, RealClangOutput)
{
  char buffer[] = "\x1B[1mtest.cpp:1:23: \x1B[0m\x1B[0;1;31merror: \x1B[0m\x1B[1minvalid digit 'f' in decimal constant\x1B[0m\n"
"int main() { return 43f; }\n"
"\x1B[0;1;32m                      ^\n"
"\x1B[0m1 error generated.";

  StripAnsiColors(buffer);

  const char* expectation = "test.cpp:1:23: error: invalid digit 'f' in decimal constant\n"
"int main() { return 43f; }\n"
"                      ^\n"
"1 error generated.";

  ASSERT_STREQ(expectation, buffer);
}

