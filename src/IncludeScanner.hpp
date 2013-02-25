#ifndef INCLUDESCANNER_HPP
#define INCLUDESCANNER_HPP

#include "Common.hpp"

// Low-level scanning functions to grab dependencies from a file buffer.

namespace t2
{

struct GenericScannerData;
struct MemAllocLinear;

struct IncludeData
{
  const char  *m_String;
  size_t       m_StringLen;
  bool         m_IsSystemInclude;
  bool         m_ShouldFollow;
  IncludeData *m_Next;
};

// Scan C/C++ style #includes from buffer.
// Buffer must be null-terminated and will be modified in place.
IncludeData*
ScanIncludesCpp(char* buffer, MemAllocLinear* allocator);

// Scan generic includes from buffer (slower, customizable).
// Buffer must be null-terminated and will be modified in place.
IncludeData*
ScanIncludesGeneric(char* buffer, MemAllocLinear* allocator, const GenericScannerData& config);

}

#endif
