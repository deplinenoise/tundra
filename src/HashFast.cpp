#include "Hash.hpp"

#include <cstdio>
#include <cctype>

// This is a 128-bit hash adapted from the xxhash project - https://code.google.com/p/xxhash/ 
//
// The idea is to compute 4 parallel 32-bit xxhash values and stash them next to each other.
// Because we're always hashing fixed sized blocks a lot of the inner loop complexity goes away.

namespace t2
{

#if ENABLED(USE_FAST_HASH)
static const uint32_t kPrime32_1 = 2654435761U;
static const uint32_t kPrime32_2 = 2246822519U;
static const uint32_t kPrime32_3 = 3266489917U;

static inline uint32_t RotateLeft(uint32_t value, int amount)
{
  return (value << amount) | (value >> (32 - amount));
}

void HashBlock(const uint8_t* block, HashStateImpl* state, void* debug_file_)
{
  const uint32_t* p = (const uint32_t*) block;
  const size_t buffer_size = sizeof(HashState().m_Buffer);

  if (FILE* debug_file = (FILE*) debug_file_)
  {
    const size_t line_size = 16;

    for (size_t i = 0; i < buffer_size; i += line_size)
    {
      for (size_t x = 0; x < line_size; ++x)
      {
        int ch = block[x + i];
        static const char hex[] = "0123456789ABCDEF";
        fputc(hex[(ch & 0xf0) >> 4], debug_file);
        fputc(hex[(ch & 0x0f)     ], debug_file);
        fputc(' ', debug_file);
      }

      fputs(" | ", debug_file);

      for (size_t x = 0; x < line_size; ++x)
      {
        int ch = block[x + i];
        if (isalnum(ch) || ispunct(ch) || ' ' == ch)
          fputc(ch, debug_file);
        else
          fputc('.', debug_file);
      }
      fputc('\n', debug_file);
    }
  }

  static_assert((buffer_size & 63) == 0, "buffer must be multiple of 64 bytes");

  for (size_t i = 0; i < buffer_size/64; ++i)
  {
    for (int v = 0; v < 4; ++v)
    {
      uint32_t v0 = state->m_V[v][0];
      uint32_t v1 = state->m_V[v][1];
      uint32_t v2 = state->m_V[v][2];
      uint32_t v3 = state->m_V[v][3];

      v0 += *p++ * kPrime32_2; v0 = RotateLeft(v0, 13); v0 *= kPrime32_1;
      v1 += *p++ * kPrime32_2; v1 = RotateLeft(v1, 13); v1 *= kPrime32_1;
      v2 += *p++ * kPrime32_2; v2 = RotateLeft(v2, 13); v2 *= kPrime32_1;
      v3 += *p++ * kPrime32_2; v3 = RotateLeft(v3, 13); v3 *= kPrime32_1;

      state->m_V[v][0] = v0;
      state->m_V[v][1] = v1;
      state->m_V[v][2] = v2;
      state->m_V[v][3] = v3;
    }
  }
}

void HashInitImpl(HashStateImpl* self)
{
  uint32_t seeds[4] = { 0x89caf13a, 0x179fa534, 0x5199afcc, 0xef901315 };

  for (int i = 0; i < 4; ++i)
  {
    uint32_t seed = seeds[i];
    self->m_V[i][0] = seed + kPrime32_1 + kPrime32_2;
    self->m_V[i][1] = seed + kPrime32_2;
    self->m_V[i][2] = seed;
    self->m_V[i][3] = seed - kPrime32_1;
  }
}

void HashFinalizeImpl(HashStateImpl* state, HashDigest* digest)
{
  for (int v = 0; v < 4; ++v)
  {
    uint32_t v0 = state->m_V[v][0];
    uint32_t v1 = state->m_V[v][1];
    uint32_t v2 = state->m_V[v][2];
    uint32_t v3 = state->m_V[v][3];

    uint32_t h32 = RotateLeft(v0, 1) + RotateLeft(v1, 7) + RotateLeft(v2, 12) + RotateLeft(v3, 18);

    h32 ^= h32 >> 15;
    h32 *= kPrime32_2;
    h32 ^= h32 >> 13;
    h32 *= kPrime32_3;
    h32 ^= h32 >> 16;

    digest->m_Words32[v] = h32;
  }
}

#endif

}
