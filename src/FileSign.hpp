#ifndef FILESIGN_HPP
#define FILESIGN_HPP

namespace t2
{

struct HashState;
struct StatCache;
struct DigestCache;

void ComputeFileSignature(HashState* out, StatCache* stat_cache, DigestCache* digest_cache, const char* filename);

}

#endif