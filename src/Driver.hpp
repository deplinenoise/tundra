#ifndef DRIVER_HPP
#define DRIVER_HPP

#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"
#include "MemoryMappedFile.hpp"
#include "NodeState.hpp"
#include "BuildQueue.hpp"
#include "Buffer.hpp"
#include "ScanCache.hpp"
#include "StatCache.hpp"
#include "DigestCache.hpp"

namespace t2
{

struct DagData;
struct ScanData;
struct StateData;

struct DriverOptions
{
  bool        m_ShowHelp;
  bool        m_DryRun;
  bool        m_ForceDagRegen;
  bool        m_ShowTargets;
  bool        m_DebugMessages;
  bool        m_Verbose;
  bool        m_SpammyVerbose;
  bool        m_DisplayStats;
  bool        m_GenDagOnly;
  bool        m_Quiet;
  bool        m_IdeGen;
  bool        m_Clean;
  bool        m_Rebuild;
  bool        m_DebugSigning;
  bool        m_ContinueOnError;
#if defined(TUNDRA_WIN32)
  bool        m_RunUnprotected;
#endif
  bool        m_QuickstartGen;
  int         m_ThreadCount;
  const char *m_WorkingDir;
  const char *m_DAGFileName;
  const char *m_ProfileOutput;
};

void DriverOptionsInit(DriverOptions* self);

struct Driver
{
  enum
  {
    kMaxPasses = 64
  };

  MemAllocHeap      m_Heap;
  MemAllocLinear    m_Allocator;

  // Read-only memory mapped data - DAG data
  MemoryMappedFile  m_DagFile;

  // Read-only memory mapped data - previous build state
  MemoryMappedFile  m_StateFile;

  // Read-only memory mapped data - header scanning cache
  MemoryMappedFile  m_ScanFile;

  // Stores pointers to mmaped data.
  const DagData*    m_DagData;
  const StateData*  m_StateData;
  const ScanData*   m_ScanData;

  DriverOptions     m_Options;

  // Remapping table from dag data node index => node state
  Buffer<int32_t> m_NodeRemap;

  // Space for dynamic DAG node state
  Buffer<NodeState> m_Nodes;

  MemAllocLinear    m_ScanCacheAllocator;
  ScanCache         m_ScanCache;

  MemAllocLinear    m_StatCacheAllocator;
  StatCache         m_StatCache;

  DigestCache       m_DigestCache;

  int32_t           m_PassNodeCount[kMaxPasses];
};

bool DriverInit(Driver* self, const DriverOptions* options);

bool DriverPrepareNodes(Driver* self, const char** targets, int target_count);

void DriverDestroy(Driver* self);

void DriverShowHelp(Driver* self);

void DriverShowTargets(Driver* self);

void DriverRemoveStaleOutputs(Driver* self);

void DriverCleanOutputs(Driver* self);

BuildResult::Enum DriverBuild(Driver* self);

bool DriverInitData(Driver* self);

bool DriverSaveScanCache(Driver* self);
bool DriverSaveBuildState(Driver* self);
bool DriverSaveDigestCache(Driver* self);

void DriverInitializeTundraFilePaths(DriverOptions* driverOptions);

}

#endif
