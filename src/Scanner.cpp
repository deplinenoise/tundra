#include "Scanner.hpp"
#include "Common.hpp"
#include "IncludeScanner.hpp"
#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"
#include "DagData.hpp"
#include "PathUtil.hpp"
#include "FileInfo.hpp"
#include "Buffer.hpp"
#include "ScanCache.hpp"
#include "StatCache.hpp"
#include "HashTable.hpp"

#include <stdio.h>

namespace t2
{

struct IncludeSet
{
  MemAllocLinear *m_LinearAlloc;
  HashSet<kFlagPathStrings> m_HashTable;
};

static void IncludeSetInit(IncludeSet *self, MemAllocHeap *heap, MemAllocLinear* linear_alloc)
{
  self->m_LinearAlloc = linear_alloc;
  HashSetInit(&self->m_HashTable, heap);
}

static void IncludeSetDestroy(IncludeSet* self)
{
  HashSetDestroy(&self->m_HashTable);
}

static bool IncludeSetAddNoDuplicateString(IncludeSet* self, const char* string, uint32_t hash)
{
  if (HashSetLookup(&self->m_HashTable, hash, string))
  {
    return false;
  }

  // Allocate a new cell
  HashSetInsert(&self->m_HashTable, hash, string);

  return true;
}

static bool IncludeSetAddDuplicateString(IncludeSet* self, const char* string, uint32_t hash)
{
  if (HashSetLookup(&self->m_HashTable, hash, string))
  {
    return false;
  }

  // Allocate a new cell
  HashSetInsert(&self->m_HashTable, hash, StrDup(self->m_LinearAlloc, string));

  return true;
}

static bool FindFile(
    StatCache* stat_cache, 
    PathBuffer* buffer,
    char (&path_buf)[kMaxPathLength],
    const char* filename,
    const ScannerData* scanner_config,
    const IncludeData* include)
{
  PathBuffer filename_buf;
  PathInit(&filename_buf, filename);

  PathBuffer include_buf;
  PathInit(&include_buf, include->m_String);

  if (!include->m_IsSystemInclude)
  {
    // Try a relative include path for ""-style includes.
    *buffer = filename_buf;
    PathStripLast(buffer);
    PathConcat(buffer, &include_buf);
    PathFormat(path_buf, buffer);
    FileInfo info = StatCacheStat(stat_cache, path_buf);
    if (info.Exists())
      return true;
  }

  for (const char* include_path : scanner_config->m_IncludePaths)
  {
    PathInit(buffer, include_path);
    PathConcat(buffer, &include_buf);
    PathFormat(path_buf, buffer);

    FileInfo info = StatCacheStat(stat_cache, path_buf);
    if (info.Exists())
      return true;
  }

  return false;
}


static void ScanFile(
    StatCache* stat_cache,
    const char* filename,
    char* file_data,
    const ScanInput* input,
    Buffer<const char*>* found_includes)
{
  MemAllocHeap      *heap           = input->m_ScratchHeap;
  MemAllocLinear    *scratch        = input->m_ScratchAlloc;
  const ScannerData *scanner_config = input->m_ScannerConfig;
  IncludeData       *includes       = nullptr;

  switch (scanner_config->m_ScannerType)
  {
    case ScannerType::kGeneric:
      includes = ScanIncludesGeneric(file_data, scratch, *static_cast<const GenericScannerData*>(scanner_config));
      break;
    case ScannerType::kCpp:
      includes = ScanIncludesCpp(file_data, scratch);
      break;
    default:
      Croak("Unsupported scanner type");
  }

  // Resolve includes to file paths.
  IncludeData* include = includes;

  while (include)
  {
    PathBuffer path;
    char path_buf[kMaxPathLength];

    if (FindFile(stat_cache, &path, path_buf, filename, scanner_config, include))
    {
      BufferAppendOne(found_includes, heap, StrDup(scratch, path_buf));
    }

    include = include->m_Next;
  }
}

bool ScanImplicitDeps(StatCache* stat_cache, const ScanInput* input, ScanOutput* output)
{
  MemAllocHeap      *scratch_heap   = input->m_ScratchHeap;
  MemAllocLinear    *scratch_alloc  = input->m_ScratchAlloc;
  const ScannerData *scanner_config = input->m_ScannerConfig;
  ScanCache         *scan_cache     = input->m_ScanCache;

  Buffer<const char*> found_includes;
  Buffer<const char*> filename_stack;
  BufferInitWithCapacity(&found_includes, scratch_heap, 128);
  BufferInitWithCapacity(&filename_stack, scratch_heap, 128);
  BufferAppendOne(&filename_stack, scratch_heap, input->m_FileName);

  IncludeSet incset;
  IncludeSetInit(&incset, scratch_heap, scratch_alloc);

  while (filename_stack.m_Size > 0)
  {
    const char* fn = BufferPopOne(&filename_stack);

    // Compute key for scan cache lookup/insert
    // Reuse scan key for initial file.
    HashDigest scan_key;

    FileInfo info = StatCacheStat(stat_cache, fn);

    if (!info.Exists())
      continue;

    ComputeScanCacheKey(&scan_key, fn, scanner_config->m_ScannerGuid);

    ScanCacheLookupResult cache_result;

    if (ScanCacheLookup(scan_cache, scan_key, info.m_Timestamp, &cache_result, scratch_alloc))
    {
      int                 file_count = cache_result.m_IncludedFileCount;
      const FileAndHash  *files      = cache_result.m_IncludedFiles;

      for (int i = 0; i < file_count; ++i)
      {
        if (IncludeSetAddNoDuplicateString(&incset, files[i].m_Filename, files[i].m_FilenameHash))
        {
          // This was a new file, schedule it for scanning as well. 
          BufferAppendOne(&filename_stack, scratch_heap, files[i].m_Filename);
        }
      }
    }
    else
    {
      // Reset buffer
      BufferClear(&found_includes);

      // Read file into RAM, and add a terminating newline character.
      FILE* f = fopen(fn, "rb");
      if (!f)
        continue;

      if (0 != fseek(f, 0, SEEK_END))
      {
        fclose(f);
        continue;
      }

      long file_size = ftell(f);
      if (-1 == file_size || 0 == file_size)
      {
        fclose(f);
        continue;
      }

      rewind(f);

      char* buffer = (char*) HeapAllocate(scratch_heap, file_size + 2);
      if (1 == (long) fread(buffer, file_size, 1, f))
      {
        // Add an extra newline to sort out trailing #includes on last line
        buffer[file_size + 0] = '\n';
        buffer[file_size + 1] = '\0';

        char* scan_start = buffer;

        // Skip UTF-8 marker if present as it freaks out ctype functions
        static const unsigned char utf8_mark[] = { 0xef, 0xbb, 0xbf };
        if (file_size >= 3 && 0 == memcmp(scan_start, utf8_mark, sizeof utf8_mark))
          scan_start += sizeof utf8_mark;

        ScanFile(stat_cache, fn, scan_start, input, &found_includes);
      }

      // Insert result into scan cache
      ScanCacheInsert(scan_cache, scan_key, info.m_Timestamp, found_includes.m_Storage, (int) found_includes.m_Size);

      for (const char* file : found_includes)
      {
        if (IncludeSetAddDuplicateString(&incset, file, Djb2HashPath(file)))
        {
          // This was a new file, schedule it for scanning as well. 
          BufferAppendOne(&filename_stack, scratch_heap, file);
        }
      }

      HeapFree(scratch_heap, buffer);
      fclose(f);
    }
  }

  // Allocate space for output array. String data is already in scratch allocator.
  int include_count = incset.m_HashTable.m_RecordCount;
  FileAndHash* result = LinearAllocateArray<FileAndHash>(scratch_alloc, include_count);
  HashSetWalk(&incset.m_HashTable, [=] (uint32_t index, uint32_t hash, const char* path) {
    result[index].m_Filename = path;
    result[index].m_FilenameHash = hash;
  });

  BufferDestroy(&filename_stack, scratch_heap);
  BufferDestroy(&found_includes, scratch_heap);

  output->m_IncludedFileCount = include_count;
  output->m_IncludedFiles     = result;

  IncludeSetDestroy(&incset);
  return true;
}

}
