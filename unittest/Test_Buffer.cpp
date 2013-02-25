#include "Buffer.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

BEGIN_TEST_CASE("Buffer/init")
{
  MemAllocHeap heap;
  HeapInit(&heap, 10 * 1024 * 1024, HeapFlags::kDefault);
  Buffer<int> b;

  BufferInit(&b);
  ASSERT_EQUAL(b.m_Storage, nullptr);
  ASSERT_EQUAL(b.m_Size, 0);

  BufferDestroy(&b, &heap);
  ASSERT_EQUAL(b.m_Storage, nullptr);

  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("Buffer/append")
{
  MemAllocHeap heap;
  HeapInit(&heap, 10 * 1024 * 1024, HeapFlags::kDefault);

  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAlloc(&b, &heap, 3);
  ASSERT_EQUAL(b.m_Size, 3);
  ptr[0] = 1;
  ptr[1] = 2;
  ptr[2] = 3;
  BufferDestroy(&b, &heap);
  ASSERT_EQUAL(b.m_Storage, nullptr);

  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("Buffer/zero")
{
  MemAllocHeap heap;
  HeapInit(&heap, 10 * 1024 * 1024, HeapFlags::kDefault);
  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAllocZero(&b, &heap, 3);
  ASSERT_EQUAL(b.m_Size, 3);
  ASSERT_EQUAL(ptr[0], 0);
  ASSERT_EQUAL(ptr[1], 0);
  ASSERT_EQUAL(ptr[2], 0);
  BufferDestroy(&b, &heap);
  ASSERT_EQUAL(b.m_Storage, nullptr);

  HeapDestroy(&heap);
}
END_TEST_CASE

BEGIN_TEST_CASE("Buffer/fill")
{
  MemAllocHeap heap;
  HeapInit(&heap, 10 * 1024 * 1024, HeapFlags::kDefault);
  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAllocFill(&b, &heap, 3, -1);
  ASSERT_EQUAL(b.m_Size, 3);
  ASSERT_EQUAL(ptr[0], -1);
  ASSERT_EQUAL(ptr[1], -1);
  ASSERT_EQUAL(ptr[2], -1);
  BufferDestroy(&b, &heap);
  ASSERT_EQUAL(b.m_Storage, nullptr);

  HeapDestroy(&heap);
}
END_TEST_CASE
