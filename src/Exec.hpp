#ifndef EXEC_HPP
#define EXEC_HPP

#include "stddef.h"
#include <thread>

namespace t2
{
  struct NodeData;
  struct MemAllocHeap;
  struct BuildQueue;

  struct EnvVariable
  {
    const char *m_Name;
    const char *m_Value;
  };

  struct OutputBufferData
  {
     char* buffer;
     size_t buffer_size;
     int cursor;
     MemAllocHeap* heap;
  };

  struct ExecResult
  {
    int               m_ReturnCode;
    bool              m_WasSignalled;
    bool              m_WasAborted;
    NodeData*         m_FrozenNodeData;
    OutputBufferData  m_OutputBuffer;
  };

  void InitOutputBuffer(OutputBufferData* data, MemAllocHeap* heap);
  void ExecResultFreeMemory(ExecResult* result);
  void ExecInit();
  void EmitOutputBytesToDestination(ExecResult* execResult, const char* text, size_t count);

  ExecResult ExecuteProcess(
        const char*         cmd_line,
        int                 env_count,
        const EnvVariable*  env_vars,
        MemAllocHeap*       heap,
        int                 job_id,
        bool                stream_output_to_stdout,
        int (*callback_on_slow)(void* user_data) = nullptr,
        void*               callback_on_slow_userdata = nullptr,
        int                 time_until_first_callback = 1
        );

}

#endif
