#include "PathUtil.hpp"

#include <string.h>
#include <ctype.h>

namespace t2
{

#if 0
static const char ascii_letters[] = 
"abcdefghijklmnopqrstuvwxyz"
"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static bool IsAbsolutePath(PathStyle style, const char* path)
{
  switch (style)
  {
    case PathStyle::kWindows:
      // Windows-style device-relative or network share path
      if (path[0] == '\\')
        return true;

      // Windows-style device path
      if (strchr(ascii_letters, path[0]) && path[1] == ':' && path[2] == '\\')
        return true;

      return false;

    case PathStyle::kUnix:
      // Unix style absolute path
      if (path[0] == '/')
        return true;

    default:
      Croak("bad path style");
  }
}

static char** TokenizePath(
    MemAllocLinear& alloc,
    PathStyle style,
    char (&root)[kDeviceMax],
    const char* string)
{
  const size_t len = strlen(string);

  if (

}

Path CreatePath(MemAllocLinear& alloc, PathStyle style, const char* string)
{
  // Tokenize string into path tokens
  const bool is_absolute = IsAbsolutePath(style, string);

  char root[kDeviceMax];
  char** tokens = nullptr;
  tokens = TokenizePath(alloc, style, root, string);

}

char* SimpifyPath(MemAllocLinear& alloc, const char* path)
{
  char root_prefix[64];

}

char* CombinePaths(MemAllocLinear& alloc, const char* base, const char* end);
#endif

template <typename T>
static int BufferSize(const T* path, int seg_count)
{
  CHECK(uint16_t(seg_count) <= path->m_SegCount);

  if (seg_count > 0)
  {
    return path->m_SegEnds[seg_count - 1];
  }
  else
  {
    return 0;
  }
} 


struct PathSeg
{
	uint16_t  offset;
	uint16_t  len;
	uint8_t   dotdot;
	uint8_t   drop;
};

static int
PathGetSegments(const char* scratch, PathSeg segments[kMaxPathSegments])
{
  const char *start    = scratch;
  int         segcount = 0;
  const char *last     = scratch;

	for (;;)
	{
		char ch = *scratch;

		if ('\\' == ch || '/' == ch || '\0' == ch)
		{
			int len = (int) (scratch - last);

      if (len > 0)
      {
        int is_dotdot = 2 == len && 0 == memcmp("..", last, 2);
        int is_dot = 1 == len && '.' == last[0];

        if (segcount == kMaxPathSegments)
          Croak("too many segments in path; limit is %d", kMaxPathSegments);

        segments[segcount].offset = (uint16_t) (last - start);
        segments[segcount].len    = (uint16_t) len;
        segments[segcount].dotdot = (uint8_t) is_dotdot;
        segments[segcount].drop   = (uint8_t) is_dot;

        ++segcount;
      }
      last = scratch + 1;

			if ('\0' == ch)
				break;
		}

		++scratch;
	}

	return segcount;
}

void PathInit(PathBuffer* buffer, const char* path, PathType::Enum type)
{
  // Initialize
  buffer->m_Type  = type;
  buffer->m_Flags = 0;

  // Check to see if the path is absolute
  switch (type)
  {
    case PathType::kUnix:
      if ('/' == path[0])
      {
        buffer->m_Flags |= PathBuffer::kFlagAbsolute;
        path++;
      }
      break;

    case PathType::kWindows:
      // Check for absolute path w/o device name
      if ('\\' == path[0] || '/' == path[0])
      {
        buffer->m_Flags |= PathBuffer::kFlagAbsolute;
        path++;
      }
      // Check for X:\ style path
      else if (isalpha(path[0]) && ':' == path[1] && ('\\' == path[2] || '/' == path[2]))
      {
        buffer->m_Flags |= PathBuffer::kFlagAbsolute | PathBuffer::kFlagWindowsDevicePath;
      }
      // FIXME: network paths
      break;

    default:
      Croak("bad path type");
      break;
  }

  // Initialize segment data
  PathSeg segments[kMaxPathSegments];

  int raw_seg_count = PathGetSegments(path, segments);
  uint16_t dotdot_drops = 0;

  // Drop segments based on .. following them
  for (int i = raw_seg_count - 1; i >= 0; --i)
  {
    if (segments[i].drop)
      continue;

    if (segments[i].dotdot)
    {
      ++dotdot_drops;
      segments[i].drop = 1;
    }
    else if (dotdot_drops > 0)
    {
      --dotdot_drops;
      segments[i].drop = 1;
    }
  }

  buffer->m_LeadingDotDots = dotdot_drops;

  // Copy retained segments to output array
  uint16_t output_seg_count = 0;
  uint16_t output_pos = 0;
  for (int i = 0; i < raw_seg_count; ++i)
  {
    if (segments[i].drop)
      continue;

    if (output_pos >= kMaxPathLength)
      Croak("Path too long: %s", path);

    memcpy(buffer->m_Data + output_pos, path + segments[i].offset, segments[i].len);
    output_pos += segments[i].len;
    buffer->m_SegEnds[output_seg_count] = output_pos;
    ++output_seg_count;
  }

  buffer->m_SegCount = output_seg_count;
}

bool PathStripLast(PathBuffer* buffer)
{
  uint16_t min_seg_count = (buffer->m_Flags & PathBuffer::kFlagWindowsDevicePath) ? 1 : 0;
  uint16_t seg_count = buffer->m_SegCount;
  if (seg_count > min_seg_count)
  {
    buffer->m_SegCount = seg_count - 1;
    return true;
  }
  else
    return false;
}

template <typename T>
static void PathConcatImpl(PathBuffer* buffer, const T* other)
{
  CHECK(!PathIsAbsolute(other));

  int seg_count = buffer->m_SegCount;

  int min_seg_count = (buffer->m_Flags & PathBuffer::kFlagWindowsDevicePath) ? 1 : 0;

  // Start by throwing away .. from the other path
  for (int i = 0, count = other->m_LeadingDotDots; i < count; ++i)
  {
    if (seg_count > min_seg_count)
    {
      seg_count--;
    }
    else if (seg_count == 0)
    {
      buffer->m_LeadingDotDots++;
    }
  }

  // Can't go higher than root directory. Just clamp to there.
  if (PathIsAbsolute(buffer))
  {
    buffer->m_LeadingDotDots = 0;
  }

  // Compute how much of our data buffer we need to keep
  int keep_buffer = BufferSize(buffer, seg_count);
  // Compute size of the other buffer.
  int other_buffer = BufferSize(other, other->m_SegCount);

  CHECK(seg_count + other->m_SegCount <= kMaxPathSegments);
  CHECK(keep_buffer + other_buffer <= kMaxPathLength);

  memcpy(buffer->m_Data + keep_buffer, other->m_Data, other_buffer);
  for (int i = 0, count = other->m_SegCount; i < count; ++i)
  {
    buffer->m_SegEnds[seg_count + i] = uint16_t(other->m_SegEnds[i] + keep_buffer);
  }
  
  buffer->m_SegCount = uint16_t(seg_count + other->m_SegCount);
}

void PathConcat(PathBuffer* buffer, const char* other)
{
  PathBuffer buf;
  PathInit(&buf, other);
  PathConcat(buffer, &buf);
}

void PathConcat(PathBuffer* buffer, const PathBuffer* other)
{
  if (PathIsAbsolute(other))
  {
    *buffer = *other;
  }
  else
  {
    PathConcatImpl(buffer, other);
  }
}

void PathFormat(char (&output)[kMaxPathLength], const PathBuffer* buffer)
{
  PathFormatPartial(output, buffer, 0, buffer->m_SegCount - 1);
}

void PathFormatPartial(char (&output)[kMaxPathLength], const PathBuffer* buffer, int start_seg, int end_seg)
{
  char *cursor = &output[0];
  char pathsep = PathType::kWindows == buffer->m_Type ? '\\' : '/';

  if (start_seg == 0 &&
      PathBuffer::kFlagAbsolute ==
      (buffer->m_Flags & (PathBuffer::kFlagAbsolute|PathBuffer::kFlagWindowsDevicePath)))
  {
    *cursor++ = pathsep;
  }

  // Emit all leading ".." tokens we've got left
  for (int i = 0, count = buffer->m_LeadingDotDots; i < count; ++i)
  {
    *cursor++ = '.';
    *cursor++ = '.';
    *cursor++ = pathsep;
  }

  // Emit all remaining tokens.
  uint16_t off = 0;

  for (int i = 0; i < start_seg; ++i)
  {
    off += buffer->SegLength(i);
  }

  for (int i = start_seg; i <= end_seg; ++i)
  {
    uint16_t len = buffer->SegLength(i);

    if ((cursor - &output[0]) + len + 1 >= kMaxPathLength)
      Croak("Path too long");

    if (i > start_seg)
      *cursor++ = pathsep;

    memcpy(cursor, buffer->m_Data + off, len);
    cursor += len;
    off += len;
  }
  *cursor = 0;
}

}


