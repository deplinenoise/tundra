#ifndef NODESTATE_HPP
#define NODESTATE_HPP

#include "Common.hpp"
#include "Hash.hpp"

namespace t2
{

namespace BuildProgress
{
  enum Enum
  {
    kInitial         = 0,
    kBlocked         = 1,
    kUnblocked       = 2,
    kRunAction       = 3,
    kSucceeded       = 100,
    kUpToDate        = 101,
    kFailed          = 102,
    kCompleted       = 200 
  };
}

namespace NodeStateFlags
{
  static const uint16_t kQueued = 1 << 0;
  static const uint16_t kActive = 1 << 1;
}

struct NodeData;
struct NodeStateData;

struct NodeState
{
  uint16_t                  m_Flags;
  uint16_t                  m_PassIndex;
  BuildProgress::Enum       m_Progress;

  const NodeData*           m_MmapData;
  const NodeStateData*      m_MmapState;

  int32_t                   m_FailedDependencyCount;
  int32_t                   m_BuildResult;

  int32_t                   m_ImplicitDepCount;
  const char**              m_ImplicitDeps;

  HashDigest                m_InputSignature;
};

inline bool NodeStateIsCompleted(const NodeState* state)
{
  return state->m_Progress == BuildProgress::kCompleted;
}

inline bool NodeStateIsQueued(const NodeState* state)
{
  return 0 != (state->m_Flags & NodeStateFlags::kQueued);
}

inline void NodeStateFlagQueued(NodeState* state)
{
  state->m_Flags |= NodeStateFlags::kQueued;
}

inline void NodeStateFlagUnqueued(NodeState* state)
{
  state->m_Flags &= ~NodeStateFlags::kQueued;
}

inline bool NodeStateIsActive(const NodeState* state)
{
  return 0 != (state->m_Flags & NodeStateFlags::kActive);
}

inline void NodeStateFlagActive(NodeState* state)
{
  state->m_Flags |= NodeStateFlags::kActive;
}

inline void NodeStateFlagInactive(NodeState* state)
{
  state->m_Flags &= ~NodeStateFlags::kActive;
}


inline bool NodeStateIsBlocked(const NodeState* state)
{
  return BuildProgress::kBlocked == state->m_Progress;
}

}

#endif
