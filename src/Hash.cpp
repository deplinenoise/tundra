#include "Hash.hpp"

#include <cstring>
#include <cstdio>

namespace t2
{

void HashInitImpl(HashStateImpl* impl);
void HashBlock(const uint8_t* data, HashStateImpl* state, void* debug_file);
void HashFinalizeImpl(HashStateImpl* self, HashDigest* digest);

void HashUpdate(HashState* self, const void *data_in, size_t size)
{
  const uint8_t*       data   = static_cast<const uint8_t*>(data_in);
  size_t               remain = size;
  uint8_t*             buffer = self->m_Buffer;
  HashStateImpl*       state  = &self->m_StateImpl;
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
        HashBlock(buffer, state, self->m_DebugFile);
        used = 0;
      }
    }
    else
    {
      HashBlock(data, state, self->m_DebugFile);
      data   += sizeof self->m_Buffer;
      remain -= sizeof self->m_Buffer;
    }
  }
  
  self->m_BufUsed  = used;
  self->m_MsgSize += size * 8;
}

// Quickie to generate a hash digest from a single string
void HashSingleString(HashDigest* digest_out, const char* string)
{
  HashState h;
  HashInit(&h);
  HashUpdate(&h, string, strlen(string));
  HashFinalize(&h, digest_out);
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

void HashInit(HashState* self)
{
  self->m_MsgSize = 0;
  self->m_BufUsed = 0;
  self->m_DebugFile = nullptr;
  HashInitImpl(&self->m_StateImpl);
}

void HashInitDebug(HashState* self, void* fh)
{
  self->m_MsgSize = 0;
  self->m_BufUsed = 0;
  self->m_DebugFile = fh;
  HashInitImpl(&self->m_StateImpl);
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

  // Emit null padding to to make room for 64 bits of size info in the last block 
  static const uint8_t zeroes[128] = { 0 };

  int diff = int(self->m_BufUsed) - (sizeof(self->m_Buffer) - 8);

  if (diff < 0)
    HashUpdate(self, zeroes, -diff);
  else
    HashUpdate(self, zeroes, sizeof(self->m_Buffer) - diff);

  CHECK(self->m_BufUsed == sizeof(self->m_Buffer) - 8);

  // Write size in bits as last 64-bits
  HashUpdate(self, count_data, 8);

  // Make sure we actually finalized our last block
  CHECK(0 == self->m_BufUsed);

  HashFinalizeImpl(&self->m_StateImpl, digest);
}

void
DigestToString(char (&buffer)[kDigestStringSize], const HashDigest& digest)
{
  static const char hex[] = "0123456789abcdef";

#if ENABLED(USE_FAST_HASH)
  int i = 0;
  for (int k = 0; k < 2; ++k)
  {
    uint64_t w = digest.m_Words64[k];
    for (int b = 0; b < 8; ++b)
    {
      uint8_t byte = w >> 56;
      buffer[i++] = hex[byte>>4];
      buffer[i++] = hex[byte & 0xf];
      w <<= 8;
    }
  }
#else
  for (size_t i = 0; i < sizeof(digest.m_Data); ++i)
  {
    uint8_t byte = digest.m_Data[i];
    buffer[2 * i + 0] = hex[byte>>4];
    buffer[2 * i + 1] = hex[byte & 0xf];
  }
#endif

  buffer[kDigestStringSize - 1] = '\0';
}


}
