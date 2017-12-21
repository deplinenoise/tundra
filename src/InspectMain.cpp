#include "Common.hpp"
#include "DagData.hpp"
#include "StateData.hpp"
#include "ScanData.hpp"
#include "DigestCache.hpp"
#include "MemoryMappedFile.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace t2;


static void DumpDag(const DagData* data)
{
  int node_count = data->m_NodeCount;
  printf("magic number: 0x%08x\n", data->m_MagicNumber);
  printf("node count: %u\n", node_count);
  for (int i = 0; i < node_count; ++i)
  {
    printf("node %d:\n", i);
    char digest_str[kDigestStringSize];
    DigestToString(digest_str, data->m_NodeGuids[i]);

    const NodeData& node = data->m_NodeData[i];

    printf("  guid: %s\n", digest_str);
    printf("  flags:");
    if (node.m_Flags & NodeData::kFlagPreciousOutputs) printf(" precious");
    if (node.m_Flags & NodeData::kFlagOverwriteOutputs) printf(" overwrite");
    if (node.m_Flags & NodeData::kFlagExpensive) printf(" expensive");
    printf("\n  action: %s\n", node.m_Action.Get());
    printf("  preaction: %s\n", node.m_PreAction.Get() ? node.m_PreAction.Get() : "(null)");
    printf("  annotation: %s\n", node.m_Annotation.Get());
    printf("  pass index: %u\n", node.m_PassIndex);

    printf("  dependencies:");
    for (int32_t dep : node.m_Dependencies)
      printf(" %u", dep);
    printf("\n");

    printf("  backlinks:");
    for (int32_t link : node.m_BackLinks)
      printf(" %u", link);
    printf("\n");

    printf("  inputs:\n");
    for (const FrozenFileAndHash& f : node.m_InputFiles)
      printf("    %s (0x%08x)\n", f.m_Filename.Get(), f.m_FilenameHash);

    printf("  outputs:\n");
    for (const FrozenFileAndHash& f : node.m_OutputFiles)
      printf("    %s (0x%08x)\n", f.m_Filename.Get(), f.m_FilenameHash);

    printf("  aux_outputs:\n");
    for (const FrozenFileAndHash& f : node.m_AuxOutputFiles)
      printf("    %s (0x%08x)\n", f.m_Filename.Get(), f.m_FilenameHash);

    printf("  environment:\n");
    for (const EnvVarData& env : node.m_EnvVars)
    {
      printf("    %s = %s\n", env.m_Name.Get(), env.m_Value.Get());
    }

    if (const ScannerData* s = node.m_Scanner)
    {
      printf("  scanner:\n");
      switch (s->m_ScannerType)
      {
        case ScannerType::kCpp:
          printf("    type: cpp\n");
          break;
        case ScannerType::kGeneric:
          printf("    type: generic\n");
          break;
        default:
          printf("    type: garbage!\n");
          break;
      }

      printf("    include paths:\n");
      for (const char* path : s->m_IncludePaths)
      {
        printf("      %s\n", path);
      }
      DigestToString(digest_str, s->m_ScannerGuid);
      printf("    scanner guid: %s\n", digest_str);


      if (ScannerType::kGeneric == s->m_ScannerType)
      {
        const GenericScannerData* gs = static_cast<const GenericScannerData*>(s);
        printf("    flags:");
        if (GenericScannerData::kFlagRequireWhitespace & gs->m_Flags)
          printf(" RequireWhitespace");
        if (GenericScannerData::kFlagUseSeparators & gs->m_Flags)
          printf(" UseSeparators");
        if (GenericScannerData::kFlagBareMeansSystem & gs->m_Flags)
          printf(" BareMeansSystem");
        printf("\n");

        printf("    keywords:\n");
        for (const KeywordData& kw : gs->m_Keywords)
        {
          printf("      \"%s\" (%d bytes) follow: %s\n",
              kw.m_String.Get(), kw.m_StringLength, kw.m_ShouldFollow ? "yes" : "no");
        }
      }
    }

    printf("\n");
  }

  printf("\npass count: %u\n", data->m_Passes.GetCount());
  for (const PassData& pass : data->m_Passes)
  {
    printf("  pass: %s\n", pass.m_PassName.Get());
  }

  printf("\nconfig count: %u\n", data->m_ConfigCount);
  for (int i = 0; i < data->m_ConfigCount; ++i)
  {
    printf("  %d: name=\"%s\" hash=0x%08x\n", i, data->m_ConfigNames[i].Get(), data->m_ConfigNameHashes[i]);
  }

  printf("\nvariant count: %u\n", data->m_VariantCount);
  for (int i = 0; i < data->m_VariantCount; ++i)
  {
    printf("  %d: name=\"%s\" hash=0x%08x\n", i, data->m_VariantNames[i].Get(), data->m_VariantNameHashes[i]);
  }

  printf("\nsubvariant count: %u\n", data->m_SubVariantCount);
  for (int i = 0; i < data->m_SubVariantCount; ++i)
  {
    printf("  %d: name=\"%s\" hash=0x%08x\n", i, data->m_SubVariantNames[i].Get(), data->m_SubVariantNameHashes[i]);
  }

  printf("\ndefault config index: %d\n", data->m_DefaultConfigIndex);
  printf("default variant index: %d\n", data->m_DefaultVariantIndex);
  printf("default subvariant index: %d\n", data->m_DefaultSubVariantIndex);

  printf("\nbuild tuples:\n");
  for (const BuildTupleData& tuple : data->m_BuildTuples)
  {
    printf("config index    : %d\n", tuple.m_ConfigIndex);
    printf("variant index   : %d\n", tuple.m_VariantIndex);
    printf("subvariant index: %d\n", tuple.m_SubVariantIndex);
    printf("always nodes    :");
    for (int node : tuple.m_AlwaysNodes)
      printf(" %d", node);
    printf("\n");
    printf("default nodes   :");
    for (int node : tuple.m_DefaultNodes)
      printf(" %d", node);
    printf("\n");
    printf("named nodes:\n");
    for (const NamedNodeData& nn : tuple.m_NamedNodes)
      printf("  %s - node %d\n", nn.m_Name.Get(), nn.m_NodeIndex);
    printf("\n");
  }

  printf("\nfile signatures:\n");
  for (const DagFileSignature& sig : data->m_FileSignatures)
  {
    printf("file            : %s\n", sig.m_Path.Get());
    printf("timestamp       : %u\n", (unsigned int) sig.m_Timestamp);
  }
  printf("\nglob signatures:\n");
  for (const DagGlobSignature& sig : data->m_GlobSignatures)
  {
    char digest_str[kDigestStringSize];
    DigestToString(digest_str, sig.m_Digest);
    printf("path            : %s\n", sig.m_Path.Get());
    printf("digest          : %s\n", digest_str);
  }

  printf("\nSHA-1 signatures enabled for extension hashes:\n");
  for (const uint32_t ext : data->m_ShaExtensionHashes)
  {
    printf("hash            : 0x%08x\n", ext);
  }

  printf("\nMax expensive jobs: %d\n", data->m_MaxExpensiveCount);
}

static void DumpState(const StateData* data)
{
  int node_count = data->m_NodeCount;
  printf("magic number: 0x%08x\n", data->m_MagicNumber);
  printf("node count: %u\n", node_count);
  for (int i = 0; i < node_count; ++i)
  {
    printf("node %d:\n", i);
    char digest_str[kDigestStringSize];

    const NodeStateData& node = data->m_NodeStates[i];

    DigestToString(digest_str, data->m_NodeGuids[i]);
    printf("  guid: %s\n", digest_str);
    printf("  build result: %d\n", node.m_BuildResult);
    DigestToString(digest_str, node.m_InputSignature);
    printf("  input_signature: %s\n", digest_str);
    printf("  outputs:\n");
    for (const char* path : node.m_OutputFiles)
      printf("    %s\n", path);
    printf("  aux outputs:\n");
    for (const char* path : node.m_AuxOutputFiles)
      printf("    %s\n", path);
    printf("\n");
  }
}

static void DumpScanCache(const ScanData* data)
{
  int entry_count = data->m_EntryCount;
  printf("magic number: 0x%08x\n", data->m_MagicNumber);
  printf("entry count: %d\n", entry_count);
  for (int i = 0; i < entry_count; ++i)
  {
    printf("entry %d:\n", i);
    char digest_str[kDigestStringSize];

    const ScanCacheEntry& entry = data->m_Data[i];

    DigestToString(digest_str, data->m_Keys[i]);
    printf("  guid: %s\n", digest_str);
    printf("  access time stamp: %llu\n", (long long unsigned int) data->m_AccessTimes[i]);
    printf("  file time stamp: %llu\n", (long long unsigned int) entry.m_FileTimestamp);
    printf("  included files:\n");
    for (const FrozenFileAndHash& path : entry.m_IncludedFiles)
      printf("    %s (0x%08x)\n", path.m_Filename.Get(), path.m_FilenameHash);
  }
}

static const char* FmtTime(uint64_t t)
{
  time_t tt = (time_t) t;
  static char time_buf[128];
  strftime(time_buf, sizeof time_buf, "%F %H:%M:%S", localtime(&tt));
  return time_buf;
}

static void DumpDigestCache(const DigestCacheState* data)
{
  printf("record count: %d\n", data->m_Records.GetCount());
  for (const FrozenDigestRecord& r : data->m_Records)
  {
    char digest_str[kDigestStringSize];
    printf("  filename     : %s\n", r.m_Filename.Get());
    printf("  filename hash: %08x\n", r.m_FilenameHash);
    DigestToString(digest_str, r.m_ContentDigest);
    printf("  digest SHA1  : %s\n", digest_str);
    printf("  access time  : %s\n", FmtTime(r.m_AccessTime));
    printf("  timestamp    : %s\n", FmtTime(r.m_Timestamp));
    printf("\n");
  }
}

int main(int argc, char* argv[])
{
  MemoryMappedFile f;

  const char* fn = argc >=2 ? argv[1] : ".tundra2.dag";

  MmapFileInit(&f);
  MmapFileMap(&f, fn);

  if (MmapFileValid(&f))
  {
    const char* suffix = strrchr(fn, '.');

    if (0 == strcmp(suffix, ".dag"))
    {
      const DagData* data = (const DagData*) f.m_Address;
      if (data->m_MagicNumber == DagData::MagicNumber)
      {
        DumpDag(data);
      }
      else
      {
        fprintf(stderr, "%s: bad magic number\n", fn);
      }
    }
    else if (0 == strcmp(suffix, ".state"))
    {
      const StateData* data = (const StateData*) f.m_Address;
      if (data->m_MagicNumber == StateData::MagicNumber)
      {
        DumpState(data);
      }
      else
      {
        fprintf(stderr, "%s: bad magic number\n", fn);
      }
    }
    else if (0 == strcmp(suffix, ".scancache"))
    {
      const ScanData* data = (const ScanData*) f.m_Address;
      if (data->m_MagicNumber == ScanData::MagicNumber)
      {
        DumpScanCache(data);
      }
      else
      {
        fprintf(stderr, "%s: bad magic number\n", fn);
      }
    }
    else if (0 == strcmp(suffix, ".digestcache"))
    {
      const DigestCacheState* data = (const DigestCacheState*) f.m_Address;
      if (data->m_MagicNumber == DigestCacheState::MagicNumber)
      {
        DumpDigestCache(data);
      }
      else
      {
        fprintf(stderr, "%s: bad magic number\n", fn);
      }
    }
    else
    {
      fprintf(stderr, "%s: unknown file type\n", fn);
    }
  }
  else
  {
    fprintf(stderr, "%s: couldn't mmap file\n", fn);
  }

  MmapFileUnmap(&f);
  return 0;
}
