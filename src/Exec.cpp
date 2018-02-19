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
    DestroyOutputBuffer(&result->m_StdOutBuffer);
    DestroyOutputBuffer(&result->m_StdErrBuffer);
}

}