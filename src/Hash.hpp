#ifndef HASHER_HPP
#define HASHER_HPP

#include "Common.hpp"
#include <cstring>

namespace t2
{

// SHA-1 hash digest data
#pragma pack(push, 4)
union HashDigest 
{
  struct
  {
    uint64_t m_A;
    uint64_t m_B;
    uint32_t m_C;
  } m_Words;
  uint8_t  m_Data[20];
};
#pragma pack(pop)

inline int CompareHashDigests(const HashDigest& lhs, const HashDigest& rhs)
{
  const uint64_t l0 = LoadBigEndian64(lhs.m_Words.m_A);
  const uint64_t r0 = LoadBigEndian64(rhs.m_Words.m_A);

  const int res0 = (l0 > r0) - (l0 < r0);

  const uint64_t l1 = LoadBigEndian64(lhs.m_Words.m_B);
  const uint64_t r1 = LoadBigEndian64(rhs.m_Words.m_B);

  const int res1 = (l1 > r1) - (l1 < r1);

  const uint32_t l2 = LoadBigEndian32(lhs.m_Words.m_C);
  const uint32_t r2 = LoadBigEndian32(rhs.m_Words.m_C);

  const int res2 = (l2 > r2) - (l2 < r2);

  int result =  res0 ? res0 : res1 ? res1 : res2;

#if ENABLED(CHECKED_BUILD)
  int memcmp_result = memcmp(&lhs.m_Data[0], &rhs.m_Data[0], sizeof lhs.m_Data);
  if (memcmp_result < 0)
    memcmp_result = -1;
  else if (memcmp_result > 0)
    memcmp_result = 1;
  CHECK(memcmp_result == result);
#endif

  return result;
}

inline bool operator==(const HashDigest& lhs, const HashDigest& rhs)
{
  return CompareHashDigests(lhs, rhs) == 0;
}

inline bool operator!=(const HashDigest& lhs, const HashDigest& rhs)
{
  return CompareHashDigests(lhs, rhs) != 0;
}

inline bool operator<=(const HashDigest& lhs, const HashDigest& rhs)
{
  return CompareHashDigests(lhs, rhs) <= 0;
}

inline bool operator<(const HashDigest& lhs, const HashDigest& rhs)
{
  return CompareHashDigests(lhs, rhs) < 0;
}

static_assert(ALIGNOF(HashDigest) == 4, "struct layout");
static_assert(sizeof(HashDigest) == 20, "struct layout");

// SHA-1 hashing state
struct HashState
{
  uint32_t m_State[5];
  uint64_t m_MsgSize;
  size_t   m_BufUsed;
  uint8_t  m_Buffer[64];
};

// Initialize hashing state.
void HashInit(HashState* h);

// Add arbitrary data to be hashed.
void HashUpdate(HashState* h, const void* data, size_t size);

// Add string data to be hashed.
inline void HashAddString(HashState* self, const char* s)
{
  HashUpdate(self, s, strlen(s));
}

// Add binary integer data to be hashed.
void HashAddInteger(HashState* h, uint64_t value);

// Add a separator (zero byte) to keep runs of separate data apart.
void HashAddSeparator(HashState* h);

// Finalize hash and obtain hash digest.
// HashState should not be used after this.
void HashFinalize(HashState* h, HashDigest* digest);

// Generate null-terminated ASCII hex string representation from a hash digest.
void
DigestToString(char (&buffer)[41], const HashDigest& digest);

}

#endif

