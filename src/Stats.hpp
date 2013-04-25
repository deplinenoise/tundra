#ifndef STATS_HPP
#define STATS_HPP

#include "Common.hpp"
#include "Atomic.hpp"

namespace t2
{

struct TundraStats
{
  uint32_t m_NewScanCacheHits;
  uint32_t m_OldScanCacheHits;
  uint32_t m_ScanCacheMisses;
  uint32_t m_ScanCacheInserts;
  uint64_t m_ScanCacheSaveTime;
  uint32_t m_ScanCacheEntriesDropped;

  uint32_t m_StateSaveNew;
  uint32_t m_StateSaveOld;
  uint32_t m_StateSaveDropped;
  uint64_t m_StateSaveTimeCycles;

  uint32_t m_MmapCalls;
  uint64_t m_MmapTimeCycles;
  uint32_t m_MunmapCalls;
  uint64_t m_MunmapTimeCycles;

  uint32_t m_GlobCount;
  uint64_t m_GlobTimeCycles;

  uint32_t m_StatCount;
  uint64_t m_StatTimeCycles;
  uint32_t m_StatCacheHits;
  uint32_t m_StatCacheMisses;
  uint32_t m_StatCacheDirty;

  uint64_t m_StaleCheckTimeCycles;

  uint32_t m_ExecCount;
  uint64_t m_ExecTimeCycles;

  uint64_t m_JsonParseTimeCycles;

  uint64_t m_DigestCacheSaveTimeCycles;
  uint64_t m_DigestCacheGetTimeCycles;
  uint32_t m_DigestCacheHits;
  uint32_t m_FileDigestCount;
  uint64_t m_FileDigestTimeCycles;
};

struct TimingScope
{
  uint32_t* m_CountPtr;
  uint64_t* m_TimePtr;
  uint64_t  m_StartTime;

  TimingScope(uint32_t* count_ptr, uint64_t* time_ptr)
  {
    m_CountPtr  = count_ptr;
    m_TimePtr   = time_ptr;
    m_StartTime = TimerGet();
  }

  ~TimingScope()
  {
    uint64_t micros = TimerGet() - m_StartTime;
    if (uint32_t *ptr = m_CountPtr)
      AtomicIncrement(ptr);
    AtomicAdd(m_TimePtr, micros);
  }
};

extern TundraStats g_Stats;


}

#endif
