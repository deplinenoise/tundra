#include "TestHarness.hpp"

#include <cstdio>
#include <cstdarg>

#if defined(_MSC_VER)
#define snprintf _snprintf
#endif

namespace t2
{

#define IMPLEMENT_FORMAT_VALUE(t, fmt) \
void FormatValue(char* buf, size_t buf_size, t value) \
{ \
  snprintf(buf, buf_size, fmt, value); \
  buf[buf_size-1] = '\0'; \
}

IMPLEMENT_FORMAT_VALUE(long long, "%lld")
IMPLEMENT_FORMAT_VALUE(unsigned long long, "%llu")
IMPLEMENT_FORMAT_VALUE(int, "%d")
IMPLEMENT_FORMAT_VALUE(unsigned int, "%u")
IMPLEMENT_FORMAT_VALUE(long, "%ld")
IMPLEMENT_FORMAT_VALUE(unsigned long, "%lu")
IMPLEMENT_FORMAT_VALUE(const void*, "%p")

TestRegistry::TestRegistry() : m_TestCases(nullptr) {}
TestRegistry::~TestRegistry() {}

TestRegistry& TestRegistry::GetInstance()
{
  static TestRegistry s_Registry;
  return s_Registry;
}

TestCase* TestRegistry::Freeze()
{
  TestCase* chain = m_TestCases;
  m_TestCases = nullptr;

  // Reverse list to natural file order.
  while (chain)
  {
    TestCase* next = chain->GetNext();
    chain->SetNext(m_TestCases);
    m_TestCases = chain;
    chain = next;
  }
  
  return m_TestCases;
}

void TestRegistry::RegisterTestCase(TestCase* t)
{
  t->SetNext(m_TestCases);
  m_TestCases = t;
}

TestErrorMessage::TestErrorMessage()
: m_FailingFile(nullptr)
, m_FailingLine(0)
{
  m_Message[0] = 0;
}

bool TestCase::OnAssertFailure(
    const char*       file,
    int               line,
    TestErrorMessage* msg,
    const char*       fmt,
    ...)
{
  msg->m_FailingFile = file;
  msg->m_FailingLine = line;
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg->m_Message, sizeof msg->m_Message, fmt, args);
  va_end(args);
  msg->m_Message[sizeof(msg->m_Message)-1] = 0;
  return false;
}

static bool RunTests(TestCase *first_test)
{
  int test_count = 0;
  int fail_count = 0;

  while (first_test)
  {
    ++test_count;
    printf(".");
    fflush(stdout);
    TestErrorMessage msg;

    bool success = first_test->Run(&msg);

    if (!success)
    {
      fprintf(stderr, "\nTest failed: %s\n%s(%d): %s\n",
          first_test->GetLabel(),
          msg.m_FailingFile,
          msg.m_FailingLine,
          msg.m_Message);
      ++fail_count;
    }

    first_test = first_test->GetNext();
  }

  static const char* suffixes[2] = { "", "s" };
  printf("\n%d test%s run, %d failure%s\n",
      test_count, suffixes[test_count != 1], 
      fail_count, suffixes[fail_count != 1]);

  return fail_count == 0;
}

}

int main()
{
  using namespace t2;

  return RunTests(TestRegistry::GetInstance().Freeze()) ? 0 : 1;
}

