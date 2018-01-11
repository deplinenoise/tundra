#include "BuildQueue.hpp"
#include "DagData.hpp"
#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"
#include "NodeState.hpp"
#include "Scanner.hpp"
#include "FileInfo.hpp"
#include "StateData.hpp"
#include "SignalHandler.hpp"
#include "Exec.hpp"
#include "Stats.hpp"
#include "StatCache.hpp"
#include "FileSign.hpp"

#include <stdio.h>

namespace t2
{
  namespace BuildResult
  {
    const char* Names[Enum::kCount] =
    {
      "Build success",
      "Build interrupted",
      "Build failed",
      "Build failed to setup error"
    };
  }


  static void ThreadStateInit(ThreadState* self, BuildQueue* queue, size_t heap_size, size_t scratch_size, int index)
  {
    HeapInit(&self->m_LocalHeap, heap_size, HeapFlags::kDefault);
    LinearAllocInit(&self->m_ScratchAlloc, &self->m_LocalHeap, scratch_size, "thread-local scratch");
    self->m_ThreadIndex = index;
    self->m_Queue       = queue;
  }

  static void ThreadStateDestroy(ThreadState* self)
  {
    LinearAllocDestroy(&self->m_ScratchAlloc);
    HeapDestroy(&self->m_LocalHeap);
  }

  static int AvailableNodeCount(BuildQueue* queue)
  {
    const uint32_t queue_mask  = queue->m_QueueCapacity - 1;
    uint32_t       read_index  = queue->m_QueueReadIndex;
    uint32_t       write_index = queue->m_QueueWriteIndex;

    return (write_index - read_index) & queue_mask;
  }

  static NodeState* GetStateForNode(BuildQueue* queue, int32_t src_index)
  {
    int32_t state_index = queue->m_Config.m_NodeRemappingTable[src_index];

    if (state_index == -1)
      return nullptr;

    NodeState* state = queue->m_Config.m_NodeState + state_index;

    CHECK(int(state->m_MmapData - queue->m_Config.m_NodeData) == src_index);

    return state;
  }


  static bool AllDependenciesReady(BuildQueue* queue, const NodeState* state)
  {
    const NodeData *src_node      = state->m_MmapData;

    for (int32_t dep_index : src_node->m_Dependencies)
    {
      NodeState* state = GetStateForNode(queue, dep_index);

      CHECK(state != nullptr);

      if (!NodeStateIsCompleted(state))
      {
        return false;
      }
    }

    return true;
  }


  static void WakeWaiters(BuildQueue* queue, int count)
  {
    if (count > 1)
      CondBroadcast(&queue->m_WorkAvailable);
    else
      CondSignal(&queue->m_WorkAvailable);
  }

  static void Enqueue(BuildQueue* queue, NodeState* state)
  {
    uint32_t       write_index = queue->m_QueueWriteIndex;
    const uint32_t queue_mask  = queue->m_QueueCapacity - 1;
    int32_t*       build_queue = queue->m_Queue;


    CHECK(AllDependenciesReady(queue, state));
    CHECK(!NodeStateIsQueued(state));
    CHECK(!NodeStateIsActive(state));
    CHECK(!NodeStateIsCompleted(state));
    CHECK(state->m_MmapData->m_PassIndex == queue->m_CurrentPassIndex);

#if ENABLED(CHECKED_BUILD)
    const int avail_init = AvailableNodeCount(queue);
#endif

    int state_index = int(state - queue->m_Config.m_NodeState);

    build_queue[write_index] = state_index;
    write_index              = (write_index + 1) & queue_mask;
    queue->m_QueueWriteIndex = write_index;

    NodeStateFlagQueued(state);

    CHECK(AvailableNodeCount(queue) == 1 + avail_init);
  }

  static void ParkExpensiveNode(BuildQueue* queue, NodeState* state)
  {
    NodeStateFlagQueued(state);
    CHECK(queue->m_ExpensiveWaitCount < (int) queue->m_QueueCapacity);
    queue->m_ExpensiveWaitList[queue->m_ExpensiveWaitCount++] = state;
  }

  static void UnparkExpensiveNode(BuildQueue* queue)
  {
    if (queue->m_ExpensiveWaitCount > 0)
    {
      NodeState* node = queue->m_ExpensiveWaitList[--queue->m_ExpensiveWaitCount];
      CHECK(NodeStateIsQueued(node));
      // Really only to avoid tripping up checks in Enqueue()
      NodeStateFlagUnqueued(node);
      NodeStateFlagInactive(node);
      Enqueue(queue, node);
      CondSignal(&queue->m_WorkAvailable);
    }
  }

  static BuildProgress::Enum SetupDependencies(BuildQueue* queue, NodeState* node)
  {
    const NodeData *src_node         = node->m_MmapData;
    int             dep_waits_needed = 0;
    int             enqueue_count    = 0;

    // Go through all dependencies and see how those nodes are doing.  If any
    // of them are not finished, we'll have to wait before this node can continue
    // to advance its state machine.
    for (int32_t dep_index : src_node->m_Dependencies)
    {
      NodeState* state = GetStateForNode(queue, dep_index);

      CHECK(state != nullptr);

      CHECK(state->m_MmapData->m_PassIndex <= src_node->m_PassIndex);

      if (NodeStateIsCompleted(state))
        continue;

      ++dep_waits_needed;

      if (!NodeStateIsQueued(state) && !NodeStateIsActive(state) && !NodeStateIsBlocked(state))
      {
        Enqueue(queue, state);
        ++enqueue_count;
      }
    }

    if (enqueue_count > 0)
      WakeWaiters(queue, enqueue_count);

    // We're waiting on dependencies to be ready.
    if (dep_waits_needed > 0)
      return BuildProgress::kBlocked;

    return BuildProgress::kUnblocked;
  }

  static bool OutputFilesDiffer(const NodeData* node_data, const NodeStateData* prev_state)
  {
    int file_count = node_data->m_OutputFiles.GetCount();

    if (file_count != prev_state->m_OutputFiles.GetCount())
      return true;
    
    for (int i = 0; i < file_count; ++i)
    {
      if (0 != strcmp(node_data->m_OutputFiles[i].m_Filename, prev_state->m_OutputFiles[i]))
        return true;
    }

    return false;
  }

  static bool OutputFilesMissing(StatCache* stat_cache, const NodeData* node)
  {
    for (const FrozenFileAndHash& f : node->m_OutputFiles)
    {
      FileInfo i = StatCacheStat(stat_cache, f.m_Filename, f.m_FilenameHash);

      if (!i.Exists())
        return true;
    }

    return false;
  }

  static bool MakeDirectoriesRecursive(StatCache* stat_cache, const PathBuffer& dir)
  {
    PathBuffer parent_dir = dir;
    PathStripLast(&parent_dir);

    // Can't go any higher.
    if (dir == parent_dir)
      return true;

    if (!MakeDirectoriesRecursive(stat_cache, parent_dir))
      return false;

    char path[kMaxPathLength];
    PathFormat(path, &dir);

    FileInfo info = StatCacheStat(stat_cache, path);

    if (info.Exists())
    {
      // Just asssume this is a directory. We could check it - but there's currently no way via _stat64() calls
      // on Windows to check if a file is a symbolic link (to a directory).
      return true;
    }
    else
    {
      Log(kSpam, "create dir \"%s\"", path);
      bool success = MakeDirectory(path);
      StatCacheMarkDirty(stat_cache, path, Djb2HashPath(path));
      return success;
    }
  }

  static bool MakeDirectoriesForFile(StatCache* stat_cache, const PathBuffer& buffer)
  {
    PathBuffer path = buffer;
    PathStripLast(&path);
    return MakeDirectoriesRecursive(stat_cache, path);
  }

  static BuildProgress::Enum CheckInputSignature(BuildQueue* queue, ThreadState* thread_state, NodeState* node, Mutex* queue_lock)
  {
    CHECK(AllDependenciesReady(queue, node));

    MutexUnlock(queue_lock);

    const BuildQueueConfig& config = queue->m_Config;
    StatCache* stat_cache = config.m_StatCache;
    DigestCache* digest_cache = config.m_DigestCache;

    const NodeData* node_data = node->m_MmapData;

    HashState sighash;
    FILE* debug_log = (FILE*) queue->m_Config.m_FileSigningLog;

    if (debug_log)
    {
      MutexLock(queue->m_Config.m_FileSigningLogMutex);
      fprintf(debug_log, "input_sig(\"%s\"):\n", node_data->m_Annotation.Get());
      HashInitDebug(&sighash, debug_log);
    }
    else
    {
      HashInit(&sighash);
    }

    // Start with command line action. If that changes, we'll definitely have to rebuild.
    HashAddString(&sighash, node_data->m_Action);
    HashAddSeparator(&sighash);

    if (const char* pre_action = node_data->m_PreAction)
    {
      HashAddString(&sighash, pre_action);
      HashAddSeparator(&sighash);
    }

    const ScannerData* scanner = node_data->m_Scanner;

    for (const FrozenFileAndHash& input : node_data->m_InputFiles)
    {
      // Add path and timestamp of every direct input file.
      HashAddString(&sighash, input.m_Filename);
      ComputeFileSignature(&sighash, stat_cache, digest_cache, input.m_Filename, input.m_FilenameHash, config.m_ShaDigestExtensions, config.m_ShaDigestExtensionCount);

      if (scanner)
      {
        // Roll back scratch allocator between scans
        MemAllocLinearScope alloc_scope(&thread_state->m_ScratchAlloc);

        ScanInput scan_input;
        scan_input.m_ScannerConfig = scanner;
        scan_input.m_ScratchAlloc  = &thread_state->m_ScratchAlloc;
        scan_input.m_ScratchHeap   = &thread_state->m_LocalHeap;
        scan_input.m_FileName      = input.m_Filename;
        scan_input.m_ScanCache     = queue->m_Config.m_ScanCache;

        ScanOutput scan_output;

        if (ScanImplicitDeps(stat_cache, &scan_input, &scan_output))
        {
          for (int i = 0, count = scan_output.m_IncludedFileCount; i < count; ++i)
          {
            // Add path and timestamp of every indirect input file (#includes)
            const FileAndHash& path = scan_output.m_IncludedFiles[i];
            HashAddString(&sighash, path.m_Filename);
            ComputeFileSignature(&sighash, stat_cache, digest_cache, path.m_Filename, path.m_FilenameHash, config.m_ShaDigestExtensions, config.m_ShaDigestExtensionCount);
          }
        }
      }
    }

    HashFinalize(&sighash, &node->m_InputSignature);

    if (debug_log)
    {
      char sig[kDigestStringSize];
      DigestToString(sig, node->m_InputSignature);
      fprintf(debug_log, "  => %s\n", sig);
      MutexUnlock(queue->m_Config.m_FileSigningLogMutex);
    }

    // Figure out if we need to rebuild this node.
    const NodeStateData* prev_state = node->m_MmapState;

    BuildProgress::Enum next_state;

    if (!prev_state)
    {
      // This is a new node - we must built it
      Log(kSpam, "T=%d: building %s - new node", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kRunAction;
    }
    else if (prev_state->m_InputSignature != node->m_InputSignature)
    {
      // The input signature has changed (either direct inputs or includes)
      // We need to rebuild this node.
      Log(kSpam, "T=%d: building %s - input signature changed", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kRunAction;
    }
    else if (prev_state->m_BuildResult != 0)
    {
      // The build progress failed the last time around - we need to retry it.
      Log(kSpam, "T=%d: building %s - previous build failed", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kRunAction;
    }
    else if (OutputFilesDiffer(node_data, prev_state))
    {
      // The output files are different - need to rebuild.
      Log(kSpam, "T=%d: building %s - output files have changed", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kRunAction;
    }
    else if (OutputFilesMissing(stat_cache, node_data))
    {
      // One or more output files are missing - need to rebuild.
      Log(kSpam, "T=%d: building %s - output files are missing", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kRunAction;
    }
    else
    {
      // Everything is up to date
      Log(kSpam, "T=%d: %s - up to date", thread_state->m_ThreadIndex, node_data->m_Annotation.Get());
      next_state = BuildProgress::kUpToDate;
    }

    MutexLock(queue_lock);

    return next_state;
  }

  static BuildProgress::Enum RunAction(BuildQueue* queue, ThreadState* thread_state, NodeState* node, Mutex* queue_lock)
  {
    const NodeData    *node_data    = node->m_MmapData;
    const char        *cmd_line     = node_data->m_Action;
    const char        *pre_cmd_line = node_data->m_PreAction;

    if (!cmd_line || cmd_line[0] == '\0')
      return BuildProgress::kSucceeded;

    if (node->m_MmapData->m_Flags & NodeData::kFlagExpensive)
    {
      if (queue->m_ExpensiveRunning == queue->m_Config.m_MaxExpensiveCount)
      {
        ParkExpensiveNode(queue, node);
        return BuildProgress::kRunAction;
      }
      else
      {
        ++queue->m_ExpensiveRunning;
      }
    }

    MutexUnlock(queue_lock);

    StatCache         *stat_cache   = queue->m_Config.m_StatCache;
    const char        *annotation   = node_data->m_Annotation;
    int                job_id       = thread_state->m_ThreadIndex;
    int                echo_cmdline = 0 != (queue->m_Config.m_Flags & BuildQueueConfig::kFlagEchoCommandLines);

    // Repack frozen env to pointers on the stack.
    int                env_count    = node_data->m_EnvVars.GetCount();
    EnvVariable*       env_vars     = (EnvVariable*) alloca(env_count * sizeof(EnvVariable));
    for (int i = 0; i < env_count; ++i)
    {
      env_vars[i].m_Name  = node_data->m_EnvVars[i].m_Name;
      env_vars[i].m_Value = node_data->m_EnvVars[i].m_Value;
    }

    for (const FrozenFileAndHash& output_file : node_data->m_OutputFiles)
    {
      PathBuffer output;
      PathInit(&output, output_file.m_Filename);

      if (!MakeDirectoriesForFile(stat_cache, output))
      {
        Log(kError, "failed to create output directories for %s", output_file.m_Filename.Get());
        MutexLock(queue_lock);
        return BuildProgress::kFailed;
      }
    }

    ExecResult result = { 0, false };

    // See if we need to remove the output files before running anything.
    if (0 == (node_data->m_Flags & NodeData::kFlagOverwriteOutputs))
    {
      for (const FrozenFileAndHash& output : node_data->m_OutputFiles)
      {
        Log(kDebug, "Removing output file %s before running action", output.m_Filename.Get());
        remove(output.m_Filename);
        StatCacheMarkDirty(stat_cache, output.m_Filename, output.m_FilenameHash);
      }
    }

    if (pre_cmd_line)
    {
      Log(kSpam, "Launching pre-action process");
      TimingScope timing_scope(&g_Stats.m_ExecCount, &g_Stats.m_ExecTimeCycles);
      result = ExecuteProcess(pre_cmd_line, env_count, env_vars, job_id, echo_cmdline, "(pre-build command)");
      Log(kSpam, "Process return code %d", result.m_ReturnCode);
    }

    if (0 == result.m_ReturnCode)
    {
      Log(kSpam, "Launching process");
      TimingScope timing_scope(&g_Stats.m_ExecCount, &g_Stats.m_ExecTimeCycles);
      result = ExecuteProcess(cmd_line, env_count, env_vars, job_id, echo_cmdline, annotation);
      Log(kSpam, "Process return code %d", result.m_ReturnCode);
    }

    for (const FrozenFileAndHash& output : node_data->m_OutputFiles)
    {
      StatCacheMarkDirty(stat_cache, output.m_Filename, output.m_FilenameHash);
    }

    MutexLock(queue_lock);

    if (result.m_WasSignalled)
    {
      SignalSet("child processes signalled");
    }

    if (0 == result.m_ReturnCode)
    {
      return BuildProgress::kSucceeded;
    }
    else
    {
      // Clean up output files after a failed build unless they are precious.
      if (0 == (NodeData::kFlagPreciousOutputs & node_data->m_Flags))
      {
        for (const FrozenFileAndHash& output : node_data->m_OutputFiles)
        {
          Log(kDebug, "Removing output file %s from failed build", output.m_Filename.Get());
          remove(output.m_Filename);
          StatCacheMarkDirty(stat_cache, output.m_Filename, output.m_FilenameHash);
        }
      }

      return BuildProgress::kFailed;
    }
  }

  static void UnblockWaiters(BuildQueue* queue, NodeState* node)
  {
    const NodeData *src_node       = node->m_MmapData;
    int             enqueue_count  = 0;

    for (int32_t link : src_node->m_BackLinks)
    {
      if (NodeState* waiter = GetStateForNode(queue, link))
      {
        // Only wake nodes in our current pass
        if (waiter->m_MmapData->m_PassIndex != queue->m_CurrentPassIndex)
          continue;

        // If the node isn't ready, skip it.
        if (!AllDependenciesReady(queue, waiter))
          continue;

        // Did someone else get to the node first?
        if (NodeStateIsQueued(waiter) || NodeStateIsActive(waiter))
          continue;

        //printf("%s is ready to go\n", GetSourceNode(queue, waiter)->m_Annotation);
        Enqueue(queue, waiter);
        ++enqueue_count;
      }
    }

    if (enqueue_count > 0)
      WakeWaiters(queue, enqueue_count);
  }

  static void AdvanceNode(BuildQueue* queue, ThreadState* thread_state, NodeState* node, Mutex* queue_lock)
  {
    Log(kSpam, "T=%d, [%d] Advancing %s\n",
        thread_state->m_ThreadIndex, node->m_Progress, node->m_MmapData->m_Annotation.Get());

    CHECK(!NodeStateIsCompleted(node));
    CHECK(NodeStateIsActive(node));
    CHECK(!NodeStateIsQueued(node));

    for (;;)
    {
      switch (node->m_Progress)
      {
        case BuildProgress::kInitial:
          node->m_Progress = SetupDependencies(queue, node);

          if (BuildProgress::kBlocked == node->m_Progress)
          {
            // Set ourselves as inactive until our dependencies are ready.
            NodeStateFlagInactive(node);
            return;
          }
          else
            break;

        case BuildProgress::kBlocked:
          CHECK(AllDependenciesReady(queue, node));
          node->m_Progress = BuildProgress::kUnblocked;
          break;

        case BuildProgress::kUnblocked:
          node->m_Progress = CheckInputSignature(queue, thread_state, node, queue_lock);
          break;

        case BuildProgress::kRunAction:
          node->m_Progress = RunAction(queue, thread_state, node, queue_lock);

          // If we couldn't make progress, we're a parked expensive node.
          // Another expensive job will put us back on the queue later when it
          // has finshed.
          if (BuildProgress::kRunAction == node->m_Progress)
            return;

          // Otherwise, we just ran our action. If we were an expensive node,
          // make sure to let other expensive nodes on to the cores now.
          if (node->m_MmapData->m_Flags & NodeData::kFlagExpensive)
          {
            --queue->m_ExpensiveRunning;
            CHECK(queue->m_ExpensiveRunning >= 0);

            // We were an expensive job. We can unpark another expensive job if
            // anything is waiting.
            UnparkExpensiveNode(queue);
          }
          break;

        case BuildProgress::kSucceeded:
        case BuildProgress::kUpToDate:
          node->m_BuildResult = 0;
          node->m_Progress    = BuildProgress::kCompleted;
          break;

        case BuildProgress::kFailed:
          queue->m_FailedNodeCount++;

          CondBroadcast(&queue->m_WorkAvailable);

          node->m_BuildResult = 1;
          node->m_Progress    = BuildProgress::kCompleted;
          break;

        case BuildProgress::kCompleted:
          queue->m_PendingNodeCount--;

          UnblockWaiters(queue, node);

          CondBroadcast(&queue->m_WorkAvailable);
          return;

        default:
          Croak("invalid node state progress");
          break;
      }
    }
  }

  static NodeState* NextNode(BuildQueue* queue)
  {
    int avail_count = AvailableNodeCount(queue);

    if (0 == avail_count)
      return nullptr;

    uint32_t read_index = queue->m_QueueReadIndex;

    int32_t node_index = queue->m_Queue[read_index];

    // Update read index
    queue->m_QueueReadIndex = (read_index + 1) & (queue->m_QueueCapacity - 1);

    NodeState* state = queue->m_Config.m_NodeState + node_index;

    CHECK(NodeStateIsQueued(state));
    CHECK(!NodeStateIsActive(state));

    NodeStateFlagUnqueued(state);
    NodeStateFlagActive(state);

    return state;
  }

  static bool ShouldKeepBuilding(BuildQueue* queue, int thread_index)
  {
    // Stop running if we were signalled
    if (nullptr != SignalGetReason())
      return false;

    // Stop running if there are errors and we're stopping on the first error.
    if (queue->m_FailedNodeCount > 0)
    {
      if (0 == (queue->m_Config.m_Flags & BuildQueueConfig::kFlagContinueOnError))
      {
        return false;
      }
    }

    // If we're quitting, definitely stop building.
    if (queue->m_QuitSignalled)
      return false;

    // If we're a worker thread, keep running until we quit.
    if (0 != thread_index)
      return true;
    
    // We're the main thread. Just loop until there's no more nodes and then move on to the next pass.
    return queue->m_PendingNodeCount > 0;
  }

  static void BuildLoop(ThreadState* thread_state)
  {
    BuildQueue        *queue = thread_state->m_Queue;
    ConditionVariable *cv    = &queue->m_WorkAvailable;
    Mutex             *mutex = &queue->m_Lock;

    MutexLock(mutex);

    while (ShouldKeepBuilding(queue, thread_state->m_ThreadIndex))
    {
      if (NodeState* node = NextNode(queue))
      {
        AdvanceNode(queue, thread_state, node, mutex);
      }
      else
      {
        CondWait(cv, mutex);
      }
    }

    MutexUnlock(mutex);

    Log(kSpam, "build thread %d exiting\n", thread_state->m_ThreadIndex);
  }

  static ThreadRoutineReturnType TUNDRA_STDCALL BuildThreadRoutine(void* param)
  {
    ThreadState *thread_state = static_cast<ThreadState*>(param);

    LinearAllocSetOwner(&thread_state->m_ScratchAlloc, ThreadCurrent());
    
    BuildLoop(thread_state);

    return 0;
  }

  void BuildQueueInit(BuildQueue* queue, const BuildQueueConfig* config)
  {
    CHECK(config->m_MaxExpensiveCount > 0 && config->m_MaxExpensiveCount <= config->m_ThreadCount);

    MutexInit(&queue->m_Lock);
    CondInit(&queue->m_WorkAvailable);

    // Compute queue capacity. Allocate space for a power of two number of
    // indices that's at least one larger than the max number of nodes. Because
    // the queue is treated as a ring buffer, we want W=R to mean an empty
    // buffer.
    uint32_t capacity = NextPowerOfTwo(config->m_MaxNodes + 1);

    MemAllocHeap* heap = config->m_Heap;

    queue->m_Queue              = HeapAllocateArray<int32_t>(heap, capacity);
    queue->m_QueueReadIndex     = 0;
    queue->m_QueueWriteIndex    = 0;
    queue->m_QueueCapacity      = capacity;
    queue->m_Config             = *config;
    queue->m_PendingNodeCount   = 0;
    queue->m_FailedNodeCount    = 0;
    queue->m_QuitSignalled      = false;
    queue->m_ExpensiveRunning   = 0;
    queue->m_ExpensiveWaitCount = 0;
    queue->m_ExpensiveWaitList  = HeapAllocateArray<NodeState*>(heap, capacity);

    CHECK(queue->m_Queue);

    if (queue->m_Config.m_ThreadCount > kMaxBuildThreads)
    {
      Log(kWarning, "too many build threads (%d) - clamping to %d",
          queue->m_Config.m_ThreadCount, kMaxBuildThreads);

      queue->m_Config.m_ThreadCount = kMaxBuildThreads;
    }

    Log(kDebug, "build queue initialized; ring buffer capacity = %u", queue->m_QueueCapacity);

    // Block all signals on the main thread.
    SignalBlockThread(true);
    SignalHandlerSetCondition(&queue->m_WorkAvailable);

    // Create build threads.
    for (int i = 0, thread_count = config->m_ThreadCount; i < thread_count; ++i)
    {
      ThreadState* thread_state = &queue->m_ThreadState[i];

      ThreadStateInit(thread_state, queue, MB(64), MB(32), i);

      if (i > 0)
      {
        Log(kDebug, "starting build thread %d", i);
        queue->m_Threads[i] = ThreadStart(BuildThreadRoutine, thread_state);
      }
    }
  }

  void BuildQueueDestroy(BuildQueue* queue)
  {
    Log(kDebug, "destroying build queue");
    const BuildQueueConfig* config = &queue->m_Config;

    MutexLock(&queue->m_Lock);
    queue->m_QuitSignalled = true;
    MutexUnlock(&queue->m_Lock);

    CondBroadcast(&queue->m_WorkAvailable);

    for (int i = 0, thread_count = config->m_ThreadCount; i < thread_count; ++i)
    {
      if (i > 0)
      {
        Log(kDebug, "joining with build thread %d", i);
        ThreadJoin(queue->m_Threads[i]);
      }

      ThreadStateDestroy(&queue->m_ThreadState[i]);
    }

    // Deallocate storage.
    MemAllocHeap* heap = queue->m_Config.m_Heap;
    HeapFree(heap, queue->m_ExpensiveWaitList);
    HeapFree(heap, queue->m_Queue);

    CondDestroy(&queue->m_WorkAvailable);
    MutexDestroy(&queue->m_Lock);

    // Unblock all signals on the main thread.
    SignalHandlerSetCondition(nullptr);
    SignalBlockThread(false);
  }

  BuildResult::Enum BuildQueueBuildNodeRange(BuildQueue* queue, int start_index, int count, int pass_index)
  {
    // Make sure none of the build threads see in-progress state due to a spurious wakeup.
    MutexLock(&queue->m_Lock);

    CHECK(start_index + count <= queue->m_Config.m_MaxNodes);

    queue->m_CurrentPassIndex = pass_index;

    // Initialize build queue with index range to build
    int32_t   *build_queue = queue->m_Queue;
    NodeState *node_states = queue->m_Config.m_NodeState;

    for (int i = 0; i < count; ++i)
    {
      NodeState* state = node_states + start_index + i;

      NodeStateFlagQueued(state);

      // Verify node hasn't been touched already
      CHECK(state->m_Progress == BuildProgress::kInitial);

      build_queue[i] = start_index + i;
    }

    queue->m_PendingNodeCount = count;
    queue->m_FailedNodeCount  = 0;
    queue->m_QueueWriteIndex  = count;
    queue->m_QueueReadIndex   = 0;

    MutexUnlock(&queue->m_Lock);

    CondBroadcast(&queue->m_WorkAvailable);

    // This thread is thread 0.
    BuildLoop(&queue->m_ThreadState[0]);

    if (SignalGetReason())
      return BuildResult::kInterrupted;
    else if (queue->m_FailedNodeCount)
      return BuildResult::kBuildError;
    else
      return BuildResult::kOk;
  }
}

