#ifndef PROFILER_HPP
#define PROFILER_HPP

namespace t2
{
  extern bool g_ProfilerEnabled;

  // Simple non-hierarchical profiler; that dumps output into Google Chrome Tracing
  // JSON format.
  //
  // Begin/End a profiling event with ProfilerBegin and ProfilerEnd, or automatically
  // with ProfilerScope struct. Events can not nest (profiler is not hierarchical).
  // String passed into the event will be copied, and split into "name" and "detail" parts on first
  // space character.

  void ProfilerInit(const char* fileName, int threadCount);
  void ProfilerDestroy();

  void ProfilerBeginImpl(const char* name, int threadIndex);
  void ProfilerEndImpl(int threadIndex);

  inline void ProfilerBegin(const char* name, int threadIndex)
  {
    if (g_ProfilerEnabled)
      ProfilerBeginImpl(name, threadIndex);
  }

  inline void ProfilerEnd(int threadIndex)
  {
    if (g_ProfilerEnabled)
      ProfilerEndImpl(threadIndex);
  }

  struct ProfilerScope
  {
    int m_ThreadIndex;
    ProfilerScope(const char* name, int threadIndex)
    {
      m_ThreadIndex = threadIndex;
      ProfilerBegin(name, threadIndex);
    }

    ~ProfilerScope()
    {
      ProfilerEnd(m_ThreadIndex);
    }
  };

}

#endif
