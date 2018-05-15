#include "Exec.hpp"
#include "MemAllocHeap.hpp"
#include <stdio.h>
#include "BuildQueue.hpp"
#include "DagData.hpp"
#include "Atomic.hpp"

namespace t2 {
void InitOutputBuffer(OutputBufferData* data, MemAllocHeap* heap)
{
  const int initial_buffer_size = 10*1024;
  data->buffer_size = initial_buffer_size;
  data->cursor = 0;
  data->buffer = static_cast<char*>(HeapAllocate(heap, initial_buffer_size));
  data->buffer[0] = 0;
  data->heap = heap;
}

void DestroyOutputBuffer(t2::OutputBufferData* data)
{
  if (data->buffer == nullptr)
    return;
  HeapFree(data->heap, data->buffer);
  data->buffer_size = -1;
  data->cursor = 0;
  data->heap = nullptr;
  data->buffer = nullptr;
}

void ExecResultFreeMemory(ExecResult* result)
{
    DestroyOutputBuffer(&result->m_OutputBuffer);
}


void EmitOutputBytesToDestination(ExecResult* execResult, const char* text, size_t count)
{
	OutputBufferData* data = &execResult->m_OutputBuffer;

	if (data->buffer == nullptr)
	{
		//if there's no buffer to buffer into, we'll output straight to stdout.
		fwrite(text, sizeof(char), count, stdout);
		return;
	}

	if (data->cursor + count > data->buffer_size)
	{
		int newSize = data->buffer_size * 2;
		if (newSize < data->cursor + count)
			newSize = data->cursor+count;
		char* newBuffer = static_cast<char*>(HeapReallocate(data->heap, static_cast<void*>(data->buffer), newSize));
		if (newBuffer == nullptr)
		{
			CroakAbort("out of memory allocating %d bytes for output buffer", newSize);
			return;
		}
		data->buffer = newBuffer;
		data->buffer_size = newSize;
	}

	memcpy(data->buffer+data->cursor, text, count);
	data->cursor += count;
}

}