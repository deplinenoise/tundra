#include "Hash.hpp"
#include "TestHarness.hpp"
#include <cstring>

using namespace t2;

#if ENABLED(USE_SHA1_HASH)

BEGIN_TEST_CASE("DigestToString")
{
  HashDigest digest;
  memset(&digest, 0, sizeof digest);
  char str[41];
  DigestToString(str, digest);
  ASSERT_EQUAL_STRING(str, "0000000000000000000000000000000000000000");
}
END_TEST_CASE

BEGIN_TEST_CASE("Hasher/EmptyInput")
{
  HashDigest digest;
  HashState h;
  HashInit(&h);
  HashFinalize(&h, &digest);
  char str[41];
  DigestToString(str, digest);
  ASSERT_EQUAL_STRING(str, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}
END_TEST_CASE

BEGIN_TEST_CASE("Hasher/General")
{
  HashDigest digest;
  HashState h;
  HashInit(&h);
  for (int i = 0; i < 64; ++i)
  {
    HashAddString(&h, "foobar123456789ABZ\n");
  }
  HashFinalize(&h, &digest);

  char str[41];
  DigestToString(str, digest);
  ASSERT_EQUAL_STRING(str, "2683f3ebfa90689caf6d1be96370908e0315bc0b");
}
END_TEST_CASE

BEGIN_TEST_CASE("Hasher/Int64")
{
  HashDigest digest;
  HashState h;
  HashInit(&h);
  HashAddInteger(&h, 0x1122334455667788);
  HashFinalize(&h, &digest);
  char str[41];
  DigestToString(str, digest);
  ASSERT_EQUAL_STRING(str, "bdc04c0992f37f1e3889f274d0549cc0405811d5");
}
END_TEST_CASE

#if 0
BEGIN_TEST_CASE("Hash/Compare")
{
  HashDigest a, b;
  a.m_Words[0] = 0; a.m_Words[1] = 0; a.m_Words[2] = 0; a.m_Words[3] = 0; a.m_Words[4] = 0;
  b.m_Words[0] = 0; b.m_Words[1] = 0; b.m_Words[2] = 0; b.m_Words[3] = 0; b.m_Words[4] = 0;
  ASSERT_EQUAL(CompareHashDigests(a, b), 0);

  a.m_Words[0] = 1; a.m_Words[1] = 0; a.m_Words[2] = 0; a.m_Words[3] = 0; a.m_Words[4] = 0;
  b.m_Words[0] = 0; b.m_Words[1] = 0; b.m_Words[2] = 0; b.m_Words[3] = 0; b.m_Words[4] = 0;
  ASSERT_EQUAL(CompareHashDigests(a, b), 1);

  a.m_Words[0] = 0; a.m_Words[1] = 0; a.m_Words[2] = 0; a.m_Words[3] = 0; a.m_Words[4] = 0;
  b.m_Words[0] = 1; b.m_Words[1] = 0; b.m_Words[2] = 0; b.m_Words[3] = 0; b.m_Words[4] = 0;
  ASSERT_EQUAL(CompareHashDigests(a, b), -1);
}
END_TEST_CASE
#endif

#endif
