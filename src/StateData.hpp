#ifndef STATEDATA_HPP
#define STATEDATA_HPP

#include "Common.hpp"
#include "BinaryData.hpp"

namespace t2
{

#pragma pack(push, 4)
struct NodeInputFileData
{
  uint64_t     m_Timestamp;
  FrozenString m_Filename;
};
#pragma pack(pop)

static_assert(sizeof(NodeInputFileData) == 12, "struct layout");


struct NodeStateData
{
  int32_t                        m_BuildResult;
  HashDigest                     m_InputSignature;
  FrozenArray<FrozenString>      m_OutputFiles;
  FrozenArray<FrozenString>      m_AuxOutputFiles;
  uint32_t                       m_TimeStampOfLastUseInDays;
  FrozenString                   m_Action;
  FrozenString                   m_PreAction;
  FrozenArray<NodeInputFileData> m_InputFiles;
  FrozenArray<NodeInputFileData> m_ImplicitInputFiles;
};

struct StateData
{
  static const uint32_t     MagicNumber = 0x15890104 ^ kTundraHashMagic;

  uint32_t                 m_MagicNumber;

  int32_t                  m_NodeCount;
  FrozenPtr<HashDigest>    m_NodeGuids;
  FrozenPtr<NodeStateData> m_NodeStates;

  uint32_t                   m_MagicNumberEnd;
};

}

#endif
