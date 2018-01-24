#include "Buffer.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

class BufferTest : public ::testing::Test
{
protected:
  MemAllocHeap heap;

protected:
  void SetUp() override
  {
    HeapInit(&heap);
  }

  void TearDown() override
  {
    HeapDestroy(&heap);
  }

};

TEST_F(BufferTest, Init)
{
  Buffer<int> b;

  BufferInit(&b);
  ASSERT_EQ(nullptr, b.m_Storage);
  ASSERT_EQ(0, b.m_Size);

  BufferDestroy(&b, &heap);
  ASSERT_EQ(nullptr, b.m_Storage);
}

TEST_F(BufferTest, Append)
{
  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAlloc(&b, &heap, 3);

  ASSERT_EQ(3, b.m_Size);

  ptr[0] = 1;
  ptr[1] = 2;
  ptr[2] = 3;

  BufferDestroy(&b, &heap);

  ASSERT_EQ(nullptr, b.m_Storage);
}

TEST_F(BufferTest, Zero)
{
  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAllocZero(&b, &heap, 3);
  ASSERT_EQ(3, b.m_Size);
  ASSERT_EQ(0, ptr[0]);
  ASSERT_EQ(0, ptr[1]);
  ASSERT_EQ(0, ptr[2]);
  BufferDestroy(&b, &heap);
  ASSERT_EQ(nullptr, b.m_Storage);
}

TEST_F(BufferTest, Fill)
{
  Buffer<int> b;

  BufferInit(&b);
  int* ptr = BufferAllocFill(&b, &heap, 3, -1);
  ASSERT_EQ(3, b.m_Size);
  ASSERT_EQ(-1, ptr[0]);
  ASSERT_EQ(-1, ptr[1]);
  ASSERT_EQ(-1, ptr[2]);
  BufferDestroy(&b, &heap);
  ASSERT_EQ(nullptr, b.m_Storage);
}
