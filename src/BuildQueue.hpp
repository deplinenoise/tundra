#ifndef BUILDMANAGER_HPP
#define BUILDMANAGER_HPP

#include "Common.hpp"
#include "Mutex.hpp"
#include "ConditionVar.hpp"
#include "Thread.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"

namespace t2
{
  struct MemAllocHeap;
  struct NodeState;
  struct NodeData;
  struct ScanCache;
  struct StatCache;
  struct DigestCache;

  enum
  {
    kMaxBuildThreads = 64
  };

  struct BuildQueueConfig
  {
    enum
    {
      // Print command lines to the TTY as actions are executed.
      kFlagEchoCommandLines   = 1 << 0,
      // Print annotations to the TTY as actions are executed
      kFlagEchoAnnotations    = 1 << 1,
      // Continue building even if there are errors.
      kFlagContinueOnError    = 1 << 2
    };

    uint32_t        m_Flags;
    MemAllocHeap   *m_Heap;
    int             m_ThreadCount;
    const NodeData *m_NodeData;
    NodeState      *m_NodeState;
    int             m_MaxNodes;
    const int32_t  *m_NodeRemappingTable;
    ScanCache      *m_ScanCache;
    StatCache      *m_StatCache;
    DigestCache    *m_DigestCache;
    int             m_ShaDigestExtensionCount;
    const uint32_t* m_ShaDigestExtensions;
    void*           m_FileSigningLog;
    Mutex*          m_FileSigningLogMutex;
    int32_t         m_MaxExpensiveCount;
  };

  struct BuildQueue;
    
  struct ThreadState
  {
    MemAllocHeap      m_LocalHeap;
    MemAllocLinear    m_ScratchAlloc;
    int               m_ThreadIndex;
    BuildQueue*       m_Queue;
  };

  struct BuildQueue
  {
    Mutex              m_Lock;
    ConditionVariable  m_WorkAvailable;
    int32_t           *m_Queue;
    uint32_t           m_QueueCapacity;
    uint32_t           m_QueueReadIndex;
    uint32_t           m_QueueWriteIndex;
    BuildQueueConfig   m_Config;
    int32_t            m_PendingNodeCount;
    int32_t            m_FailedNodeCount;
    int32_t            m_CurrentPassIndex;
    ThreadId           m_Threads[kMaxBuildThreads];
    ThreadState        m_ThreadState[kMaxBuildThreads];
    int32_t            m_ExpensiveRunning;
    int32_t            m_ExpensiveWaitCount;
    NodeState        **m_ExpensiveWaitList;
    bool               m_QuitSignalled;
  };

  namespace BuildResult
  {
    enum Enum
    {
      kOk          = 0, // All nodes built successfully
      kInterrupted = 1, // User interrupted the build (e.g CTRL+C)
      kBuildError  = 2, // At least one node failed to build
      kSetupError  = 3, // We couldn't set up the build
      kCount
    };

    extern const char* Names[Enum::kCount];
  }

  void BuildQueueInit(BuildQueue* queue, const BuildQueueConfig* config);

  BuildResult::Enum BuildQueueBuildNodeRange(BuildQueue* queue, int start_index, int count, int pass_index);

  void BuildQueueDestroy(BuildQueue* queue);

}

#endif
