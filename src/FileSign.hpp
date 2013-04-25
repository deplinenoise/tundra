#ifndef FILESIGN_HPP
#define FILESIGN_HPP

#include "Common.hpp"

namespace t2
{

struct HashState;
struct StatCache;
struct DigestCache;

void ComputeFileSignature(HashState* out, StatCache* stat_cache, DigestCache* digest_cache, const char* filename, uint32_t fn_hash);

}

#endif
