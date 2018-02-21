#include "NodeResultPrinting.hpp"
#include "DagData.hpp"
#include "BuildQueue.hpp"
#include "Exec.hpp"
#include <stdio.h>
#include <sstream>
#include <ctime>
#if TUNDRA_UNIX
#include <unistd.h>
#include <stdarg.h>
#endif


namespace t2
{

static bool EmitColors = false;


static time_t last_progress_message_of_any_job;
static const NodeData* last_progress_message_job = nullptr;
static int total_number_node_results_printed = 0;

void InitNodeResultPrinting()
{
  last_progress_message_of_any_job = time(0) - 100;

#if TUNDRA_UNIX
    if (isatty(fileno(stdout)))
    {
        EmitColors = true;
        return;
    }
#endif

#if TUNDRA_WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode))
    {
      static int ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;
      dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      SetConsoleMode(hOut, dwMode);
      EmitColors = true;
    }
#endif

    char* value = getenv("DOWNSTREAM_STDOUT_CONSUMER_SUPPORTS_COLOR");
    if (value == nullptr)
        return;

    if (*value == '1')
      EmitColors = true;
    if (*value == '0')
      EmitColors = false;
}

static void EmitColor(const char* colorsequence)
{
    if (EmitColors)
        printf("%s",colorsequence);
}

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define GRAY   "\x0B[37m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

static void PrintDiagnosticPrefix(const char* title)
{
    EmitColor(YEL);
    printf("##### %s\n",title);
    EmitColor(RESET);
}

static void PrintDiagnosticFormat(const char* title, const char* formatString, ...)
{
    PrintDiagnosticPrefix(title);
    va_list args;
    va_start(args, formatString);
    vfprintf(stdout, formatString, args);
    va_end(args);
    printf("\n");
}

static void PrintDiagnostic(const char* title, const char* contents)
{
    PrintDiagnosticFormat(title, "%s", contents);
}

static void PrintDiagnostic(const char* title, int content)
{
    PrintDiagnosticFormat(title, "%d", content);
}

static void PrintBufferTrimmed(OutputBufferData* buffer)
{
  auto isNewLine = [](char c) {return c == '\n' || c == '\r'; };

  int trimmedCursor = buffer->cursor;
  while (isNewLine(*(buffer->buffer + trimmedCursor -1)) && trimmedCursor > 0)
    trimmedCursor--;
  fwrite(buffer->buffer, 1, trimmedCursor +1, stdout);
  printf("\n");
}

void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose, time_t time_exec_started)
{
    int processedNodeCount = ++queue->m_ProcessedNodeCount;
    bool failed = result->m_ReturnCode != 0;
    bool verbose = failed || always_verbose;

    time_t now = time(0);
    int duration = int(now - time_exec_started);
    EmitColor(failed ? RED : GRN);
    printf("[%d/%d %ds] ", processedNodeCount, queue->m_Config.m_MaxNodes, duration);
    EmitColor(RESET); 
    printf("%s\n", (const char*)node_data->m_Annotation);   
    if (verbose)
    {
        PrintDiagnostic("CommandLine", cmd_line);
        for (int i=0; i!= node_data->m_FrontendResponseFiles.GetCount(); i++)
        {
            char titleBuffer[1024];
            const char* file = node_data->m_FrontendResponseFiles[i].m_Filename;
            snprintf(titleBuffer, sizeof titleBuffer, "Contents of %s", file);

            char* content_buffer;
            FILE* f = fopen(file, "rb");
            if (!f)
            {
              int buffersize = 512;
              content_buffer = (char*)HeapAllocate(queue->m_Config.m_Heap, buffersize);
              snprintf(content_buffer, buffersize, "couldn't open %s for reading", file);
            } else {
              fseek(f, 0L, SEEK_END);
              size_t size = ftell(f);
              rewind(f);
              size_t buffer_size = size + 1;
              content_buffer = (char*)HeapAllocate(queue->m_Config.m_Heap, buffer_size);
              
              size_t read = fread(content_buffer, 1, size, f);
              content_buffer[read] = '\0';
            }
            PrintDiagnostic(titleBuffer, content_buffer);
            HeapFree(queue->m_Config.m_Heap, content_buffer);
        }
        if (result->m_WasSignalled)
          PrintDiagnostic("Was Interrupted", "Yes");
        else
          PrintDiagnostic("ExitCode", result->m_ReturnCode);
    }

    bool anyStdOut = result->m_StdOutBuffer.cursor>0;
    bool anyStdErr = result->m_StdErrBuffer.cursor>0;
  
    if (anyStdOut && verbose)
        PrintDiagnosticPrefix("stdout");
    if (anyStdOut)
      PrintBufferTrimmed(&result->m_StdOutBuffer);

    if (anyStdErr && verbose)
        PrintDiagnosticPrefix("stderr");
    if (anyStdErr)
      PrintBufferTrimmed(&result->m_StdErrBuffer);
    
    total_number_node_results_printed++;
    last_progress_message_of_any_job = now;
}

int PrintNodeInProgress(const NodeData* node_data, time_t time_of_start)
{
  time_t now = time(0);
  int seconds_job_has_been_running_for = int(now - time_of_start);
  time_t seconds_since_last_progress_message_of_any_job = int(now - last_progress_message_of_any_job);

  int acceptable_time_since_last_message = last_progress_message_job == node_data ? 10 : (total_number_node_results_printed == 0 ? 0 : 5) ;
  int only_print_if_slower_than = seconds_since_last_progress_message_of_any_job > 30 ? 0 : 5;

  if (seconds_since_last_progress_message_of_any_job > acceptable_time_since_last_message && seconds_job_has_been_running_for > only_print_if_slower_than)
  {
    printf(YEL "[BUSY %ds] " RESET "%s\n", seconds_job_has_been_running_for, (const char*)node_data->m_Annotation);
    last_progress_message_of_any_job = now;
    last_progress_message_job = node_data;
  }

  return 1000;
}
}