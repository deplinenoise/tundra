#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"
#include "TestHarness.hpp"

using namespace t2;

class MemAllocLinearTest : public ::testing::Test
{
public:
  MemAllocHeap heap;
  MemAllocLinear alloc;

protected:
  void SetUp() override
  {
    HeapInit(&heap);
    LinearAllocInit(&alloc, &heap, 10 * 1024 * 1024, "Test Allocator");
  }

  void TearDown() override
  {
    LinearAllocDestroy(&alloc);
    HeapDestroy(&heap);
  }
};


TEST_F(MemAllocLinearTest, Init)
{
  ASSERT_EQ(0, alloc.m_Offset);
}

TEST_F(MemAllocLinearTest, Alignmenment)
{
  // Allocate an int, check alignment.
  int* iptr = LinearAllocate<int>(&alloc);
  ASSERT_EQ(sizeof(int), alloc.m_Offset);
  ASSERT_EQ(0, uintptr_t(iptr) & 3);

  // Allocate a character.
  LinearAllocate<char>(&alloc);
  ASSERT_EQ(sizeof(int) + sizeof(char), alloc.m_Offset);

  // Allocate another int, check alignment.
  iptr = LinearAllocate<int>(&alloc);
  ASSERT_EQ(sizeof(int) * 3, alloc.m_Offset);
  ASSERT_EQ(0, uintptr_t(iptr) & 3);
}

TEST_F(MemAllocLinearTest, Reset)
{
  ASSERT_EQ(0, alloc.m_Offset);
  LinearAllocate<int>(&alloc);

  ASSERT_EQ(sizeof(int), alloc.m_Offset);
  LinearAllocReset(&alloc);

  ASSERT_EQ(0, alloc.m_Offset);
  LinearAllocate<int>(&alloc);

  ASSERT_EQ(sizeof(int), alloc.m_Offset);
}
