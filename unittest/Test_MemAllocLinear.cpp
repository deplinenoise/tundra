#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

BEGIN_TEST_CASE("MemAllocLinear/init")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
    
  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  ASSERT_EQUAL(alloc.m_Offset, 0);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/alignment")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
    
  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  // Should start out at position 0
  ASSERT_EQUAL(alloc.m_Offset, 0);

  // Allocate an int, check alignment.
  int* iptr = LinearAllocate<int>(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, sizeof(int));
  ASSERT_EQUAL(uintptr_t(iptr) & 3, 0);

  // Allocate a character.
  LinearAllocate<char>(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, sizeof(int) + sizeof(char));

  // Allocate another int, check alignment.
  iptr = LinearAllocate<int>(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, sizeof(int) * 3);
  ASSERT_EQUAL(uintptr_t(iptr) & 3, 0);

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("MemAllocLinear/reset")
{
  MemAllocHeap heap;
  HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
    
  MemAllocLinear alloc;
  LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");

  ASSERT_EQUAL(alloc.m_Offset, 0);
  LinearAllocate<int>(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, sizeof(int));
  LinearAllocReset(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, 0);
  LinearAllocate<int>(&alloc);
  ASSERT_EQUAL(alloc.m_Offset, sizeof(int));

  LinearAllocDestroy(&alloc);
  HeapDestroy(&heap);
}
END_TEST_CASE
