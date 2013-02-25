#ifndef TARGETSELECT_HPP
#define TARGETSELECT_HPP

#include "Buffer.hpp"

namespace t2
{
struct MemAllocHeap;

struct TargetSpec
{
  int  m_ConfigIndex;
  int  m_VariantIndex;
  int  m_SubVariantIndex;
};

inline bool operator==(const TargetSpec& l, const TargetSpec& r)
{
  return
    l.m_ConfigIndex == r.m_ConfigIndex &&
    l.m_VariantIndex == r.m_VariantIndex &&
    l.m_SubVariantIndex == r.m_SubVariantIndex;
}

inline bool operator<(const TargetSpec& l, const TargetSpec& r)
{
  if (l.m_ConfigIndex < r.m_ConfigIndex)
    return true;
  if (l.m_ConfigIndex > r.m_ConfigIndex)
    return false;
  if (l.m_VariantIndex < r.m_VariantIndex)
    return true;
  if (l.m_VariantIndex > r.m_VariantIndex)
    return false;

  return l.m_SubVariantIndex < r.m_SubVariantIndex;
}

struct TargetSelectInput
{
  int               m_ConfigCount;
  int               m_VariantCount;
  int               m_SubVariantCount;

  const uint32_t*   m_ConfigNameHashes;
  const uint32_t*   m_VariantNameHashes;
  const uint32_t*   m_SubVariantNameHashes;

  int               m_InputNameCount;
  const char**      m_InputNames;

  int               m_DefaultConfigIndex;
  int               m_DefaultVariantIndex;
  int               m_DefaultSubVariantIndex;
};

void SelectTargets(const TargetSelectInput& input, MemAllocHeap* heap, Buffer<TargetSpec>* output, Buffer<const char*>* target_names);

}

#endif