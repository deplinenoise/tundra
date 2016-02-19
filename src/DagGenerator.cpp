#include "DagGenerator.hpp"
#include "Hash.hpp"
#include "PathUtil.hpp"
#include "Exec.hpp"
#include "FileInfo.hpp"
#include "MemAllocHeap.hpp"
#include "MemAllocLinear.hpp"
#include "JsonParse.hpp"
#include "BinaryWriter.hpp"
#include "DagData.hpp"
#include "HashTable.hpp"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <algorithm>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

namespace t2
{

static void WriteStringPtr(BinarySegment* seg, BinarySegment *str_seg, const char* text)
{
  if (text)
  {
    BinarySegmentWritePointer(seg, BinarySegmentPosition(str_seg));
    BinarySegmentWriteStringData(str_seg, text);
  }
  else
  {
    BinarySegmentWriteNullPointer(seg);
  }
}

static const char* FindStringValue(const JsonValue* obj, const char* key)
{
  if (JsonValue::kObject != obj->m_Type)
    return nullptr;

  const JsonValue *node = obj->Find(key);

  if (!node)
    return nullptr;

  if (JsonValue::kString != node->m_Type)
    return nullptr;

  return static_cast<const JsonStringValue*>(node)->m_String;
}

static const JsonArrayValue* FindArrayValue(const JsonObjectValue* obj, const char* key)
{
  if (obj == nullptr)
    return nullptr;

  const JsonValue *node = obj->Find(key);

  if (!node)
    return nullptr;
  if (JsonValue::kArray != node->m_Type)
    return nullptr;
  return static_cast<const JsonArrayValue*>(node);
}

static const JsonObjectValue* FindObjectValue(const JsonObjectValue* obj, const char* key)
{
  const JsonValue *node = obj->Find(key);
  if (!node)
    return nullptr;
  if (JsonValue::kObject != node->m_Type)
    return nullptr;
  return static_cast<const JsonObjectValue*>(node);
}

static int64_t FindIntValue(const JsonObjectValue* obj, const char* key, int64_t def_value)
{
  const JsonValue *node = obj->Find(key);
  if (!node)
    return def_value;
  if (JsonValue::kNumber != node->m_Type)
    return def_value;
  return (int64_t) static_cast<const JsonNumberValue*>(node)->m_Number;
}

static bool WriteFileArray(
    BinarySegment* seg,
    BinarySegment* ptr_seg,
    BinarySegment* str_seg,
    const JsonArrayValue* files)
{
  if (!files || 0 == files->m_Count)
  {
    BinarySegmentWriteInt32(seg, 0);
    BinarySegmentWriteNullPointer(seg);
    return true;
  }

  BinarySegmentWriteInt32(seg, (int) files->m_Count);
  BinarySegmentWritePointer(seg, BinarySegmentPosition(ptr_seg));

  for (size_t i = 0, count = files->m_Count; i < count; ++i)
  {
    const JsonStringValue *path = files->m_Values[i]->AsString();
    if (!path)
      return false;

    PathBuffer pathbuf;
    PathInit(&pathbuf, path->m_String);

    char cleaned_path[kMaxPathLength];
    PathFormat(cleaned_path, &pathbuf);

    WriteStringPtr(ptr_seg, str_seg, cleaned_path);
    BinarySegmentWriteUint32(ptr_seg, Djb2HashPath(cleaned_path));
  }

  return true;
}

static bool EmptyArray(const JsonArrayValue* a)
{
  return nullptr == a || a->m_Count == 0;
}

struct TempNodeGuid
{
  HashDigest m_Digest;
  int32_t    m_Node;

  bool operator<(const TempNodeGuid& other) const
  {
    return m_Digest < other.m_Digest;
  }
};

struct CommonStringRecord : HashRecord
{
  BinaryLocator m_Pointer;
};

void WriteCommonStringPtr(BinarySegment* segment, BinarySegment* str_seg, const char* ptr, HashTable* table, MemAllocLinear* scratch)
{
  uint32_t hash = Djb2Hash(ptr);
  CommonStringRecord* r;
  if (nullptr == (r = static_cast<CommonStringRecord*>(HashTableLookup(table, hash, ptr))))
  {
    r = LinearAllocate<CommonStringRecord>(scratch);
    r->m_Hash = hash;
    r->m_String = ptr;
    r->m_Next = nullptr;
    r->m_Pointer = BinarySegmentPosition(str_seg);
    BinarySegmentWriteStringData(str_seg, ptr);
  }

  BinarySegmentWritePointer(segment, r->m_Pointer);
}

static uint32_t GetNodeFlag(const JsonObjectValue* node, const char* name, uint32_t value)
{
  uint32_t result = 0;

  if (const JsonValue* val = node->Find(name))
  {
    if (const JsonBooleanValue* flag = val->AsBoolean())
    {
      if (flag->m_Boolean)
        result = value;
    }
  }

  return result;
}

static bool WriteNodes(
    const JsonArrayValue* nodes,
    BinarySegment* main_seg,
    BinarySegment* node_data_seg,
    BinarySegment* array2_seg,
    BinarySegment* str_seg,
    BinaryLocator scanner_ptrs[],
    MemAllocHeap* heap,
    HashTable* shared_strings,
    MemAllocLinear* scratch,
    const TempNodeGuid* order,
    const int32_t* remap_table)
{
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(node_data_seg));  // m_NodeData

  MemAllocLinearScope scratch_scope(scratch);

  size_t node_count = nodes->m_Count;
  
  struct BacklinkRec
  {
    Buffer<int32_t> m_Links;
  };

  BacklinkRec* links = HeapAllocateArrayZeroed<BacklinkRec>(heap, node_count);

  for (size_t i = 0; i < node_count; ++i)
  {
    const JsonObjectValue* node = nodes->m_Values[i]->AsObject();
    if (!node)
      return false;

    const JsonArrayValue *deps          = FindArrayValue(node, "Deps");

    if (EmptyArray(deps))
      continue;

    for (size_t di = 0, count = deps->m_Count; di < count; ++di)
    {
      if (const JsonNumberValue* dep_index_n = deps->m_Values[di]->AsNumber())
      {
        int32_t dep_index = (int) dep_index_n->m_Number;
        if (dep_index < 0 || dep_index >= (int) node_count)
          return false;

        BufferAppendOne(&links[dep_index].m_Links, heap, int32_t(i));
      }
      else
      {
        return false;
      }
    }
  }

  for (size_t ni = 0; ni < node_count; ++ni)
  {
    const int32_t i = order[ni].m_Node;
    const JsonObjectValue* node = nodes->m_Values[i]->AsObject();

    const char           *action        = FindStringValue(node, "Action");
    const char           *annotation    = FindStringValue(node, "Annotation");
    const char           *preaction     = FindStringValue(node, "PreAction");
    const int             pass_index    = (int) FindIntValue(node, "PassIndex", 0);
    const JsonArrayValue *deps          = FindArrayValue(node, "Deps");
    const JsonArrayValue *inputs        = FindArrayValue(node, "Inputs");
    const JsonArrayValue *outputs       = FindArrayValue(node, "Outputs");
    const JsonArrayValue *aux_outputs   = FindArrayValue(node, "AuxOutputs");
    const JsonArrayValue *env_vars      = FindArrayValue(node, "Env");
    const int             scanner_index = (int) FindIntValue(node, "ScannerIndex", -1);

    WriteStringPtr(node_data_seg, str_seg, action);
    WriteStringPtr(node_data_seg, str_seg, preaction);
    WriteCommonStringPtr(node_data_seg, str_seg, annotation, shared_strings, scratch);
    BinarySegmentWriteInt32(node_data_seg, pass_index);

    if (deps)
    {
      BinarySegmentAlign(array2_seg, 4);
      BinarySegmentWriteInt32(node_data_seg, (int) deps->m_Count);
      BinarySegmentWritePointer(node_data_seg, BinarySegmentPosition(array2_seg));
      for (size_t i = 0, count = deps->m_Count; i < count; ++i)
      {
        if (const JsonNumberValue* dep_index = deps->m_Values[i]->AsNumber())
        {
          int index = (int) dep_index->m_Number;
          int remapped_index = remap_table[index];
          BinarySegmentWriteInt32(array2_seg, remapped_index);
        }
        else
        {
          return false;
        }
      }
    }
    else
    {
      BinarySegmentWriteInt32(node_data_seg, 0);
      BinarySegmentWriteNullPointer(node_data_seg);
    }
    
    const Buffer<int32_t>& backlinks = links[i].m_Links;
    if (backlinks.m_Size > 0)
    {
      BinarySegmentWriteInt32(node_data_seg, (int) backlinks.m_Size);
      BinarySegmentWritePointer(node_data_seg, BinarySegmentPosition(array2_seg));
      for (int32_t index : backlinks)
      {
        BinarySegmentWriteInt32(array2_seg, remap_table[index]);
      }
    }
    else
    {
      BinarySegmentWriteInt32(node_data_seg, 0);
      BinarySegmentWriteNullPointer(node_data_seg);
    }

    WriteFileArray(node_data_seg, array2_seg, str_seg, inputs);
    WriteFileArray(node_data_seg, array2_seg, str_seg, outputs);
    WriteFileArray(node_data_seg, array2_seg, str_seg, aux_outputs);

    // Environment variables
    if (env_vars && env_vars->m_Count > 0)
    {
      BinarySegmentAlign(array2_seg, 4);
      BinarySegmentWriteInt32(node_data_seg, (int) env_vars->m_Count);
      BinarySegmentWritePointer(node_data_seg, BinarySegmentPosition(array2_seg));
      for (size_t i = 0, count = env_vars->m_Count; i < count; ++i)
      {
        const char* key = FindStringValue(env_vars->m_Values[i], "Key");
        const char* value = FindStringValue(env_vars->m_Values[i], "Value");

        if (!key || !value)
          return false;

        WriteCommonStringPtr(array2_seg, str_seg, key, shared_strings, scratch);
        WriteCommonStringPtr(array2_seg, str_seg, value, shared_strings, scratch);
      }
    }
    else
    {
      BinarySegmentWriteInt32(node_data_seg, 0);
      BinarySegmentWriteNullPointer(node_data_seg);
    }

    if (-1 != scanner_index)
    {
      BinarySegmentWritePointer(node_data_seg, scanner_ptrs[scanner_index]);
    }
    else
    {
      BinarySegmentWriteNullPointer(node_data_seg);
    }

    uint32_t flags = 0;
    
    flags |= GetNodeFlag(node, "OverwriteOutputs", NodeData::kFlagOverwriteOutputs);
    flags |= GetNodeFlag(node, "PreciousOutputs",  NodeData::kFlagPreciousOutputs);
    flags |= GetNodeFlag(node, "Expensive",        NodeData::kFlagExpensive);

    BinarySegmentWriteUint32(node_data_seg, flags);
  }

  for (size_t i = 0; i < node_count; ++i)
  {
    BufferDestroy(&links[i].m_Links, heap);
  }

  HeapFree(heap, links);

  return true;
}

static bool WriteStrHashArray(
    BinarySegment* main_seg,
    BinarySegment* aux_seg,
    BinarySegment* str_seg,
    const JsonArrayValue* strings)
{
  BinarySegmentWriteInt32(main_seg, (int) strings->m_Count);
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));
  for (size_t i = 0, count = strings->m_Count; i < count; ++i)
  {
    const char* str = strings->m_Values[i]->GetString();
    if (!str)
      return false;
    WriteStringPtr(aux_seg, str_seg, str);
  }
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));
  for (size_t i = 0, count = strings->m_Count; i < count; ++i)
  {
    const char* str = strings->m_Values[i]->GetString();
    uint32_t hash = Djb2Hash(str);
    BinarySegmentWriteUint32(aux_seg, hash);
  }

  return true;
}

static bool WriteNodeArray(BinarySegment* top_seg, BinarySegment* data_seg, const JsonArrayValue* ints, const int32_t remap_table[])
{
  BinarySegmentWriteInt32(top_seg, (int) ints->m_Count);
  BinarySegmentWritePointer(top_seg, BinarySegmentPosition(data_seg));

  for (size_t i = 0, count = ints->m_Count; i < count; ++i)
  {
    if (const JsonNumberValue* num = ints->m_Values[i]->AsNumber())
    {
      int index = remap_table[(int) num->m_Number];
      BinarySegmentWriteInt32(data_seg, index);
    }
    else
      return false;
  }

  return true;
}

static bool SortJsonStrings(const JsonValue* l, const JsonValue* r)
{
  const char* ls = l->GetString();
  const char* rs = r->GetString();

  if (ls == rs)
    return false;

  if (nullptr == ls)
    return true;

  if (nullptr == rs)
    return false;

  return strcmp(ls, rs) < 0;
}

static bool GetBoolean(const JsonObjectValue* obj, const char* name)
{
  if (const JsonValue* val = obj->Find(name))
  {
    if (const JsonBooleanValue* b = val->AsBoolean())
    {
      return b->m_Boolean;
    }
  }

  return false;
}

static bool WriteScanner(BinaryLocator* ptr_out, BinarySegment* seg, BinarySegment* array_seg, BinarySegment* str_seg, const JsonObjectValue* data, HashTable* shared_strings, MemAllocLinear* scratch)
{
  if (!data)
    return false;

  const char* kind = FindStringValue(data, "Kind");
  const JsonArrayValue* incpaths = FindArrayValue(data, "IncludePaths");

  if (!kind || !incpaths)
    return false;

  BinarySegmentAlign(seg, 4);
  *ptr_out = BinarySegmentPosition(seg);

  ScannerType::Enum type;
  if (0 == strcmp(kind, "cpp"))
    type = ScannerType::kCpp;
  else if (0 == strcmp(kind, "generic"))
    type = ScannerType::kGeneric;
  else
    return false;

  BinarySegmentWriteInt32(seg, type);
  BinarySegmentWriteInt32(seg, (int) incpaths->m_Count);
  BinarySegmentWritePointer(seg, BinarySegmentPosition(array_seg));
  HashState h;
  HashInit(&h);
  HashAddString(&h, kind);
  for (size_t i = 0, count = incpaths->m_Count; i < count; ++i)
  {
    const char* path = incpaths->m_Values[i]->GetString();
    if (!path)
      return false;
    HashAddString(&h, path);
    WriteCommonStringPtr(array_seg, str_seg, path, shared_strings, scratch);
  }

  void* digest_space = BinarySegmentAlloc(seg, sizeof(HashDigest));
  
  if (ScannerType::kGeneric == type)
  {
    uint32_t flags = 0;

    if (GetBoolean(data, "RequireWhitespace"))
      flags |= GenericScannerData::kFlagRequireWhitespace;
    if (GetBoolean(data, "UseSeparators"))
      flags |= GenericScannerData::kFlagUseSeparators;
    if (GetBoolean(data, "BareMeansSystem"))
      flags |= GenericScannerData::kFlagBareMeansSystem;

    BinarySegmentWriteUint32(seg, flags);

    const JsonArrayValue* follow_kws = FindArrayValue(data, "Keywords");
    const JsonArrayValue* nofollow_kws = FindArrayValue(data, "KeywordsNoFollow");

    size_t kw_count =
      (follow_kws ? follow_kws->m_Count : 0) + 
      (nofollow_kws ? nofollow_kws->m_Count : 0);

    BinarySegmentWriteInt32(seg, (int) kw_count);
    if (kw_count > 0)
    {
      BinarySegmentAlign(array_seg, 4);
      BinarySegmentWritePointer(seg, BinarySegmentPosition(array_seg));
      auto write_kws = [array_seg, str_seg](const JsonArrayValue* array, bool follow) -> bool
      {
        if (array)
        {
          for (size_t i = 0, count = array->m_Count; i < count; ++i)
          {
            const JsonStringValue* value = array->m_Values[i]->AsString();
            if (!value)
              return false;
            WriteStringPtr(array_seg, str_seg, value->m_String);
            BinarySegmentWriteInt16(array_seg, (int16_t) strlen(value->m_String));
            BinarySegmentWriteUint8(array_seg, follow ? 1 : 0);
            BinarySegmentWriteUint8(array_seg, 0);
          }
        }
        return true;
      };
      if (!write_kws(follow_kws, true))
        return false;
      if (!write_kws(nofollow_kws, false))
        return false;
    }
    else
    {
      BinarySegmentWriteNullPointer(seg);
    }
  }

  HashFinalize(&h, static_cast<HashDigest*>(digest_space));

  return true;
}

bool ComputeNodeGuids(const JsonArrayValue* nodes, int32_t* remap_table, TempNodeGuid* guid_table)
{
  size_t node_count = nodes->m_Count;
  for (size_t i = 0; i < node_count; ++i)
  {
    const JsonObjectValue* nobj = nodes->m_Values[i]->AsObject();

    if (!nobj)
      return false;

    guid_table[i].m_Node = (int) i;
    
    HashState h;
    HashInit(&h);

    const char           *action     = FindStringValue(nobj, "Action");
    const JsonArrayValue *inputs     = FindArrayValue(nobj, "Inputs");

    if (action && action[0])
      HashAddString(&h, action);

    if (inputs)
    {
      for (size_t fi = 0, fi_count = inputs->m_Count; fi < fi_count; ++fi)
      {
        if (const JsonStringValue* str = inputs->m_Values[fi]->AsString())
        {
          HashAddString(&h, str->m_String);
        }
      }
    }

    const char *annotation = FindStringValue(nobj, "Annotation");
	if (annotation)
		HashAddString(&h, annotation);

    if ((!action || action[0] == '\0') && !inputs && !annotation)
    {
        return false;
    }

    HashFinalize(&h, &guid_table[i].m_Digest);
  }

  std::sort(guid_table, guid_table + node_count);

  for (size_t i = 1; i < node_count; ++i)
  {
    if (guid_table[i-1].m_Digest == guid_table[i].m_Digest)
    {
      int i0 = guid_table[i-1].m_Node;
      int i1 = guid_table[i].m_Node;
      const JsonObjectValue* o0 = nodes->m_Values[i0]->AsObject();
      const JsonObjectValue* o1 = nodes->m_Values[i1]->AsObject();
      const char* anno0 = FindStringValue(o0, "Annotation");
      const char* anno1 = FindStringValue(o1, "Annotation");
      char digest[kDigestStringSize];
      DigestToString(digest, guid_table[i].m_Digest);
      Log(kError, "duplicate node guids: %s and %s share common GUID (%s)", anno0, anno1, digest);
      return false;
    }
  }

  for (size_t i = 0; i < node_count; ++i)
  {
    remap_table[guid_table[i].m_Node] = (int32_t) i;
  }

  return true;
}


static bool CompileDag(const JsonObjectValue* root, BinaryWriter* writer, MemAllocHeap* heap, MemAllocLinear* scratch)
{
  printf("compiling mmapable DAG data..\n"); 

  HashTable shared_strings;
  HashTableInit(&shared_strings, heap, 0);

  BinarySegment         *main_seg      = BinaryWriterAddSegment(writer);
  BinarySegment         *node_guid_seg = BinaryWriterAddSegment(writer);
  BinarySegment         *node_data_seg = BinaryWriterAddSegment(writer);
  BinarySegment         *aux_seg       = BinaryWriterAddSegment(writer);
  BinarySegment         *aux2_seg      = BinaryWriterAddSegment(writer);
  BinarySegment         *str_seg       = BinaryWriterAddSegment(writer);

  const JsonArrayValue  *nodes         = FindArrayValue(root, "Nodes");
  const JsonArrayValue  *passes        = FindArrayValue(root, "Passes");
  const JsonArrayValue  *scanners      = FindArrayValue(root, "Scanners");

  if (EmptyArray(nodes))
  {
    fprintf(stderr, "invalid Nodes data\n");
    return false;
  }

  if (EmptyArray(passes))
  {
    fprintf(stderr, "invalid Passes data\n");
    return false;
  }

  // Write scanners, store pointers
  BinaryLocator* scanner_ptrs = nullptr;

  if (!EmptyArray(scanners))
  {
    scanner_ptrs = (BinaryLocator*) alloca(sizeof(BinaryLocator) * scanners->m_Count);
    for (size_t i = 0, count = scanners->m_Count; i < count; ++i)
    {
      if (!WriteScanner(&scanner_ptrs[i], aux_seg, aux2_seg, str_seg, scanners->m_Values[i]->AsObject(), &shared_strings, scratch))
      {
        fprintf(stderr, "invalid scanner data\n");
        return false;
      }
    }
  }
  
  // Write magic number
  BinarySegmentWriteUint32(main_seg, DagData::MagicNumber);

  // Compute node guids and index remapping table.
  // FIXME: this just leaks
  int32_t      *remap_table = HeapAllocateArray<int32_t>(heap, nodes->m_Count);
  TempNodeGuid *guid_table  = HeapAllocateArray<TempNodeGuid>(heap, nodes->m_Count);

  if (!ComputeNodeGuids(nodes, remap_table, guid_table))
    return false;

  // m_NodeCount
  size_t node_count = nodes->m_Count;
  BinarySegmentWriteInt32(main_seg, int(node_count));

  // Write node guids
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(node_guid_seg));  // m_NodeGuids
  for (size_t i = 0; i < node_count; ++i)
  {
    BinarySegmentWrite(node_guid_seg, (char*) &guid_table[i].m_Digest, sizeof guid_table[i].m_Digest);
  }

  // Write nodes.
  if (!WriteNodes(nodes, main_seg, node_data_seg, aux_seg, str_seg, scanner_ptrs, heap, &shared_strings, scratch, guid_table, remap_table))
    return false;

  // Write passes
  BinarySegmentWriteInt32(main_seg, (int) passes->m_Count);
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));
  for (size_t i = 0, count = passes->m_Count; i < count; ++i)
  {
    const char* pass_name = passes->m_Values[i]->GetString();
    if (!pass_name)
      return false;
    WriteStringPtr(aux_seg, str_seg, pass_name);
  }

  // Write configs
  const JsonObjectValue *setup       = FindObjectValue(root, "Setup");
  const JsonArrayValue  *configs     = FindArrayValue(setup, "Configs");
  const JsonArrayValue  *variants    = FindArrayValue(setup, "Variants");
  const JsonArrayValue  *subvariants = FindArrayValue(setup, "SubVariants");
  const JsonArrayValue  *tuples      = FindArrayValue(setup, "BuildTuples");

  if (nullptr == setup || EmptyArray(configs) || EmptyArray(variants) || EmptyArray(subvariants) || EmptyArray(tuples))
  {
    fprintf(stderr, "invalid Setup data\n");
    return false;
  }

  if (!WriteStrHashArray(main_seg, aux_seg, str_seg, configs))
  {
    fprintf(stderr, "invalid Setup.Configs data\n");
    return false;
  }

  if (!WriteStrHashArray(main_seg, aux_seg, str_seg, variants))
  {
    fprintf(stderr, "invalid Setup.Variants data\n");
    return false;
  }

  if (!WriteStrHashArray(main_seg, aux_seg, str_seg, subvariants))
  {
    fprintf(stderr, "invalid Setup.SubVariants data\n");
    return false;
  }

  BinarySegmentWriteInt32(main_seg, (int) tuples->m_Count);
  BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));

  for (size_t i = 0, count = tuples->m_Count; i < count; ++i)
  {
    const JsonObjectValue* obj = tuples->m_Values[i]->AsObject();
    if (!obj)
    {
      fprintf(stderr, "invalid Setup.BuildTuples[%d] data\n", (int) i);
      return false;
    }

    int                    config_index     = (int) FindIntValue(obj, "ConfigIndex", -1);
    int                    variant_index    = (int) FindIntValue(obj, "VariantIndex", -1);
    int                    subvariant_index = (int) FindIntValue(obj, "SubVariantIndex", -1);
    const JsonArrayValue  *default_nodes    = FindArrayValue(obj, "DefaultNodes");
    const JsonArrayValue  *always_nodes     = FindArrayValue(obj, "AlwaysNodes");
    const JsonObjectValue *named_nodes      = FindObjectValue(obj, "NamedNodes");

    if (config_index == -1 || variant_index == -1 || subvariant_index == -1 ||
        !default_nodes || !always_nodes)
    {
      fprintf(stderr, "invalid Setup.BuildTuples[%d] data\n", (int) i);
      return false;
    }

    BinarySegmentWriteInt32(aux_seg, config_index);
    BinarySegmentWriteInt32(aux_seg, variant_index);
    BinarySegmentWriteInt32(aux_seg, subvariant_index);

    if (!WriteNodeArray(aux_seg, aux2_seg, default_nodes, remap_table))
    {
      fprintf(stderr, "bad DefaultNodes data\n");
      return false;
    }

    if (!WriteNodeArray(aux_seg, aux2_seg, always_nodes, remap_table))
    {
      fprintf(stderr, "bad AlwaysNodes data\n");
      return false;
    }

    if (named_nodes)
    {
      size_t ncount = named_nodes->m_Count;
      BinarySegmentWriteInt32(aux_seg, (int) ncount);
      BinarySegmentWritePointer(aux_seg, BinarySegmentPosition(aux2_seg));
      for (size_t i = 0; i < ncount; ++i)
      {
        WriteStringPtr(aux2_seg, str_seg, named_nodes->m_Names[i]);
        const JsonNumberValue* node_index = named_nodes->m_Values[i]->AsNumber();
        if (!node_index)
        {
          fprintf(stderr, "named node index must be number\n");
          return false;
        }
        int remapped_index = remap_table[(int) node_index->m_Number];
        BinarySegmentWriteInt32(aux2_seg, remapped_index);
      }
    }
    else
    {
      BinarySegmentWriteInt32(aux_seg, 0);
      BinarySegmentWriteNullPointer(aux_seg);
    }
  }

  const JsonObjectValue* default_tuple = FindObjectValue(setup, "DefaultBuildTuple");
  if (!default_tuple)
  {
    fprintf(stderr, "missing Setup.DefaultBuildTuple\n");
    return false;
  }

  int def_config_index = (int) FindIntValue(default_tuple, "ConfigIndex", -2);
  int def_variant_index = (int) FindIntValue(default_tuple, "VariantIndex", -2);
  int def_subvariant_index = (int) FindIntValue(default_tuple, "SubVariantIndex", -2);

  if (-2 == def_config_index || -2 == def_variant_index || -2 == def_subvariant_index)
  {
    fprintf(stderr, "bad Setup.DefaultBuildTuple data\n");
    return false;
  }

  BinarySegmentWriteInt32(main_seg, def_config_index);
  BinarySegmentWriteInt32(main_seg, def_variant_index);
  BinarySegmentWriteInt32(main_seg, def_subvariant_index);

  if (const JsonArrayValue* file_sigs = FindArrayValue(root, "FileSignatures"))
  {
    size_t count = file_sigs->m_Count;
    BinarySegmentWriteInt32(main_seg, (int) count);
    BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));
    for (size_t i = 0; i < count; ++i)
    {
      if (const JsonObjectValue* sig = file_sigs->m_Values[i]->AsObject())
      {
        const char* path = FindStringValue(sig, "File");
        int64_t timestamp = FindIntValue(sig, "Timestamp", -1);
        if (!path || -1 == timestamp)
        {
          fprintf(stderr, "bad FileSignatures data\n");
          return false;
        }
        WriteStringPtr(aux_seg, str_seg, path);
        char padding[4] = { 0, 0, 0, 0 };
        BinarySegmentWrite(aux_seg, padding, 4);
        BinarySegmentWriteUint64(aux_seg, uint64_t(timestamp));
      }
      else
      {
        fprintf(stderr, "bad FileSignatures data\n");
        return false;
      }
    }
  }
  else
  {
    BinarySegmentWriteInt32(main_seg, 0);
    BinarySegmentWriteNullPointer(main_seg);
  }

  if (const JsonArrayValue* glob_sigs = FindArrayValue(root, "GlobSignatures"))
  {
    size_t count = glob_sigs->m_Count;
    BinarySegmentWriteInt32(main_seg, (int) count);
    BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));
    for (size_t i = 0; i < count; ++i)
    {
      if (const JsonObjectValue* sig = glob_sigs->m_Values[i]->AsObject())
      {
        const char* path = FindStringValue(sig, "Path");
        const JsonArrayValue* files = FindArrayValue(sig, "Files");
        const JsonArrayValue* subdirs = FindArrayValue(sig, "SubDirs");
        if (!path || !files || !subdirs)
        {
          fprintf(stderr, "bad GlobSignatures data\n");
          return false;
        }

        // Compute digest of dir query.
        std::sort(files->m_Values, files->m_Values + files->m_Count, SortJsonStrings);
        std::sort(subdirs->m_Values, subdirs->m_Values + subdirs->m_Count, SortJsonStrings);

        HashState h;
        HashInit(&h);

        for (size_t i = 0, count = subdirs->m_Count; i < count; ++i)
        {
          HashAddString(&h, subdirs->m_Values[i]->GetString());
          HashAddSeparator(&h);
        }

        for (size_t i = 0, count = files->m_Count; i < count; ++i)
        {
          HashAddString(&h, files->m_Values[i]->GetString());
          HashAddSeparator(&h);
        }

        HashDigest digest;
        HashFinalize(&h, &digest);

        WriteStringPtr(aux_seg, str_seg, path);
        BinarySegmentWrite(aux_seg, (char*) &digest, sizeof digest);
      }
    }
  }
  else
  {
    BinarySegmentWriteInt32(main_seg, 0);
    BinarySegmentWriteNullPointer(main_seg);
  }

  // Emit hashes of file extensions to sign using SHA-1 content digest instead of the normal timestamp signing.
  if (const JsonArrayValue* sha_exts = FindArrayValue(root, "ContentDigestExtensions"))
  {
    BinarySegmentWriteInt32(main_seg, (int) sha_exts->m_Count);
    BinarySegmentWritePointer(main_seg, BinarySegmentPosition(aux_seg));

    for (size_t i = 0, count = sha_exts->m_Count; i < count; ++i)
    {
      const JsonValue* v = sha_exts->m_Values[i];
      if (const JsonStringValue* sv = v->AsString())
      {
        const char* str = sv->m_String;
        if (str[0] != '.')
        {
          fprintf(stderr, "ContentDigestExtensions: Expected extension to start with dot: %s\b", str);
          return false;
        }

        BinarySegmentWriteUint32(aux_seg, Djb2Hash(str));
      }
      else
        return false;
    }
  }
  else
  {
    BinarySegmentWriteInt32(main_seg, 0);
    BinarySegmentWriteNullPointer(main_seg);
  }

  BinarySegmentWriteInt32(main_seg, (int) FindIntValue(root, "MaxExpensiveCount", -1));

  HashTableDestroy(&shared_strings);

  return true;
}

static bool CreateDagFromJsonData(char* json_memory, const char* dag_fn)
{
  MemAllocHeap heap;
  HeapInit(&heap, MB(256), HeapFlags::kDefault);

  MemAllocLinear alloc;
  MemAllocLinear scratch;

  LinearAllocInit(&alloc, &heap, MB(128), "json alloc");
  LinearAllocInit(&scratch, &heap, MB(64), "json scratch");

  char error_msg[1024];

  bool result = false;

  const JsonValue* value = JsonParse(json_memory, &alloc, &scratch, error_msg);

  if (value)
  {
    if (const JsonObjectValue* obj = value->AsObject())
    {
      BinaryWriter writer;
      BinaryWriterInit(&writer, &heap);

      result = CompileDag(obj, &writer, &heap, &scratch);

      result = result && BinaryWriterFlush(&writer, dag_fn);

      BinaryWriterDestroy(&writer);
    }
    else
    {
      Log(kError, "bad JSON structure");
    }
  }
  else
  {
    Log(kError, "failed to parse JSON: %s", error_msg);
  }

  LinearAllocDestroy(&scratch);
  LinearAllocDestroy(&alloc);

  HeapDestroy(&heap);
  return result;
}

static bool RunExternalTool(const char* options, ...)
{
  char dag_gen_path[kMaxPathLength];

  if (const char* env_option = getenv("TUNDRA_DAGTOOL"))
  {
    strncpy(dag_gen_path, env_option, sizeof dag_gen_path);
    dag_gen_path[sizeof(dag_gen_path)-1] = '\0';
  }
  else
  {
    // Figure out the path to the default t2-lua DAG generator.
    PathBuffer pbuf;
    PathInit(&pbuf, GetExePath());
    PathStripLast(&pbuf);
    PathConcat(&pbuf, "t2-lua" TUNDRA_EXE_SUFFIX);
    PathFormat(dag_gen_path, &pbuf);
  }

  char option_str[1024];
  va_list args;
  va_start(args, options);
  vsnprintf(option_str, sizeof option_str, options, args);
  va_end(args);
  option_str[sizeof(option_str)-1] = '\0';

  const char* quotes = "";
  if (strchr(dag_gen_path, ' '))
    quotes = "\"";

  char cmdline[1024];
  snprintf(cmdline, sizeof cmdline, "%s%s%s %s", quotes, dag_gen_path, quotes, option_str);
  cmdline[sizeof(cmdline)-1] = '\0';

  const bool echo = (GetLogFlags() & kDebug) ? true : false;
  ExecResult result = ExecuteProcess(cmdline, 0, nullptr, 0, echo, nullptr);

  if (0 != result.m_ReturnCode)
  {
    Log(kError, "DAG generator driver failed: %s", cmdline);
    return false;
  }

  return true;
}

bool GenerateDag(const char* script_fn, const char* dag_fn)
{
  Log(kDebug, "regenerating DAG data");

  char json_filename[kMaxPathLength];
  snprintf(json_filename, sizeof json_filename, "%s.json", dag_fn);
  json_filename[sizeof(json_filename)- 1] = '\0';

  // Nuke any old JSON data.
  remove(json_filename);

  // Run DAG generator.
  if (!RunExternalTool("generate-dag %s %s", script_fn, json_filename))
    return false;

  FileInfo json_info = GetFileInfo(json_filename);
  if (!json_info.Exists())
  {
    Log(kError, "build script didn't generate %s", json_filename);
    return false;
  }

  size_t json_size = size_t(json_info.m_Size + 1);
  char* json_memory = (char*) malloc(json_size);
  if (!json_memory)
    Croak("couldn't allocate memory for JSON buffer");

  FILE* f = fopen(json_filename, "rb");
  if (!f)
  {
    Log(kError, "couldn't open %s for reading", json_filename);
    return false;
  }

  size_t read_count = fread(json_memory, 1, json_size - 1, f);
  if (json_size - 1 != read_count)
  {
    fclose(f);
    free(json_memory);
    Log(kError, "couldn't read JSON data (%d bytes read out of %d)",
        json_filename, (int) read_count, (int) json_size);
    return false;
  }

  fclose(f);

  json_memory[json_size-1] = 0;

  bool success = CreateDagFromJsonData(json_memory, dag_fn);

  free(json_memory);

  return success;
}

bool GenerateIdeIntegrationFiles(const char* build_file, int argc, const char** argv)
{
  MemAllocHeap heap;
  HeapInit(&heap, MB(1), HeapFlags::kDefault);

  Buffer<char> args;
  BufferInit(&args);

  for (int i = 0; i < argc; ++i)
  {
    if (i > 0)
      BufferAppendOne(&args, &heap, ' ');

    const size_t arglen = strlen(argv[i]);
    const bool has_spaces = nullptr != strchr(argv[i], ' ');

    if (has_spaces)
      BufferAppendOne(&args, &heap, '"');

    BufferAppend(&args, &heap, argv[i], arglen);

    if (has_spaces)
      BufferAppendOne(&args, &heap, '"');
  }

  BufferAppendOne(&args, &heap, '\0');

  // Run DAG generator.
  bool result = RunExternalTool("generate-ide-files %s %s", build_file, args.m_Storage);

  BufferDestroy(&args, &heap);
  HeapDestroy(&heap);

  return result;
}

}
