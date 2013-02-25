#ifndef TESTHARNESS_HPP
#define TESTHARNESS_HPP

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

namespace t2
{

#define ASSERT_EQUAL(value, expected) \
do { if (!this->AssertEqual(__FILE__, __LINE__, #value " == " #expected, value, expected, message_out_)) return false; } while(0)

#define ASSERT_NOT_EQUAL(value, expected) \
do { if (!this->AssertNotEqual(__FILE__, __LINE__, #value " != " #expected, value, expected, message_out_)) return false; } while(0)

#define ASSERT_EQUAL_STRING(value, expected) \
do { if (!this->AssertEqualString(__FILE__, __LINE__, #value " strequals " #expected, value, expected, message_out_)) return false; } while(0)
#define ASSERT_EQUAL_FLOAT(value, expected, tolerance) \
do { if (!this->AssertEqualFloat(__FILE__, __LINE__, #value " == " #expected, value, expected, tolerance, message_out_)) return false; } while(0)

class TestCase;

class TestRegistry
{
public:
  void RegisterTestCase(TestCase* t);

  void RunTests();

public:
  static TestRegistry& GetInstance();

private:
  TestRegistry();
  ~TestRegistry();

private:
  TestRegistry(const TestRegistry&);
  TestRegistry& operator=(const TestRegistry&);

public:
  TestCase* Freeze();

private:
  TestCase* m_TestCases;
};

void FormatValue(char* buf, size_t buf_size, int value);
void FormatValue(char* buf, size_t buf_size, unsigned int value);
void FormatValue(char* buf, size_t buf_size, long value);
void FormatValue(char* buf, size_t buf_size, unsigned long value);
void FormatValue(char* buf, size_t buf_size, long long value);
void FormatValue(char* buf, size_t buf_size, unsigned long long value);
void FormatValue(char* buf, size_t buf_size, const void* value);

struct TestErrorMessage
{
  TestErrorMessage();

  const char* m_FailingFile;
  int         m_FailingLine;
  char        m_Message[2048];
};

class TestCase
{
private:
  const char* m_Label;
  TestCase*   m_Next;

public:
  TestCase(const char *label)
  : m_Label(label)
  , m_Next(nullptr)
  {}

public:
  const char* GetLabel() const { return m_Label; }

  TestCase* GetNext() const { return m_Next; }
  void SetNext(TestCase* c) { m_Next = c; }

  virtual bool Run(TestErrorMessage* message_out) = 0;

private:
  bool OnAssertFailure(
      const char*       file,
      int               line,
      TestErrorMessage* msg,
      const char*       fmt,
      ...); 

protected:
  template <typename T, typename U>
  bool AssertEqual(const char* file, int line, const char* expr, T value, U expected, TestErrorMessage *msg)
  {
    if (static_cast<T>(expected) != value)
    {
      char buf1[1024];
      char buf2[1024];
      FormatValue(buf1, sizeof buf1, value);
      FormatValue(buf2, sizeof buf2, expected);
      return OnAssertFailure(file, line, msg, "value equality failed: %s: %s != %s", expr, buf1, buf2);
    }
    return true;
  }

  inline bool AssertEqualString(const char* file, int line, const char* expr, const char* value, const char* expected, TestErrorMessage *msg)
  {
    if (0 != strcmp(expected, value))
    {
      return OnAssertFailure(file, line, msg, "string equality failed: %s: \"%s\" != \"%s\"", expr, value, expected);
    }
    return true;
  }

  inline bool AssertEqualFloat(const char* file, int line, const char* expr, double value, double expected, double tolerance, TestErrorMessage *msg)
  {
    if (fabs(value - expected) > tolerance)
    {
      return OnAssertFailure(file, line, msg, "float equality failed: %s: %f != %f (tolerance: %f)", expr, value, expected, tolerance);
    }
    return true;
  }

  template <typename T, typename U>
  bool AssertNotEqual(const char* file, int line, const char* expr, T value, U expected, TestErrorMessage *msg)
  {
    if (static_cast<T>(expected) == value)
    {
      char buf1[1024];
      char buf2[1024];
      FormatValue(buf1, sizeof buf1, value);
      FormatValue(buf2, sizeof buf2, expected);
      return OnAssertFailure(file, line, msg, "value inequality failed: %s: %s == %s", expr, buf1, buf2);
    }

    return true;
  }
};

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)

#define BEGIN_TEST_CASE(label) \
  namespace { \
    static class TOKENPASTE2(TestCase, __LINE__) : protected TestCase { \
    public: \
            TOKENPASTE2(TestCase, __LINE__)() : TestCase(label) { \
              TestRegistry::GetInstance().RegisterTestCase(this); \
            } \
    public: \
            virtual bool Run(TestErrorMessage* message_out_) {

#define END_TEST_CASE \
              return true; \
            } \
    } TOKENPASTE2(s_test_case_, __LINE__); \
  }

}

#endif
