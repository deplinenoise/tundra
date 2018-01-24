#include "Driver.hpp"
#include "BinaryWriter.hpp"
#include "Buffer.hpp"
#include "BuildQueue.hpp"
#include "Common.hpp"
#include "DagData.hpp"
#include "DagGenerator.hpp"
#include "FileInfo.hpp"
#include "MemAllocLinear.hpp"
#include "MemoryMappedFile.hpp"
#include "NodeState.hpp"
#include "ScanData.hpp"
#include "SortedArrayUtil.hpp"
#include "StateData.hpp"
#include "Stats.hpp"
#include "TargetSelect.hpp"
#include "HashTable.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ENABLED(TUNDRA_CASE_INSENSITIVE_FILESYSTEM)
#if defined(_MSC_VER) || defined(TUNDRA_WIN32_MINGW)
#define PathCompareN _strnicmp
#define PathCompare _stricmp
#else
#define PathCompareN strncasecmp
#define PathCompare strcasecmp
#endif
#else
#define PathCompareN strncmp
#define PathCompare strcmp
#endif

namespace t2
{

TundraStats g_Stats;

static const char s_BuildFile[]              = "tundra.lua";
static const char s_DagFileName[]            = ".tundra2.dag";
static const char s_StateFileName[]          = ".tundra2.state";
static const char s_ScanCacheFileName[]      = ".tundra2.scancache";
static const char s_DigestCacheFileName[]    = ".tundra2.digestcache";
// Temporary filenames where we write data first. These are then renamed to commit.
static const char s_StateFileNameTmp[]       = ".tundra2.state.tmp";
static const char s_ScanCacheFileNameTmp[]   = ".tundra2.scancache.tmp";
static const char s_DigestCacheFileNameTmp[] = ".tundra2.digestcache.tmp";

static bool DriverPrepareDag(Driver* self, const char* dag_fn);
static bool DriverCheckDagSignatures(Driver* self);

// Set default options.
void DriverOptionsInit(DriverOptions* self)
{
  self->m_ShowHelp        = false;
  self->m_DryRun          = false;
  self->m_ForceDagRegen   = false;
  self->m_ShowTargets     = false;
  self->m_DebugMessages   = false;
  self->m_Verbose         = false;
  self->m_SpammyVerbose   = false;
  self->m_DisplayStats    = false;
  self->m_GenDagOnly      = false;
  self->m_Quiet           = false;
  self->m_Clean           = false;
  self->m_Rebuild         = false;
  self->m_IdeGen          = false;
  self->m_DebugSigning    = false;
  self->m_ContinueOnError = false;
  self->m_ThreadCount     = GetCpuCount();
  self->m_WorkingDir      = nullptr;
  #if defined(TUNDRA_WIN32)
  self->m_RunUnprotected  = false;
#endif
}

// Helper routine to load frozen data into RAM via memory mapping
template <typename FrozenType>
static bool LoadFrozenData(const char* fn, MemoryMappedFile* result, const FrozenType** ptr)
{
  MemoryMappedFile mapping;

  MmapFileInit(&mapping);

  MmapFileMap(&mapping, fn);

  if (MmapFileValid(&mapping))
  {
    char *mmap_buffer = static_cast<char*>(mapping.m_Address);
    const FrozenType* data = reinterpret_cast<const FrozenType*>(mmap_buffer);

    Log(kDebug, "%s: successfully mapped at %p (%d bytes)", fn, data, (int) mapping.m_Size);

    // Check size
    if (mapping.m_Size < sizeof(FrozenType))
    {
      Log(kWarning, "%s: Bad mmap size %d - need at least %d bytes",
          fn, (int) mapping.m_Size, (int) sizeof(FrozenType));
      goto error;
    }

    // Check magic number
    if (data->m_MagicNumber != FrozenType::MagicNumber)
    {
      Log(kDebug, "%s: Bad magic number %08x - current is %08x",
          fn, data->m_MagicNumber, FrozenType::MagicNumber);
      goto error;
    }

    // Move ownership of memory mapping to member variable.
    *result = mapping;

    *ptr = data;

    return true;
  }

  Log(kDebug, "%s: mmap failed", fn);

error:
  MmapFileDestroy(&mapping);
  return false;
}

void DriverShowTargets(Driver* self)
{
  const DagData* dag = self->m_DagData;

  printf("%-20s %-20s %-20s\n", "Config", "Variant", "SubVariant");
  printf("----------------------------------------------------------------\n");

  for (const BuildTupleData& tuple : dag->m_BuildTuples)
  {
    const char* config_name = dag->m_ConfigNames[tuple.m_ConfigIndex];
    const char* variant_name = dag->m_VariantNames[tuple.m_VariantIndex];
    const char* subvariant_name = dag->m_SubVariantNames[tuple.m_SubVariantIndex];
    printf("%-20s %-20s %-20s\n", config_name, variant_name, subvariant_name);
  }

  printf("\nNamed nodes and aliases:\n");
  printf("----------------------------------------------------------------\n");

  for (const BuildTupleData& tuple : dag->m_BuildTuples)
  {
    int32_t count = tuple.m_NamedNodes.GetCount();
    const char** temp = (const char**)alloca(sizeof(const char*) * count);
    for (int i = 0; i < count; ++i)
    {
      temp[i] = tuple.m_NamedNodes[i].m_Name.Get();
    }
    std::sort(temp, temp + count, [](const char *a, const char *b) { return strcmp(a, b) < 0; });

    for (int i = 0; i < count; ++i)
    {
      printf(" - %s\n", temp[i]);
    }
    // Currently the named nodes are the same for all build tuples.
    // We just need one.
    break;
  }
}

bool DriverInitData(Driver* self)
{
  if (!DriverPrepareDag(self, s_DagFileName))
    return false;

  LoadFrozenData<StateData>(s_StateFileName, &self->m_StateFile, &self->m_StateData);

  LoadFrozenData<ScanData>(s_ScanCacheFileName, &self->m_ScanFile, &self->m_ScanData);

  ScanCacheSetCache(&self->m_ScanCache, self->m_ScanData);

  return true;
}

static bool DriverPrepareDag(Driver* self, const char* dag_fn)
{
  // Try to use an existing DAG
  if (!self->m_Options.m_ForceDagRegen && LoadFrozenData<DagData>(dag_fn, &self->m_DagFile, &self->m_DagData))
  {
    if (DriverCheckDagSignatures(self))
    {
      Log(kDebug, "DAG signatures match - using existing data w/o Lua invocation");
      return true;
    }

    MmapFileUnmap(&self->m_DagFile);
  }

  // We need to generate the DAG data
  if (!GenerateDag(s_BuildFile, dag_fn))
    return false;

  // The DAG had better map in now, or we can give up.
  if (!LoadFrozenData<DagData>(dag_fn, &self->m_DagFile, &self->m_DagData))
  {
    Log(kError, "panic: couldn't load in freshly generated DAG");
    return false;
  }

  // In checked builds, make sure signatures are valid.
  CHECK(DriverCheckDagSignatures(self));

  return true;
}

static bool DriverCheckDagSignatures(Driver* self)
{
  const DagData* dag_data = self->m_DagData;

#if ENABLED(CHECKED_BUILD)
    // Paranoia - make sure the data is sorted.
    for (int i = 1, count = dag_data->m_NodeCount; i < count; ++i)
    {
      if (dag_data->m_NodeGuids[i] < dag_data->m_NodeGuids[i - 1])
        Croak("DAG data is not sorted by guid");
    }
#endif

  Log(kDebug, "checking file signatures for DAG data");

  if (dag_data->m_Passes.GetCount() > Driver::kMaxPasses)
  {
    Log(kError, "too many passes, max is %d", Driver::kMaxPasses);
    return false;
  }

  // Check timestamps of lua files used to produce the DAG
  for (const DagFileSignature& sig : dag_data->m_FileSignatures)
  {
    const char* path = sig.m_Path;

    uint64_t timestamp = sig.m_Timestamp;
    FileInfo info      = GetFileInfo(path);

    if (info.m_Timestamp != timestamp)
    {
      Log(kInfo, "DAG out of date: timestamp change for %s", path);
      return false;
    }
  }

  // Helper for directory iteration + memory allocation of strings.  We need to
  // buffer the filenames as we need them in sorted order to ensure the results
  // are consistent between runs.
  struct IterContext
  {
    MemAllocLinear       *m_Allocator;
    MemAllocHeap         *m_Heap;
    Buffer<const char *>  m_Dirs;
    Buffer<const char *>  m_Files;

    void Init(MemAllocHeap* heap, MemAllocLinear* linear)
    {
      m_Allocator = linear;
      m_Heap      = heap;
      BufferInit(&m_Dirs);
      BufferInit(&m_Files);
    }

    void Destroy()
    {
      BufferDestroy(&m_Files, m_Heap);
      BufferDestroy(&m_Dirs, m_Heap);
    }

    static void Callback(void* user_data, const FileInfo& info, const char* path)
    {
      IterContext* self = (IterContext*) user_data;
      char* data = StrDup(self->m_Allocator, path);
      Buffer<const char*>* target = info.IsDirectory() ? &self->m_Dirs : &self->m_Files;
      BufferAppendOne(target, self->m_Heap, data);
    }

    static int SortStringPtrs(const void* l, const void* r)
    {
      return strcmp(*(const char**)l, *(const char**)r);
    }
  };

  // Check directory listing fingerprints
  // Note that the digest computation in here must match the one in LuaListDirectory
  // The digests computed there are stored in the signature block by Lua code.
  for (const DagGlobSignature& sig : dag_data->m_GlobSignatures)
  {
    const char* path = sig.m_Path;

    // Set up to rewind allocator for each loop iteration
    MemAllocLinearScope mem_scope(&self->m_Allocator);

    // Set up context
    IterContext ctx;
    ctx.Init(&self->m_Heap, &self->m_Allocator);

    // Get directory data
    ListDirectory(path, &ctx, IterContext::Callback);

    // Sort data
    qsort(ctx.m_Dirs.m_Storage, ctx.m_Dirs.m_Size, sizeof(const char*), IterContext::SortStringPtrs);
    qsort(ctx.m_Files.m_Storage, ctx.m_Files.m_Size, sizeof(const char*), IterContext::SortStringPtrs);

    // Compute digest
    HashState h;
    HashInit(&h);
    for (const char* p : ctx.m_Dirs)
    {
      HashAddString(&h, p);
      HashAddSeparator(&h);
    }

    for (const char* p : ctx.m_Files)
    {
      HashAddString(&h, p);
      HashAddSeparator(&h);
    }

    HashDigest digest;
    HashFinalize(&h, &digest);

    ctx.Destroy();

    // Compare digest with the one stored in the signature block
    if (0 != memcmp(&digest, &sig.m_Digest, sizeof digest))
    {
      char stored[kDigestStringSize], actual[kDigestStringSize];
      DigestToString(stored, sig.m_Digest);
      DigestToString(actual, digest);
      Log(kInfo, "DAG out of date: file glob change for %s (%s => %s)", sig.m_Path.Get(), stored, actual);
      return false;
    }
  }

  return true;
}

static const BuildTupleData* FindBuildTuple(const DagData* dag, const TargetSpec spec)
{
  for (const BuildTupleData& tuple : dag->m_BuildTuples)
  {
    if (tuple.m_ConfigIndex == spec.m_ConfigIndex &&
        tuple.m_VariantIndex == spec.m_VariantIndex &&
        tuple.m_SubVariantIndex == spec.m_SubVariantIndex)
    {
      return &tuple;
    }
  }

  return nullptr;
}

// Walk reachable nodes from the entry points in the DAG
static void FindReachable(uint32_t* node_bits, const DagData* dag, int index)
{
  int word_index = index / 32;
  int bit_index = index & 31;
  if (node_bits[word_index] & (1 << bit_index))
    return;

  node_bits[word_index] |= (1 << bit_index);

  const NodeData* node = dag->m_NodeData + index;

  for (int dep : node->m_Dependencies)
  {
    FindReachable(node_bits, dag, dep);
  }
}

static void FindReachableNodes(uint32_t* node_bits, const DagData* dag, const BuildTupleData* tuple)
{
  for (const NamedNodeData& named_node : tuple->m_NamedNodes)
  {
    FindReachable(node_bits, dag, named_node.m_NodeIndex);
  }
}

// Match their source files and output files against the names specified.
static void FindNodesByName(
    const DagData*          dag,
    Buffer<int32_t>*        out_nodes,
    MemAllocHeap*           heap,
    const char**            names,
    size_t                  name_count,
    const BuildTupleData*   tuple)
{
  size_t    node_bits_size = (dag->m_NodeCount + 31) / 32 *sizeof(uint32_t);
  uint32_t *node_bits      = (uint32_t*) alloca(node_bits_size);
  bool      bits_valid     = false;

  memset(node_bits, 0, node_bits_size);

  for (size_t name_i = 0; name_i < name_count; ++name_i)
  {
    const char *name = names[name_i];

    bool found = false;

    // Try all named nodes first
    for (const NamedNodeData& named_node : tuple->m_NamedNodes)
    {
      if (0 == strcmp(named_node.m_Name, name))
      {
        BufferAppendOne(out_nodes, heap, named_node.m_NodeIndex);
        Log(kDebug, "mapped %s to node %d", name, named_node.m_NodeIndex);
        found = true;
        break;
      }
    }

    // Try to match against all input/output filenames
    if (!found)
    {
      char cwd[kMaxPathLength + 1];
      GetCwd(cwd, sizeof cwd);
      size_t cwd_len = strlen(cwd);
      cwd[cwd_len] = TD_PATHSEP;
      cwd[cwd_len+1] = '\0';

      const char *filename = name;
      PathBuffer path;
      PathInit(&path, filename);
      char path_fmt[kMaxPathLength];
      PathFormat(path_fmt, &path);

      if (0 == PathCompareN(path_fmt, cwd, cwd_len+1))
      {
        filename = path_fmt + cwd_len + 1;
        Log(kDebug, "Mapped %s to %s for DAG searching", path_fmt, filename);
      }

      const uint32_t filename_hash = Djb2HashPath(filename);

      if (!bits_valid)
      {
        FindReachableNodes(node_bits, dag, tuple);
        bits_valid = true;
      }

      // Brute force all reachable nodes from tuple to match input or output file names
      size_t base_index = 0;
      for (size_t i = 0, count = node_bits_size / 4; !found && i < count; ++i, base_index += 32)
      {
        uint32_t bits = node_bits[i];

        while (bits)
        {
          int bit = CountTrailingZeroes(bits);

          size_t node_index = base_index + bit;
          const NodeData* node = dag->m_NodeData + node_index;

          for (const FrozenFileAndHash& input : node->m_InputFiles)
          {
            if (filename_hash == input.m_FilenameHash && 0 == PathCompare(input.m_Filename, filename))
            {
              BufferAppendOne(out_nodes, heap, node_index);
              Log(kDebug, "mapped %s to node %d (based on input file)", name, node_index);
              found = true;
              break;
            }
          }

          if (found)
            break;

          for (const FrozenFileAndHash& output : node->m_OutputFiles)
          {
            if (filename_hash == output.m_FilenameHash && 0 == PathCompare(output.m_Filename, filename))
            {
              BufferAppendOne(out_nodes, heap, node_index);
              Log(kDebug, "mapped %s to node %d (based on output file)", name, node_index);
              found = true;
              break;
            }
          }

          if (found)
            break;

          bits &= ~(1 << bit);
        }
      }
    }

    if (!found)
    {
      Log(kWarning, "unable to map %s to any named node or input/output file", name);
    }
  }
}

static void DriverSelectNodes(const DagData* dag, const char** targets, int target_count, Buffer<int32_t>* out_nodes, MemAllocHeap* heap)
{
  Buffer<TargetSpec> target_specs;
  Buffer<const char*> named_targets;

  BufferInit(&target_specs);
  BufferInit(&named_targets);

  TargetSelectInput tsel;
  tsel.m_ConfigCount            = dag->m_ConfigCount;
  tsel.m_VariantCount           = dag->m_VariantCount;
  tsel.m_SubVariantCount        = dag->m_SubVariantCount;
  tsel.m_ConfigNameHashes       = dag->m_ConfigNameHashes;
  tsel.m_VariantNameHashes      = dag->m_VariantNameHashes;
  tsel.m_SubVariantNameHashes   = dag->m_SubVariantNameHashes;
  tsel.m_InputNameCount         = target_count;
  tsel.m_InputNames             = targets;
  tsel.m_DefaultConfigIndex     = dag->m_DefaultConfigIndex;
  tsel.m_DefaultVariantIndex    = dag->m_DefaultVariantIndex;
  tsel.m_DefaultSubVariantIndex = dag->m_DefaultSubVariantIndex;

  SelectTargets(tsel, heap, &target_specs, &named_targets);

  for (const TargetSpec& spec : target_specs)
  {
    const BuildTupleData* tuple = FindBuildTuple(dag, spec);
    if (!tuple)
      Croak("couldn't find build tuple in DAG");

    if (named_targets.m_Size > 0)
    {
      FindNodesByName(
          dag,
          out_nodes, heap,
          named_targets.m_Storage, named_targets.m_Size,
          tuple);
    }
    else
    {
      BufferAppend(out_nodes, heap, tuple->m_DefaultNodes.GetArray(), tuple->m_DefaultNodes.GetCount());
    }

    BufferAppend(out_nodes, heap, tuple->m_AlwaysNodes.GetArray(), tuple->m_AlwaysNodes.GetCount());
  }

  std::sort(out_nodes->begin(), out_nodes->end());
  int32_t* new_end = std::unique(out_nodes->begin(), out_nodes->end());
  out_nodes->m_Size = new_end - out_nodes->begin();
  Log(kDebug, "Node selection finished with %d nodes to build", (int) out_nodes->m_Size);

  BufferDestroy(&named_targets, heap);
  BufferDestroy(&target_specs, heap);
}

bool DriverPrepareNodes(Driver* self, const char** targets, int target_count)
{
  const DagData    *dag       = self->m_DagData;
  const NodeData   *src_nodes = dag->m_NodeData;
  const HashDigest *src_guids = dag->m_NodeGuids;
  MemAllocHeap     *heap      = &self->m_Heap;

  Buffer<int32_t> node_stack;
  BufferInitWithCapacity(&node_stack, heap, 1024);

  DriverSelectNodes(dag, targets, target_count, &node_stack, heap);

  const size_t node_word_count = (dag->m_NodeCount + 31) / 32;
  uint32_t* node_visited_bits = HeapAllocateArrayZeroed<uint32_t>(heap, node_word_count);

  int node_count = 0;

  Buffer<int32_t> node_indices;
  BufferInitWithCapacity(&node_indices, heap, 1024);

  while (node_stack.m_Size > 0)
  {
    int       dag_index = BufferPopOne(&node_stack);
    const int dag_word  = dag_index / 32;
    const int dag_bit   = 1 << (dag_index &31);

    if (0 == (node_visited_bits[dag_word] & dag_bit))
    {
      const NodeData* node = src_nodes + dag_index;
      CHECK(uint32_t(node->m_PassIndex) < uint32_t(dag->m_Passes.GetCount()));

      BufferAppendOne(&node_indices, &self->m_Heap, dag_index);

      node_visited_bits[dag_word] |= dag_bit;

      // Update counts
      ++node_count;
      self->m_PassNodeCount[node->m_PassIndex]++;

      // Stash node dependencies on the work queue to keep iterating
      BufferAppend(&node_stack, &self->m_Heap, node->m_Dependencies.GetArray(), node->m_Dependencies.GetCount());
    }
  }

  HeapFree(heap, node_visited_bits);
  node_visited_bits = nullptr;

  // Allocate space for nodes
  NodeState* out_nodes = BufferAllocZero(&self->m_Nodes, &self->m_Heap, node_count);

  // Initialize node state
  for (int i = 0; i < node_count; ++i)
  {
    const NodeData* src_node = src_nodes + node_indices[i];
    out_nodes[i].m_MmapData  = src_node;
    out_nodes[i].m_PassIndex = (uint16_t) src_node->m_PassIndex;
  }

  // Find frozen node state from previous build, if present.
  if (const StateData* state_data = self->m_StateData)
  {
    const NodeStateData *frozen_states    = state_data->m_NodeStates;
    const HashDigest    *state_guids      = state_data->m_NodeGuids;
    const int            state_guid_count = state_data->m_NodeCount;

    for (int i = 0; i < node_count; ++i)
    {
      const HashDigest* src_guid = src_guids + node_indices[i];

      if (const HashDigest* old_guid = BinarySearch(state_guids, state_guid_count, *src_guid))
      {
        int state_index = int(old_guid - state_guids);
        out_nodes[i].m_MmapState = frozen_states + state_index;
      }
    }
  }


  // Sort the node state array based on which pass the nodes are in.
  auto compare_node_passes = [](const NodeState& l, const NodeState& r) -> bool
  {
    return l.m_PassIndex < r.m_PassIndex;
  };

  std::sort(out_nodes, out_nodes + node_count, compare_node_passes);

  // Now that our local order is established (sorted for pass purposes),
  // initialize a remapping table from global (dag) index to local (state)
  // index. This is so we can map any DAG node reference onto any local state.
  int32_t* node_remap = BufferAllocFill(&self->m_NodeRemap, heap, dag->m_NodeCount, -1);

  CHECK(node_remap == self->m_NodeRemap.m_Storage);

  for (int local_index = 0; local_index < node_count; ++local_index)
  {
    const NodeData* global_node = out_nodes[local_index].m_MmapData;
    const int global_index = int(global_node - src_nodes);
    CHECK(node_remap[global_index] == -1);
    node_remap[global_index] = local_index;
  }

  Log(kDebug, "Node remap: %d src nodes, %d active nodes, using %d bytes of node state buffer space",
      dag->m_NodeCount, node_count, sizeof(NodeState) * node_count);

  BufferDestroy(&node_stack, &self->m_Heap);
  BufferDestroy(&node_indices, &self->m_Heap);

  return true;
}

bool DriverInit(Driver* self, const DriverOptions* options)
{
  HeapInit(&self->m_Heap);
  LinearAllocInit(&self->m_Allocator, &self->m_Heap, MB(64), "Driver Linear Allocator");

  LinearAllocSetOwner(&self->m_Allocator, ThreadCurrent());

  MmapFileInit(&self->m_DagFile);
  MmapFileInit(&self->m_StateFile);
  MmapFileInit(&self->m_ScanFile);

  self->m_DagData = nullptr;
  self->m_StateData = nullptr;
  self->m_ScanData = nullptr;

  BufferInit(&self->m_NodeRemap);
  BufferInit(&self->m_Nodes);

  self->m_Options = *options;

  // This linear allocator is only accessed when the state cache is locked.
  LinearAllocInit(&self->m_ScanCacheAllocator, &self->m_Heap, MB(64), "scan cache");
  ScanCacheInit(&self->m_ScanCache, &self->m_Heap, &self->m_ScanCacheAllocator);

  // This linear allocator is only accessed when the state cache is locked.
  LinearAllocInit(&self->m_StatCacheAllocator, &self->m_Heap, MB(64), "stat cache");
  StatCacheInit(&self->m_StatCache, &self->m_StatCacheAllocator, &self->m_Heap);

  memset(&self->m_PassNodeCount, 0, sizeof self->m_PassNodeCount);

  DigestCacheInit(&self->m_DigestCache, MB(128), s_DigestCacheFileName);

  return true;
}

void DriverDestroy(Driver* self)
{
  DigestCacheDestroy(&self->m_DigestCache);

  StatCacheDestroy(&self->m_StatCache);

  ScanCacheDestroy(&self->m_ScanCache);

  BufferDestroy(&self->m_Nodes, &self->m_Heap);
  BufferDestroy(&self->m_NodeRemap, &self->m_Heap);

  MmapFileDestroy(&self->m_ScanFile);
  MmapFileDestroy(&self->m_StateFile);
  MmapFileDestroy(&self->m_DagFile);

  LinearAllocDestroy(&self->m_ScanCacheAllocator);
  LinearAllocDestroy(&self->m_StatCacheAllocator);
  LinearAllocDestroy(&self->m_Allocator);
  HeapDestroy(&self->m_Heap);
}

bool DriverPrepareDag(Driver* self, const char* dag_fn);
bool DriverAllocNodes(Driver* self);

BuildResult::Enum DriverBuild(Driver* self)
{
  const DagData* dag = self->m_DagData;
  const int pass_count = dag->m_Passes.GetCount();

#if ENABLED(CHECKED_BUILD)
  // Do some paranoia checking of the node state to make sure pass indices are
  // set up correctly.
  {
    int i = 0;
    for (int pass = 0; pass < pass_count; ++pass)
    {
      for (int n = 0, node_count = self->m_PassNodeCount[pass]; n < node_count; ++n)
      {
        CHECK(self->m_Nodes[i].m_PassIndex == pass);
        ++i;
      }
    }
    CHECK(size_t(i) == self->m_Nodes.m_Size);
  }
#endif

  // Initialize build queue
  Mutex debug_signing_mutex;

  int max_expensive_count = self->m_DagData->m_MaxExpensiveCount;

  if (max_expensive_count < 0)
    max_expensive_count = self->m_Options.m_ThreadCount;
  else
    max_expensive_count = std::max(std::min(max_expensive_count, self->m_Options.m_ThreadCount), 1);

  Log(kDebug, "Max # expensive jobs: %d", max_expensive_count);

  BuildQueueConfig queue_config;
  queue_config.m_Flags                   = 0;
  queue_config.m_Heap                    = &self->m_Heap;
  queue_config.m_ThreadCount             = (int) self->m_Options.m_ThreadCount;
  queue_config.m_NodeData                = self->m_DagData->m_NodeData;
  queue_config.m_NodeState               = self->m_Nodes.m_Storage;
  queue_config.m_MaxNodes                = (int) self->m_Nodes.m_Size;
  queue_config.m_NodeRemappingTable      = self->m_NodeRemap.m_Storage;
  queue_config.m_ScanCache               = &self->m_ScanCache;
  queue_config.m_StatCache               = &self->m_StatCache;
  queue_config.m_DigestCache             = &self->m_DigestCache;
  queue_config.m_ShaDigestExtensionCount = dag->m_ShaExtensionHashes.GetCount();
  queue_config.m_ShaDigestExtensions     = dag->m_ShaExtensionHashes.GetArray();
  queue_config.m_MaxExpensiveCount       = max_expensive_count;

  if (self->m_Options.m_Verbose)
  {
    queue_config.m_Flags |= BuildQueueConfig::kFlagEchoAnnotations | BuildQueueConfig::kFlagEchoCommandLines;
  }
  if (!self->m_Options.m_Quiet)
  {
    queue_config.m_Flags |= BuildQueueConfig::kFlagEchoAnnotations;
  }
  if (self->m_Options.m_ContinueOnError)
  {
    queue_config.m_Flags |= BuildQueueConfig::kFlagContinueOnError;
  }

  if (self->m_Options.m_DebugSigning)
  {
    MutexInit(&debug_signing_mutex);
    queue_config.m_FileSigningLogMutex = &debug_signing_mutex;
    queue_config.m_FileSigningLog      = fopen("signing-debug.txt", "w");
  }
  else
  {
    queue_config.m_FileSigningLogMutex = nullptr;
    queue_config.m_FileSigningLog      = nullptr;
  }

#if ENABLED(CHECKED_BUILD)
  // Paranoia - double check node remapping table
  for (size_t i = 0, count = self->m_Nodes.m_Size; i < count; ++i)
  {
    const NodeState* state = self->m_Nodes.m_Storage + i;
    const NodeData* src = state->m_MmapData;
    const int src_index = int(src - self->m_DagData->m_NodeData);
    int remapped_index = self->m_NodeRemap[src_index];
    CHECK(size_t(remapped_index) == i);
  }
#endif


  BuildQueue build_queue;
  BuildQueueInit(&build_queue, &queue_config);

  int global_node_index = 0;

  BuildResult::Enum build_result = BuildResult::kOk;

  for (int pass = 0; BuildResult::kOk == build_result && pass < pass_count; ++pass)
  {
    const char *pass_name  = dag->m_Passes[pass].m_PassName;
    const int   pass_nodes = self->m_PassNodeCount[pass];

    Log(kInfo, "begin pass %s (nodes: %d - %d (%d))",
        pass_name, global_node_index, global_node_index + pass_nodes - 1, pass_nodes);

    build_result = BuildQueueBuildNodeRange(&build_queue, global_node_index, pass_nodes, pass);

    global_node_index += pass_nodes;

    Log(kInfo, "end pass %s", pass_name);
  }

  if (self->m_Options.m_DebugSigning)
  {
    fclose((FILE*)queue_config.m_FileSigningLog);
    MutexDestroy(&debug_signing_mutex);
  }

  // Shut down build queue
  BuildQueueDestroy(&build_queue);

  return build_result;
}

// Save scan cache
bool DriverSaveScanCache(Driver* self)
{
  ScanCache* scan_cache = &self->m_ScanCache;

  if (!ScanCacheDirty(scan_cache))
    return true;

  // This will be invalidated.
  self->m_ScanData = nullptr;

  bool success = ScanCacheSave(scan_cache, s_ScanCacheFileNameTmp, &self->m_Heap);

  // Unmap the file so we can overwrite it (on Windows.)
  MmapFileDestroy(&self->m_ScanFile);

  if (success)
  {
    success = RenameFile(s_ScanCacheFileNameTmp, s_ScanCacheFileName);
  }
  else
  {
    remove(s_ScanCacheFileNameTmp);
  }

  return success;
}

// Save digest cache
bool DriverSaveDigestCache(Driver* self)
{
  // This will be invalidated.
  return DigestCacheSave(&self->m_DigestCache, &self->m_Heap, s_DigestCacheFileNameTmp);
}


bool DriverSaveBuildState(Driver* self)
{
  TimingScope timing_scope(nullptr, &g_Stats.m_StateSaveTimeCycles);

  MemAllocLinearScope alloc_scope(&self->m_Allocator);

  BinaryWriter writer;
  BinaryWriterInit(&writer, &self->m_Heap);

  BinarySegment *main_seg   = BinaryWriterAddSegment(&writer);
  BinarySegment *guid_seg   = BinaryWriterAddSegment(&writer);
  BinarySegment *state_seg  = BinaryWriterAddSegment(&writer);
  BinarySegment *array_seg  = BinaryWriterAddSegment(&writer);
  BinarySegment *string_seg = BinaryWriterAddSegment(&writer);

  BinaryLocator guid_ptr  = BinarySegmentPosition(guid_seg);
  BinaryLocator state_ptr = BinarySegmentPosition(state_seg);

  uint32_t             src_count       = self->m_DagData->m_NodeCount;
  const HashDigest    *src_guids       = self->m_DagData->m_NodeGuids;
  const NodeData      *src_data        = self->m_DagData->m_NodeData;
  NodeState           *new_state       = self->m_Nodes.m_Storage;
  const size_t         new_state_count = self->m_Nodes.m_Size;

  std::sort(new_state, new_state + new_state_count, [=](const NodeState& l, const NodeState& r) {
    // We know guids are sorted, so all we need to do is compare pointers into that table.
    return l.m_MmapData < r.m_MmapData;
  });

  const HashDigest    *old_guids       = nullptr;
  const NodeStateData *old_state       = nullptr;
  uint32_t             old_count       = 0;

  if (const StateData* state_data = self->m_StateData)
  {
    old_guids      = state_data->m_NodeGuids;
    old_state      = state_data->m_NodeStates;
    old_count      = state_data->m_NodeCount;
  }

  int entry_count = 0;

  auto save_node_state = [=](int build_result, const HashDigest* input_signature, const NodeData* src_node, const HashDigest* guid) -> void
  {
    BinarySegmentWrite(guid_seg, (const char*) guid, sizeof(HashDigest));

    BinarySegmentWriteInt32(state_seg, build_result);
    BinarySegmentWrite(state_seg, (const char*) input_signature, sizeof(HashDigest));

    int32_t file_count = src_node->m_OutputFiles.GetCount();
    BinarySegmentWriteInt32(state_seg, file_count);
    BinarySegmentWritePointer(state_seg, BinarySegmentPosition(array_seg));
    for (int32_t i = 0; i < file_count; ++i)
    {
      BinarySegmentWritePointer(array_seg, BinarySegmentPosition(string_seg));
      BinarySegmentWriteStringData(string_seg, src_node->m_OutputFiles[i].m_Filename);
    }

    file_count = src_node->m_AuxOutputFiles.GetCount();
    BinarySegmentWriteInt32(state_seg, file_count);
    BinarySegmentWritePointer(state_seg, BinarySegmentPosition(array_seg));
    for (int32_t i = 0; i < file_count; ++i)
    {
      BinarySegmentWritePointer(array_seg, BinarySegmentPosition(string_seg));
      BinarySegmentWriteStringData(string_seg, src_node->m_AuxOutputFiles[i].m_Filename);
    }
  };

  auto save_node_state_old = [=](int build_result, const HashDigest* input_signature, const NodeStateData* src_node, const HashDigest* guid) -> void
  {
    BinarySegmentWrite(guid_seg, (const char*) guid, sizeof(HashDigest));

    BinarySegmentWriteInt32(state_seg, build_result);
    BinarySegmentWrite(state_seg, (const char*) input_signature, sizeof(HashDigest));

    int32_t file_count = src_node->m_OutputFiles.GetCount();
    BinarySegmentWriteInt32(state_seg, file_count);
    BinarySegmentWritePointer(state_seg, BinarySegmentPosition(array_seg));
    for (int32_t i = 0; i < file_count; ++i)
    {
      BinarySegmentWritePointer(array_seg, BinarySegmentPosition(string_seg));
      BinarySegmentWriteStringData(string_seg, src_node->m_OutputFiles[i]);
    }

    file_count = src_node->m_AuxOutputFiles.GetCount();
    BinarySegmentWriteInt32(state_seg, file_count);
    BinarySegmentWritePointer(state_seg, BinarySegmentPosition(array_seg));
    for (int32_t i = 0; i < file_count; ++i)
    {
      BinarySegmentWritePointer(array_seg, BinarySegmentPosition(string_seg));
      BinarySegmentWriteStringData(string_seg, src_node->m_AuxOutputFiles[i]);
    }
  };

  auto save_new = [=, &entry_count](size_t index) {
    const NodeState  *elem      = new_state + index;
    const NodeData   *src_elem  = elem->m_MmapData;
    const int         src_index = int(src_elem - src_data);
    const HashDigest *guid      = src_guids + src_index;

    // If this node never computed an input signature (due to an error, or build cancellation), copy the old build progress over to retain the history.
    // Only do this if the output files and aux output files agree with the previously stored build state.
    if (elem->m_Progress < BuildProgress::kUnblocked)
    {
      if (const HashDigest* old_guid = BinarySearch(old_guids, old_count, *guid))
      {
        size_t old_index = old_guid - old_guids;
        const NodeStateData* old_state_data = old_state + old_index;
        save_node_state_old(old_state_data->m_BuildResult, &old_state_data->m_InputSignature, old_state_data, guid);
        ++entry_count;
        ++g_Stats.m_StateSaveNew;
      }
    }
    else
    {
      save_node_state(elem->m_BuildResult, &elem->m_InputSignature, src_elem, guid);
      ++entry_count;
      ++g_Stats.m_StateSaveNew;
    }
  };

  auto save_old = [=, &entry_count](size_t index) {
    const HashDigest    *guid = old_guids + index;

    // Make sure this node is still relevant before saving.
    if (const HashDigest* new_version = BinarySearch(src_guids, src_count, *guid))
    {
      size_t src_index = (new_version - src_guids);
      const NodeData* src_elem = src_data + src_index;
      const NodeStateData *data = old_state + index;

      save_node_state(data->m_BuildResult, &data->m_InputSignature, src_elem, guid);
      ++entry_count;
      ++g_Stats.m_StateSaveOld;
    }
    else
    {
      // Drop this node.
      ++g_Stats.m_StateSaveDropped;
    }
  };

  auto key_new = [=](size_t index) -> const HashDigest* {
    int dag_index = int(new_state[index].m_MmapData - src_data);
    return src_guids + dag_index;
  };

  auto key_old = [=](size_t index) {
    return old_guids + index;
  };

  TraverseSortedArrays(
      new_state_count, save_new, key_new,
      old_count, save_old, key_old);

  // Complete main data structure.
  BinarySegmentWriteUint32(main_seg, StateData::MagicNumber);
  BinarySegmentWriteInt32(main_seg, entry_count);
  BinarySegmentWritePointer(main_seg, guid_ptr);
  BinarySegmentWritePointer(main_seg, state_ptr);

  // Unmap old state data.
  MmapFileUnmap(&self->m_StateFile);
  self->m_StateData = nullptr;

  bool success = BinaryWriterFlush(&writer, s_StateFileNameTmp);

  if (success)
  {
    // Commit atomically with a file rename.
    success = RenameFile(s_StateFileNameTmp, s_StateFileName);
  }
  else
  {
    remove(s_StateFileNameTmp);
  }

  BinaryWriterDestroy(&writer);

  return success;
}

void DriverRemoveStaleOutputs(Driver* self)
{
  TimingScope timing_scope(nullptr, &g_Stats.m_StaleCheckTimeCycles);

  const DagData* dag = self->m_DagData;
  const StateData* state = self->m_StateData;
  MemAllocLinear* scratch = &self->m_Allocator;

  MemAllocLinearScope scratch_scope(scratch);

  if (!state)
  {
    Log(kDebug, "unable to clean up stale output files - no previous build state");
    return;
  }

  HashSet<kFlagPathStrings> file_table;
  HashSetInit(&file_table, &self->m_Heap);

  // Insert all current regular and aux output files into the hash table.
  auto add_file = [&file_table, scratch](const FrozenFileAndHash& p) -> void
  {
    const uint32_t hash = p.m_FilenameHash;

    if (!HashSetLookup(&file_table, hash, p.m_Filename))
    {
      HashSetInsert(&file_table, hash, p.m_Filename);
    }
  };

  for (int i = 0, node_count = dag->m_NodeCount; i < node_count; ++i)
  {
    const NodeData* node = dag->m_NodeData + i;

    for (const FrozenFileAndHash& p : node->m_OutputFiles)
    {
      add_file(p);
    }

    for (const FrozenFileAndHash& p : node->m_AuxOutputFiles)
    {
      add_file(p);
    }
  }

  HashSet<kFlagPathStrings> nuke_table;
  HashSetInit(&nuke_table, &self->m_Heap);

  // Check all output files in the state if they're still around.
  // Otherwise schedule them (and all their parent dirs) for nuking.
  // We will rely on the fact that we can't rmdir() non-empty directories.
  auto check_file = [&file_table, &nuke_table, scratch](const char* path)
  {
    uint32_t path_hash = Djb2HashPath(path);

    if (!HashSetLookup(&file_table, path_hash, path))
    {
      if (!HashSetLookup(&nuke_table, path_hash, path))
      {
        HashSetInsert(&nuke_table, path_hash, path);
      }

      PathBuffer buffer;
      PathInit(&buffer, path);

      while (PathStripLast(&buffer))
      {
        if (buffer.m_SegCount == 0)
          break;

        char dir[kMaxPathLength];
        PathFormat(dir, &buffer);
        uint32_t dir_hash = Djb2HashPath(dir);

        if (!HashSetLookup(&nuke_table, dir_hash, dir))
        {
          HashSetInsert(&nuke_table, dir_hash, StrDup(scratch, dir));
        }
      }
    }
  };

  for (int i = 0, state_count = state->m_NodeCount; i < state_count; ++i)
  {
    const NodeStateData* node = state->m_NodeStates + i;

    for (const char* path : node->m_OutputFiles)
    {
      check_file(path);
    }

    for (const char* path : node->m_AuxOutputFiles)
    {
      check_file(path);
    }
  }

  // Create list of files and dirs, sort descending by path length. This sorts
  // files and subdirectories before their parent directories.
  const char** paths = LinearAllocateArray<const char*>(scratch, nuke_table.m_RecordCount);
  HashSetWalk(&nuke_table, [paths](uint32_t index, uint32_t hash, const char* str) {
    paths[index] = str;
  });

  std::sort(paths, paths + nuke_table.m_RecordCount, [](const char* l, const char* r) {
    return strlen(r) < strlen(l);
  });

  for (uint32_t i = 0, nuke_count = nuke_table.m_RecordCount; i < nuke_count; ++i)
  {
    Log(kDebug, "cleaning up %s", paths[i]);
    RemoveFileOrDir(paths[i]);
  }

  HashSetDestroy(&nuke_table);
  HashSetDestroy(&file_table);
}

void DriverCleanOutputs(Driver* self)
{
  int count = 0;
  for (NodeState& state : self->m_Nodes)
  {
    for (const FrozenFileAndHash& fh : state.m_MmapData->m_OutputFiles)
    {
      if (0 == RemoveFileOrDir(fh.m_Filename))
        ++count;
    }
  }
  Log(kInfo, "Removed %d output files\n", count);
}

}
