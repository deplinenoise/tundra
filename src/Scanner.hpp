#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "Common.hpp"

// High-level include scanner

namespace t2
{

struct ScannerData;
struct MemAllocLinear;
struct MemAllocHeap;
struct ScanCache;
struct StatCache;

struct ScanInput
{
  const ScannerData *m_ScannerConfig;
  MemAllocLinear    *m_ScratchAlloc;
  MemAllocHeap      *m_ScratchHeap;
  const char        *m_FileName;
  ScanCache         *m_ScanCache;
};

struct ScanOutput
{
  int                m_IncludedFileCount;
  const FileAndHash *m_IncludedFiles;
};

bool ScanImplicitDeps(StatCache* stat_cache, const ScanInput* input, ScanOutput* output);

}

#endif
