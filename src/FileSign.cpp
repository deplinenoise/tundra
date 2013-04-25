#include "FileSign.hpp"
#include "Hash.hpp"
#include "StatCache.hpp"
#include "Stats.hpp"
#include "DigestCache.hpp"

#include <stdio.h>

namespace t2
{

static void ComputeFileSignatureSha1(HashState* state, StatCache* stat_cache, DigestCache* digest_cache, const char* filename, uint32_t fn_hash)
{
  FileInfo file_info = StatCacheStat(stat_cache, filename, fn_hash);

  if (!file_info.Exists())
  {
    HashAddInteger(state, ~0ull);
    return;
  }

  HashDigest digest;

  if (!DigestCacheGet(digest_cache, filename, fn_hash, file_info.m_Timestamp, &digest))
  {
    TimingScope timing_scope(&g_Stats.m_FileDigestCount, &g_Stats.m_FileDigestTimeCycles);

    FILE* f = fopen(filename, "rb");
    if (!f)
    {
      HashAddString(state, "<missing>");
      return;
    }

    HashState h;
    HashInit(&h);

    char buffer[8192];
    while (size_t nbytes = fread(buffer, 1, sizeof buffer, f))
    {
      HashUpdate(&h, buffer, nbytes);
    }
    fclose(f);

    HashFinalize(&h, &digest);
    DigestCacheSet(digest_cache, filename, fn_hash, file_info.m_Timestamp, digest);
  }
  else
  {
    AtomicIncrement(&g_Stats.m_DigestCacheHits);
  }

  HashUpdate(state, &digest, sizeof(digest));
}

static bool ComputeFileSignatureTimestamp(HashState* out, StatCache* stat_cache, const char* filename, uint32_t hash)
{
  FileInfo info = StatCacheStat(stat_cache, filename, hash);
  if (info.Exists())
    HashAddInteger(out, info.m_Timestamp);
  else
    HashAddInteger(out, ~0ull);
  return false;
}

void ComputeFileSignature(HashState* state, StatCache* stat_cache, DigestCache* digest_cache, const char* filename, uint32_t fn_hash)
{
  if (const char* ext = strrchr(filename, '.'))
  {
    if (0 == strcmp(ext, ".cpp") ||
        0 == strcmp(ext, ".c") ||
        0 == strcmp(ext, ".h") ||
        0 == strcmp(ext, ".hpp"))
    {
      ComputeFileSignatureSha1(state, stat_cache, digest_cache, filename, fn_hash);
      return;
    }
  }

  ComputeFileSignatureTimestamp(state, stat_cache, filename, fn_hash);
}

}
