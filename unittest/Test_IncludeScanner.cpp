#include "IncludeScanner.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

class IncludeScannerTest : public ::testing::Test
{
protected:
  MemAllocHeap heap;
  MemAllocLinear alloc;

protected:
  void SetUp() override
  {
    HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
    LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");
  }

  void TearDown() override
  {
    LinearAllocDestroy(&alloc);
    HeapDestroy(&heap);
  }

};


TEST_F(IncludeScannerTest, EmptyFile)
{
  char data[1] = { '\0' };
  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_EQ(nullptr, incs);
}

TEST_F(IncludeScannerTest, SingleIncludeNewline)
{
  char data[] = "#include \"foo.h\"\n";
  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NE(nullptr, incs);

  ASSERT_STREQ("foo.h", incs->m_String);
  ASSERT_EQ(5, incs->m_StringLen);
  ASSERT_EQ(false, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_EQ(nullptr, incs->m_Next);
}

TEST_F(IncludeScannerTest, NoClosingTerminator)
{
  char data[] = "#include <bar.h\n";
  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_EQ(nullptr, incs);
}

TEST_F(IncludeScannerTest, SingleIncludeNoNewLine)
{
  char data[] = "#include <bar.h>";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NE(nullptr, data);

  ASSERT_STREQ("bar.h", incs->m_String);
  ASSERT_EQ(5, incs->m_StringLen);
  ASSERT_EQ(true, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_EQ(nullptr, incs->m_Next);
}

TEST_F(IncludeScannerTest, MaxWhitespace)
{
  char data[] = "\n\n   #      include     <bar.h>  \n\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NE(nullptr, data);

  ASSERT_STREQ("bar.h", incs->m_String);
  ASSERT_EQ(5, incs->m_StringLen);
  ASSERT_EQ(true, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_EQ(nullptr, incs->m_Next);
}

TEST_F(IncludeScannerTest, MultiIncludes)
{
  char data[] =
    "#include <foo.h>\n"
    "#include \"a.h\"\n"
    "#include <foo/bar/baz.h>\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NE(nullptr, data);

  ASSERT_STREQ("foo.h", incs->m_String);
  ASSERT_EQ(5, incs->m_StringLen);
  ASSERT_EQ(true, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_NE(nullptr, incs->m_Next);

  incs = incs->m_Next;

  ASSERT_STREQ("a.h", incs->m_String);
  ASSERT_EQ(3, incs->m_StringLen);
  ASSERT_EQ(false, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_NE(nullptr, incs->m_Next);

  incs = incs->m_Next;

  ASSERT_STREQ("foo/bar/baz.h", incs->m_String);
  ASSERT_EQ(13, incs->m_StringLen);
  ASSERT_EQ(true, incs->m_IsSystemInclude);
  ASSERT_EQ(true, incs->m_ShouldFollow);
  ASSERT_EQ(nullptr, incs->m_Next);
}
