#include "Hash.hpp"

#include <cstring>

namespace t2
{

static inline uint32_t SHA1Rotate(uint32_t value, uint32_t bits)
{
  return ((value) << bits) | (value >> (32 - bits));
}

static void HashBlock(const uint8_t* block, uint32_t* state)
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
  uint32_t a = state[0];
  uint32_t b = state[1];
  uint32_t c = state[2];
  uint32_t d = state[3];
  uint32_t e = state[4];

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
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

void HashInit(HashState* self)
{
  self->m_MsgSize = 0;
  self->m_BufUsed = 0;
  static const uint32_t init_state[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
  memcpy(self->m_State, init_state, sizeof init_state);
}

void HashUpdate(HashState* self, const void *data_in, size_t size)
{
  const uint8_t*       data   = static_cast<const uint8_t*>(data_in);
  size_t               remain = size;
  uint8_t*             buffer = self->m_Buffer;
  uint32_t*            state  = self->m_State;
  size_t               used   = self->m_BufUsed;

  while (remain > 0)
  {
    if (used != 0 || remain < sizeof self->m_Buffer)
    {
      const size_t buf_space = (sizeof self->m_Buffer) - used;
      const size_t copy_size = remain < buf_space ? remain : buf_space;
      memcpy(buffer + used, data, copy_size);

      used   += copy_size;
      data   += copy_size;
      remain -= copy_size;

      if (used == sizeof self->m_Buffer)
      {
        HashBlock(buffer, state);
        used = 0;
      }
    }
    else
    {
      HashBlock(data, state);
      data   += sizeof self->m_Buffer;
      remain -= sizeof self->m_Buffer;
    }
  }
  
  self->m_BufUsed  = used;
  self->m_MsgSize += size * 8;
}

void HashFinalize(HashState* self, HashDigest* digest)
{
  uint8_t one_bit = 0x80;
  uint8_t count_data[8];

  // Generate size data in bit endian format
  for (int i = 0; i < 8; ++i)
  {
    count_data[i] = (uint8_t) (self->m_MsgSize >> (56 - i * 8));
  }

  // Set trailing one-bit
  HashUpdate(self, &one_bit, 1);

  // Emit null padding to to make room for 64 bits of size info in the last 512 bit block 
  static const uint8_t zeroes[128] = { 0 };

  int diff = int(self->m_BufUsed) - 56;

  if (diff < 0)
    HashUpdate(self, zeroes, -diff);
  else
    HashUpdate(self, zeroes, 64 - diff);

  CHECK(self->m_BufUsed == 56);

  // Write size in bits as last 64-bits
  HashUpdate(self, count_data, 8);

  // Make sure we actually finalized our last block
  CHECK(0 == self->m_BufUsed);

  // Copy digest bytes out (our 5 state words)
  for (int i = 0; i < 20; ++i)
  {
    uint32_t word = self->m_State[i >> 2];
    int byte = 3 - (i & 3);
    digest->m_Data[i] = (uint8_t) (word >> (byte * 8));
  }
}

void HashAddInteger(HashState* self, uint64_t value)
{
  uint8_t bytes[8];
  bytes[0] = uint8_t(value >> 56);
  bytes[1] = uint8_t(value >> 48);
  bytes[2] = uint8_t(value >> 40);
  bytes[3] = uint8_t(value >> 32);
  bytes[4] = uint8_t(value >> 24);
  bytes[5] = uint8_t(value >> 16);
  bytes[6] = uint8_t(value >>  8);
  bytes[7] = uint8_t(value >>  0);
  HashUpdate(self, bytes, sizeof bytes);
}

void HashAddSeparator(HashState* self)
{
  uint8_t zero = 0;
  HashUpdate(self, &zero, 1);
}

void
DigestToString(char (&buffer)[41], const HashDigest& digest)
{
  static const char hex[] = "0123456789abcdef";

  for (int i = 0; i < 20; ++i)
  {
    uint8_t byte = digest.m_Data[i];
    buffer[2 * i + 0] = hex[byte>>4];
    buffer[2 * i + 1] = hex[byte & 0xf];
  }

  buffer[40] = '\0';
}

// Quickie to generate a hash digest from a single string
void HashSingleString(HashDigest* digest_out, const char* string)
{
  HashState h;
  HashInit(&h);
  HashUpdate(&h, string, strlen(string));
  HashFinalize(&h, digest_out);
}

}
