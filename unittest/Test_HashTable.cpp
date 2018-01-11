#include "TestHarness.hpp"
#include "HashTable.hpp"

using namespace t2;

class HashTableTest : public ::testing::Test
{
protected:
  MemAllocHeap heap;

protected:
  void SetUp() override
  {
    HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
  }

  void TearDown() override
  {
    HeapDestroy(&heap);
  }
};

class HashSetTest : public ::testing::Test
{
protected:
  MemAllocHeap heap;

protected:
  void SetUp() override
  {
    HeapInit(&heap, 16 * 1024 * 1024, HeapFlags::kDefault);
  }

  void TearDown() override
  {
    HeapDestroy(&heap);
  }
};


TEST_F(HashTableTest, Empty)
{
  HashTable<int, kFlagCaseSensitive> tbl;
  HashTableInit(&tbl, &heap);
  EXPECT_EQ(0, tbl.m_RecordCount);
  HashTableDestroy(&tbl);
}

TEST_F(HashTableTest, Single)
{
  HashTable<int, kFlagCaseSensitive> tbl;
  HashTableInit(&tbl, &heap);
  HashTableInsert(&tbl, 1, "foo", 15);
  EXPECT_EQ(1, tbl.m_RecordCount);
  int* ptr = HashTableLookup(&tbl, 1, "foo");
  ASSERT_NE(nullptr, ptr);
  ASSERT_EQ(15, *ptr);
  HashTableDestroy(&tbl);
}

TEST_F(HashTableTest, MultiDistinct)
{
  HashTable<int, kFlagCaseSensitive> tbl;
  HashTableInit(&tbl, &heap);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    HashTableInsert(&tbl, Djb2Hash(str), str, i);
  }
  EXPECT_EQ(2048, tbl.m_RecordCount);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    int* ptr = HashTableLookup(&tbl, Djb2Hash(str), str);
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(i, *ptr);
  }
  HashTableDestroy(&tbl);
}

TEST_F(HashTableTest, MultiDistinctCaseFolded)
{
  HashTable<int, kFlagCaseInsensitive> tbl;
  HashTableInit(&tbl, &heap);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    HashTableInsert(&tbl, Djb2HashNoCase(str), str, i);
  }
  EXPECT_EQ(2048, tbl.m_RecordCount);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "fOO%d", i);
    int* ptr = HashTableLookup(&tbl, Djb2HashNoCase(str), str);
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(i, *ptr);
  }
  HashTableDestroy(&tbl);
}

TEST_F(HashSetTest, Empty)
{
  HashSet<kFlagCaseSensitive> tbl;
  HashSetInit(&tbl, &heap);
  EXPECT_EQ(0, tbl.m_RecordCount);
  HashSetDestroy(&tbl);
}

TEST_F(HashSetTest, Single)
{
  HashSet<kFlagCaseSensitive> tbl;
  HashSetInit(&tbl, &heap);
  HashSetInsert(&tbl, 1, "foo");
  EXPECT_EQ(1, tbl.m_RecordCount);
  ASSERT_TRUE(HashSetLookup(&tbl, 1, "foo"));
  HashSetDestroy(&tbl);
}

TEST_F(HashSetTest, MultiDistinct)
{
  HashSet<kFlagCaseSensitive> tbl;
  HashSetInit(&tbl, &heap);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    HashSetInsert(&tbl, Djb2Hash(str), str);
  }
  EXPECT_EQ(2048, tbl.m_RecordCount);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    EXPECT_TRUE(HashSetLookup(&tbl, Djb2Hash(str), str));
  }
  HashSetDestroy(&tbl);
}

TEST_F(HashSetTest, MultiDistinctCaseFolded)
{
  HashSet<kFlagCaseInsensitive> tbl;
  HashSetInit(&tbl, &heap);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "foo%d", i);
    HashSetInsert(&tbl, Djb2HashNoCase(str), str);
  }
  EXPECT_EQ(2048, tbl.m_RecordCount);

  for (int i = 0; i < 2048; ++i)
  {
    char str[128];
    sprintf(str, "fOO%d", i);
    EXPECT_TRUE(HashSetLookup(&tbl, Djb2HashNoCase(str), str));
  }
  HashSetDestroy(&tbl);
}
