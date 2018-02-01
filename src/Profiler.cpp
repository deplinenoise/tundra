#include "Profiler.hpp"
#include "Common.hpp"
#include "MemAllocLinear.hpp"
#include "MemAllocHeap.hpp"

#include <stdio.h>

namespace t2
{
  const size_t kProfilerThreadMaxEvents = 32 * 1024; // max this # of events per thread
  const size_t kProfilerThreadStringsSize = kProfilerThreadMaxEvents * 128; // that many bytes of name string storage per thread

  struct ProfilerEvent
  {
    uint64_t    m_Time;
    uint64_t    m_Duration;
    const char* m_Name;
    const char* m_Info;
  };

  struct ProfilerThread
  {
    MemAllocLinear  m_ScratchStrings;
    ProfilerEvent*  m_Events;
    int             m_EventCount;
    bool            m_IsBegin;
  };

  struct ProfilerState
  {
    MemAllocHeap    m_Heap;
    char*           m_FileName;
    ProfilerThread* m_Threads;
    int             m_ThreadCount;
  };

  static ProfilerState s_ProfilerState;

  bool g_ProfilerEnabled;

  void ProfilerInit(const char* fileName, int threadCount)
  {
    CHECK(!g_ProfilerEnabled);
    CHECK(threadCount > 0);

    g_ProfilerEnabled = true;

    s_ProfilerState.m_ThreadCount = threadCount;

    HeapInit(&s_ProfilerState.m_Heap);

    size_t fileNameLen = strlen(fileName);
    s_ProfilerState.m_FileName = (char*)HeapAllocate(&s_ProfilerState.m_Heap, fileNameLen + 1);
    memcpy(s_ProfilerState.m_FileName, fileName, fileNameLen + 1);

    s_ProfilerState.m_Threads = HeapAllocateArray<ProfilerThread>(&s_ProfilerState.m_Heap, s_ProfilerState.m_ThreadCount);
    for (int i = 0; i < s_ProfilerState.m_ThreadCount; ++i)
    {
      ProfilerThread& thread = s_ProfilerState.m_Threads[i];
      thread.m_Events = HeapAllocateArray<ProfilerEvent>(&s_ProfilerState.m_Heap, kProfilerThreadMaxEvents);
      thread.m_EventCount = 0;
      LinearAllocInit(&thread.m_ScratchStrings, &s_ProfilerState.m_Heap, kProfilerThreadStringsSize, "profilerStrings");
      thread.m_IsBegin = false;
    }
  }

  static void EscapeString(const char* src, char* dst, int dstSpace)
  {
    while (*src && dstSpace > 2) // some input chars might expand into 2 in the output
    {
      char c = *src;
      switch (c)
      {
      case '"':
      case '\\':
      case '\b':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
        *dst++ = '\\'; *dst++ = c; dstSpace -= 2;
        break;
      default:
        if (c >= 32 && c < 126)
        {
          *dst++ = c; dstSpace -= 1;
        }
      }
      ++src;
    }
    *dst = 0;
  }

  void ProfilerWriteOutput()
  {
    FILE* f = fopen(s_ProfilerState.m_FileName, "w");
    if (!f)
    {
      Log(kWarning, "profiler: failed to write profiler output file into '%s'", s_ProfilerState.m_FileName);
      return;
    }

    // See https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/edit for
    // Chrome Tracing profiler format description

    fputs("{\n", f);
    // JSON does not support comments, so emit "how to use this" as a fake string value    
    fputs("\"instructions_readme\": \"1) Open Chrome, 2) go to chrome://tracing, 3) click Load, 4) navigate to this file.\",\n", f);
    fputs("\"traceEvents\":[\n", f);
    fputs("{ \"cat\":\"\", \"pid\":1, \"tid\":0, \"ts\":0, \"ph\":\"M\", \"name\":\"process_name\", \"args\": { \"name\":\"tundra\" } }\n", f);
    for (int i = 0; i < s_ProfilerState.m_ThreadCount; ++i)
    {
      const ProfilerThread& thread = s_ProfilerState.m_Threads[i];
      for (int j = 0; j < thread.m_EventCount; ++j)
      {
        const ProfilerEvent& evt = thread.m_Events[j];
        double timeUs = TimerToSeconds(evt.m_Time)*1.0e6;
        double durUs = TimerToSeconds(evt.m_Duration)*1.0e6;
        char name[1024];
        char info[1024];
        EscapeString(evt.m_Name, name, sizeof(name));
        EscapeString(evt.m_Info, info, sizeof(info));
        fprintf(f, ",{ \"pid\":1, \"tid\":%d, \"ts\":%.0f, \"dur\":%.0f, \"ph\":\"X\", \"name\": \"%s\", \"args\": { \"durationMS\":%.0f, \"detail\":\"%s\" }}\n", i, timeUs, durUs, name, durUs*0.001, info);
      }
    }
    fputs("\n]\n", f);
    fputs("}\n", f);

    fclose(f);
  }

  void ProfilerDestroy()
  {
    if (!g_ProfilerEnabled)
      return;

    for (int i = 0; i < s_ProfilerState.m_ThreadCount; ++i)
    {
      ProfilerThread& thread = s_ProfilerState.m_Threads[i];
      if (thread.m_IsBegin)
        ProfilerEndImpl(i);
    }

    ProfilerWriteOutput();

    for (int i = 0; i < s_ProfilerState.m_ThreadCount; ++i)
    {
      ProfilerThread& thread = s_ProfilerState.m_Threads[i];
      Log(kSpam, "profiler: thread %i had %d events, %.1f KB strings", i, thread.m_EventCount, double(thread.m_ScratchStrings.m_Offset)/1024.0);
      HeapFree(&s_ProfilerState.m_Heap, thread.m_Events);
      LinearAllocDestroy(&thread.m_ScratchStrings);
    }
    HeapFree(&s_ProfilerState.m_Heap, s_ProfilerState.m_Threads);
    HeapFree(&s_ProfilerState.m_Heap, s_ProfilerState.m_FileName);
    HeapDestroy(&s_ProfilerState.m_Heap);
  }

  void ProfilerBeginImpl(const char* name, int threadIndex)
  {
    CHECK(g_ProfilerEnabled);
    CHECK(threadIndex >= 0 && threadIndex < s_ProfilerState.m_ThreadCount);
    ProfilerThread& thread = s_ProfilerState.m_Threads[threadIndex];
    CHECK(!thread.m_IsBegin);
    thread.m_IsBegin = true;
    if (thread.m_EventCount >= kProfilerThreadMaxEvents)
    {
      Log(kWarning, "profiler: max events (%d) reached on thread %i, '%s' and later won't be recorded", (int)kProfilerThreadMaxEvents, threadIndex, name);
      return;
    }
    ProfilerEvent& evt = thread.m_Events[thread.m_EventCount++];
    evt.m_Time = TimerGet();

    // split input name by first space
    const char* nextWord = strchr(name, ' ');
    if (nextWord == nullptr)
    {
      evt.m_Name = StrDup(&thread.m_ScratchStrings, name);
      evt.m_Info = "";
    }
    else
    {
      evt.m_Name = StrDupN(&thread.m_ScratchStrings, name, nextWord - name);
      evt.m_Info = StrDup(&thread.m_ScratchStrings, nextWord + 1);
    }
  }

  void ProfilerEndImpl(int threadIndex)
  {
    CHECK(g_ProfilerEnabled);
    CHECK(threadIndex >= 0 && threadIndex < s_ProfilerState.m_ThreadCount);
    ProfilerThread& thread = s_ProfilerState.m_Threads[threadIndex];
    CHECK(thread.m_IsBegin);
    CHECK(thread.m_EventCount > 0);
    thread.m_IsBegin = false;
    if (thread.m_EventCount > kProfilerThreadMaxEvents)
      return;
    ProfilerEvent& evt = thread.m_Events[thread.m_EventCount-1];
    evt.m_Duration = TimerGet() - evt.m_Time;
  }
}
