#ifndef FILESIGN_HPP
#define FILESIGN_HPP

#include "Common.hpp"
#include "Hash.hpp"

namespace t2
{

struct HashState;
struct StatCache;
struct DigestCache;
struct MemAllocHeap;
struct MemAllocLinear;

void ComputeFileSignature(
  HashState*          out,                  // out
  StatCache*          stat_cache,
  DigestCache*        digest_cache,
  const char*         filename,
  uint32_t            fn_hash,
  const uint32_t      sha_extension_hashes[],
  int                 sha_extension_hash_count);

  HashDigest CalculateGlobSignatureFor(const char* path, MemAllocHeap* heap, MemAllocLinear* scratch);

  bool ShouldUseSHA1SignatureFor(const char* filename, const uint32_t sha_extension_hashes[], int sha_extension_hash_count);

}


#endif
