#ifndef FILESIGN_HPP
#define FILESIGN_HPP

#include "Common.hpp"

namespace t2
{

struct HashState;
struct StatCache;
struct DigestCache;

void ComputeFileSignature(
  HashState*          out,                  // out
  StatCache*          stat_cache,
  DigestCache*        digest_cache,
  const char*         filename,
  uint32_t            fn_hash,
  const uint32_t      sha_extension_hashes[],
  int                 sha_extension_hash_count);
}

#endif
