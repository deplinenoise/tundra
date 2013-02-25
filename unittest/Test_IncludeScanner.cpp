#include "IncludeScanner.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

BEGIN_TEST_CASE("IncludeScanner/empty_file")
{
  char data[1] = { '\0' };
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_EQUAL(incs, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/single_include_newline")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  char data[] = "#include \"foo.h\"\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NOT_EQUAL(incs, nullptr);

  ASSERT_EQUAL_STRING(incs->m_String, "foo.h");
  ASSERT_EQUAL(incs->m_StringLen, 5);
  ASSERT_EQUAL(incs->m_IsSystemInclude, false);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_EQUAL(incs->m_Next, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/no_closing_terminator")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  char data[] = "#include <bar.h\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_EQUAL(incs, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/single_include_nonewline")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  char data[] = "#include <bar.h>";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NOT_EQUAL(data, nullptr);

  ASSERT_EQUAL_STRING(incs->m_String, "bar.h");
  ASSERT_EQUAL(incs->m_StringLen, 5);
  ASSERT_EQUAL(incs->m_IsSystemInclude, true);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_EQUAL(incs->m_Next, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/max_whitespace")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  char data[] = "\n\n   #      include     <bar.h>  \n\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NOT_EQUAL(data, nullptr);

  ASSERT_EQUAL_STRING(incs->m_String, "bar.h");
  ASSERT_EQUAL(incs->m_StringLen, 5);
  ASSERT_EQUAL(incs->m_IsSystemInclude, true);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_EQUAL(incs->m_Next, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/multi_includes")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);

  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  char data[] =
    "#include <foo.h>\n"
    "#include \"a.h\"\n"
    "#include <foo/bar/baz.h>\n";

  IncludeData* incs = ScanIncludesCpp(data, &alloc);
  ASSERT_NOT_EQUAL(data, nullptr);

  ASSERT_EQUAL_STRING(incs->m_String, "foo.h");
  ASSERT_EQUAL(incs->m_StringLen, 5);
  ASSERT_EQUAL(incs->m_IsSystemInclude, true);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_NOT_EQUAL(incs->m_Next, nullptr);

  incs = incs->m_Next;
  ASSERT_EQUAL_STRING(incs->m_String, "a.h");
  ASSERT_EQUAL(incs->m_StringLen, 3);
  ASSERT_EQUAL(incs->m_IsSystemInclude, false);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_NOT_EQUAL(incs->m_Next, nullptr);

  incs = incs->m_Next;
  ASSERT_EQUAL_STRING(incs->m_String, "foo/bar/baz.h");
  ASSERT_EQUAL(incs->m_StringLen, 13);
  ASSERT_EQUAL(incs->m_IsSystemInclude, true);
  ASSERT_EQUAL(incs->m_ShouldFollow, true);
  ASSERT_EQUAL(incs->m_Next, nullptr);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE
