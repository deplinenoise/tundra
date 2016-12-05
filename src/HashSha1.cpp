#include "Hash.hpp"

namespace t2
{

#if ENABLED(USE_SHA1_HASH)

static inline uint32_t SHA1Rotate(uint32_t value, uint32_t bits)
{
  return ((value) << bits) | (value >> (32 - bits));
}

void HashBlock(const uint8_t* block, HashStateImpl* state, void* debug_file_)
{
  uint32_t w[80];

  // Prepare message schedule
  for (int i = 0; i < 16; ++i)
  {
    w[i] =
      (((uint32_t)block[(i*4)+0]) << 24) |
      (((uint32_t)block[(i*4)+1]) << 16) |
      (((uint32_t)block[(i*4)+2]) <<  8) |
      (((uint32_t)block[(i*4)+3]) <<  0);
  }

  for (int i = 16; i < 80; ++i)
  {
    w[i] = SHA1Rotate(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
  }

  // Initialize working variables
  uint32_t a = state->m_State[0];
  uint32_t b = state->m_State[1];
  uint32_t c = state->m_State[2];
  uint32_t d = state->m_State[3];
  uint32_t e = state->m_State[4];

  // This is the core loop for each 20-word span.
#define SHA1_LOOP(start, end, func, constant) \
  for (int i = (start); i < (end); ++i) \
  { \
    uint32_t t = SHA1Rotate(a, 5) + (func) + e + (constant) + w[i]; \
    e = d; d = c; c = SHA1Rotate(b, 30); b = a; a = t; \
  }

  SHA1_LOOP( 0, 20, ((b & c) ^ (~b & d)),           0x5a827999)
  SHA1_LOOP(20, 40, (b ^ c ^ d),                    0x6ed9eba1)
  SHA1_LOOP(40, 60, ((b & c) ^ (b & d) ^ (c & d)),  0x8f1bbcdc)
  SHA1_LOOP(60, 80, (b ^ c ^ d),                    0xca62c1d6)

#undef SHA1_LOOP

  // Update state
  state->m_State[0] += a;
  state->m_State[1] += b;
  state->m_State[2] += c;
  state->m_State[3] += d;
  state->m_State[4] += e;
}

void HashInitImpl(HashStateImpl* self)
{
  self->m_State[0] = 0x67452301;
  self->m_State[1] = 0xefcdab89;
  self->m_State[2] = 0x98badcfe;
  self->m_State[3] = 0x10325476;
  self->m_State[4] = 0xc3d2e1f0;
}

void HashFinalizeImpl(HashStateImpl* self, HashDigest* digest)
{
  // Copy digest bytes out (our 5 state words)
  for (int i = 0; i < 20; ++i)
  {
    uint32_t word = self->m_State[i >> 2];
    int byte = 3 - (i & 3);
    digest->m_Data[i] = (uint8_t) (word >> (byte * 8));
  }
}

#endif

}
