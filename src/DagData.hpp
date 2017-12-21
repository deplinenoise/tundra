#ifndef DAGDATA_HPP
#define DAGDATA_HPP

#include "Common.hpp"
#include "BinaryData.hpp"
#include "Hash.hpp"
#include "PathUtil.hpp"

namespace t2
{

namespace ScannerType
{
  enum Enum
  {
    kCpp     = 0,
    kGeneric = 1
  };
}

struct ScannerData
{
  FrozenEnum<ScannerType::Enum, int32_t>  m_ScannerType;
  FrozenArray<FrozenString>               m_IncludePaths;
  HashDigest                              m_ScannerGuid;
};

struct KeywordData
{
  FrozenString m_String;
  int16_t      m_StringLength;
  int8_t       m_ShouldFollow;
  int8_t       m_Padding;
};

struct GenericScannerData : ScannerData
{
  enum
  {
    kFlagRequireWhitespace      = 1 << 0,
    kFlagUseSeparators          = 1 << 1,
    kFlagBareMeansSystem        = 1 << 2
  };

  uint32_t                 m_Flags;
  FrozenArray<KeywordData> m_Keywords;
};

struct NamedNodeData
{
  FrozenString m_Name;
  int32_t      m_NodeIndex;
};

struct BuildTupleData
{
  int32_t                    m_ConfigIndex;
  int32_t                    m_VariantIndex;
  int32_t                    m_SubVariantIndex;
  FrozenArray<int32_t>       m_DefaultNodes;
  FrozenArray<int32_t>       m_AlwaysNodes;
  FrozenArray<NamedNodeData> m_NamedNodes;
};

struct DagFileSignature
{
  FrozenString      m_Path;
  uint8_t           m_Padding[4];
  uint64_t          m_Timestamp;
};
static_assert(offsetof(DagFileSignature, m_Timestamp) == 8, "struct layout");
static_assert(sizeof(DagFileSignature) == 16, "struct layout");

struct DagGlobSignature
{
  FrozenString      m_Path;
  HashDigest        m_Digest;
};
static_assert(sizeof(HashDigest) + sizeof(FrozenString) == sizeof(DagGlobSignature), "struct layout");

struct EnvVarData
{
  FrozenString m_Name;
  FrozenString m_Value;
};

struct NodeData
{
  enum
  {
    // Set in m_Flags if it is safe to overwrite the output files in place.  If
    // this flag is not present, the build system will remove the output files
    // before running the action. This is useful to prevent tools that
    // sometimes misbehave in the presence of old output files. ar is a good
    // example.
    kFlagOverwriteOutputs   = 1 << 0,

    // Keep output files even if the build fails. Useful mostly to retain files
    // for incremental linking.
    kFlagPreciousOutputs    = 1 << 1,

    kFlagExpensive          = 1 << 2
  };

  FrozenString                    m_Action;
  FrozenString                    m_PreAction;
  FrozenString                    m_Annotation;
  int32_t                         m_PassIndex;
  FrozenArray<int32_t>            m_Dependencies;
  FrozenArray<int32_t>            m_BackLinks;
  FrozenArray<FrozenFileAndHash>  m_InputFiles;
  FrozenArray<FrozenFileAndHash>  m_OutputFiles;
  FrozenArray<FrozenFileAndHash>  m_AuxOutputFiles;
  FrozenArray<EnvVarData>         m_EnvVars;
  FrozenPtr<ScannerData>          m_Scanner;
  uint32_t                        m_Flags;
};

struct PassData
{
  FrozenString m_PassName;
};

struct DagData
{
  static const uint32_t         MagicNumber   = 0x1589010d ^ kTundraHashMagic;

  uint32_t                      m_MagicNumber;

  int32_t                       m_NodeCount;
  FrozenPtr<HashDigest>         m_NodeGuids;
  FrozenPtr<NodeData>           m_NodeData;

  FrozenArray<PassData>         m_Passes;

  int32_t                       m_ConfigCount;
  FrozenPtr<FrozenString>       m_ConfigNames;
  FrozenPtr<uint32_t>           m_ConfigNameHashes;

  int32_t                       m_VariantCount;
  FrozenPtr<FrozenString>       m_VariantNames;
  FrozenPtr<uint32_t>           m_VariantNameHashes;

  int32_t                       m_SubVariantCount;
  FrozenPtr<FrozenString>       m_SubVariantNames;
  FrozenPtr<uint32_t>           m_SubVariantNameHashes;

  FrozenArray<BuildTupleData>   m_BuildTuples;

  int32_t                       m_DefaultConfigIndex;
  int32_t                       m_DefaultVariantIndex;
  int32_t                       m_DefaultSubVariantIndex;

  FrozenArray<DagFileSignature> m_FileSignatures;
  FrozenArray<DagGlobSignature> m_GlobSignatures;
  
  // Hashes of filename extensions to use SHA-1 digest signing instead of timestamp signing.
  FrozenArray<uint32_t>         m_ShaExtensionHashes;

  int32_t                       m_MaxExpensiveCount;
};

}

#endif
