#include "IncludeScanner.hpp"

#include <cstddef>
#include <cctype>
#include <cstring>
#include <stdint.h>

#include "MemAllocLinear.hpp"
#include "DagData.hpp"

namespace t2
{

static IncludeData*
ScanCppLine(const char* start, MemAllocLinear* allocator)
{
	while (isspace(*start))
		++start;

	if (*start++ != '#')
		return nullptr;

	while (isspace(*start))
		++start;
	
	if (0 != strncmp("include", start, 7))
		return nullptr;

	start += 7;

	if (!isspace(*start++))
		return nullptr;

	while (isspace(*start))
		++start;

  IncludeData* dest = LinearAllocate<IncludeData>(allocator);

  char closing_separator;

  switch (*start++)
  {
    case '<':
      closing_separator = '>';
      break;
    case '"':
      closing_separator = '"';
      break;
    default:
      return nullptr;
  }

	const char* str_start = start;
	for (;;)
	{
		char ch = *start++;
		if (ch == closing_separator)
			break;
		if (!ch)
			return nullptr;
	}

	dest->m_StringLen       = (size_t) (start - str_start - 1);
	dest->m_String          = StrDupN(allocator, str_start, dest->m_StringLen);
	dest->m_IsSystemInclude = '>' == closing_separator;
	dest->m_ShouldFollow    = true;
	dest->m_Next            = nullptr;

  return dest;
}

static char*
GetNextLine(char *p)
{
  if (char *lf = strchr(p, '\n'))
  {
    *lf = '\0';
    return lf + 1;
  }
  else
  {
    return nullptr;
  }
}

// Helper to maintain a linked list head + curr pointer to build linked list in
// natural order by appending to last item.
struct IncludeDataList
{
  IncludeData* m_Head;
  IncludeData* m_Curr;

  IncludeDataList()
  : m_Head(nullptr)
  , m_Curr(nullptr)
  {}

  void Add(IncludeData* d)
  {
    if (!m_Head)
    {
      m_Head = d;
      m_Curr = d;
    }
    else
    {
      m_Curr->m_Next = d;
      m_Curr = d;
    }
  }
};

IncludeData*
ScanIncludesCpp(char* buffer, MemAllocLinear* allocator)
{
  IncludeDataList list;

  char *linep = buffer;

  while (linep)
  {
    char *line = linep;
    linep = GetNextLine(linep);

    if (IncludeData* d = ScanCppLine(line, allocator))
    {
      list.Add(d);
    }
  }

  return list.m_Head;
}

static IncludeData*
ScanLineGeneric(MemAllocLinear* allocator, const char *start_in, const GenericScannerData& config)
{
	const char *start = start_in;
	const char *str_start;

  const bool require_ws     = 0 != (config.m_Flags & GenericScannerData::kFlagRequireWhitespace);
  const bool use_separators = 0 != (config.m_Flags & GenericScannerData::kFlagUseSeparators);
  const bool bare_is_system = 0 != (config.m_Flags & GenericScannerData::kFlagBareMeansSystem);

	while (isspace(*start))
		++start;

	if (require_ws && start == start_in)
		return nullptr;

  const KeywordData* keyword = nullptr;

  for (const KeywordData& kwdata : config.m_Keywords)
  {
    if (0 == strncmp(kwdata.m_String, start, kwdata.m_StringLength))
    {
      keyword = &kwdata;
      break;
		}
	}

	if (!keyword)
		return nullptr;

	start += keyword->m_StringLength;
	
  // TDDO: Should make this optional
	if (!isspace(*start++))
		return nullptr;

	while (isspace(*start))
		++start;

  IncludeData* dest = LinearAllocate<IncludeData>(allocator);

	if (use_separators)
	{
    char closing_separator;

    switch (*start++)
    {
      case '<':
        closing_separator = '>';
        break;
      case '>': // A really crude way to match <file>path...</file> in QRC files.
        closing_separator = '<';
        break;
      case '"':
        closing_separator = '"';
        break;
      default:
        return nullptr;
    }

		str_start = start;
		for (;;)
		{
			char ch = *start++;
			if (ch == closing_separator)
				break;
			if (!ch)
				return 0;
		}

    // start is pointing to the character after the closing separator, so wind it back one
    start--;

    dest->m_IsSystemInclude = '>' == closing_separator;
	}
	else
	{
		str_start = start;

		// just grab the next token 
		while (*start && !isspace(*start))
			++start;

		if (str_start == start)
			return 0;

    dest->m_IsSystemInclude = bare_is_system;
	}

	dest->m_StringLen    = (unsigned short) (start - str_start);
	dest->m_String       = StrDupN(allocator, str_start, dest->m_StringLen);
	dest->m_ShouldFollow = keyword->m_ShouldFollow ? true : false;
	dest->m_Next         = nullptr;
	return dest;
}

IncludeData* ScanIncludesGeneric(char* buffer, MemAllocLinear* allocator, const GenericScannerData& config)
{
  char* linep = buffer;
  IncludeDataList includes;

  while (linep)
  {
    char *line_data = linep;
    linep = GetNextLine(linep);

    if (IncludeData* d = ScanLineGeneric(allocator, line_data, config))
    {
      includes.Add(d);
    }
  }

  return includes.m_Head;
}

}
