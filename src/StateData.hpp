#ifndef STATEDATA_HPP
#define STATEDATA_HPP

#include "Common.hpp"
#include "BinaryData.hpp"

namespace t2
{

struct NodeStateData
{
  int32_t                   m_BuildResult;
  HashDigest                m_InputSignature;
  FrozenArray<FrozenString> m_OutputFiles;
  FrozenArray<FrozenString> m_AuxOutputFiles;
};

struct StateData
{
  static const uint32_t     MagicNumber = 0x15890102 ^ kTundraHashMagic;

  uint32_t                 m_MagicNumber;

  int32_t                  m_NodeCount;
  FrozenPtr<HashDigest>    m_NodeGuids;
  FrozenPtr<NodeStateData> m_NodeStates;
};

}

#endif
