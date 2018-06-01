#include "NodeResultPrinting.hpp"
#include "DagData.hpp"
#include "BuildQueue.hpp"
#include "Exec.hpp"
#include <stdio.h>
#include <sstream>
#include <ctime>
#include <math.h>
#if TUNDRA_UNIX
#include <unistd.h>
#include <stdarg.h>
#endif


namespace t2
{

static bool EmitColors = false;

static uint64_t last_progress_message_of_any_job;
static const NodeData* last_progress_message_job = nullptr;
static int total_number_node_results_printed = 0;


static bool isTerminatingChar(char c)
{
    return c >= 0x40 && c <= 0x7E;
}

static bool IsEscapeCode(char c)
{
    return c == 0x1B;
}

static char* DetectEscapeCode(char* ptr)
{
    if (!IsEscapeCode(ptr[0]))
        return ptr;
    if (ptr[1] == 0)
        return ptr;

    //there are other characters valid than [ here, but for now we'll only support stripping ones that have [, as all color sequences have that.
    if (ptr[1] != '[')
        return ptr;

    char* endOfCode = ptr+2;

    while(true) {
        char c = *endOfCode;
        if (c == 0)
            return ptr;
        if (isTerminatingChar(c))
            return endOfCode+1;
        endOfCode++;
    }
}

void StripAnsiColors(char* buffer)
{
   char* writeCursor = buffer;
   char* readCursor = buffer;
   while(*readCursor)
   {
       char* adjusted = DetectEscapeCode(readCursor);
       if (adjusted != readCursor)
       {
           readCursor = adjusted;
           continue;
       }
       *writeCursor++ = *readCursor++;
   }
    *writeCursor++ = 0;
}

void InitNodeResultPrinting()
{
  last_progress_message_of_any_job = TimerGet() - 10000;

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
      EmitColors = true;
#endif

    char* value = getenv("DOWNSTREAM_STDOUT_CONSUMER_SUPPORTS_COLOR");
    if (value == nullptr)
        return;

    if (*value == '1')
      EmitColors = true;
    if (*value == '0')
      EmitColors = false;
}


static void EnsureConsoleCanHandleColors()
{
#if TUNDRA_WIN32
  //We invoke this function before every printf that wants to emit a color, because it looks like child processes that tundra invokes
  //can and do SetConsoleMode() which affects our console. Sometimes a child process will set the consolemode to no longer have our flag
  //which makes all color output suddenly screw up.
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwMode = 0;
  if (GetConsoleMode(hOut, &dwMode))
  {
    const int ENABLE_VIRTUAL_TERMINAL_PROCESSING_impl = 0x0004;
    DWORD newMode = dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING_impl;
    if (newMode != dwMode)
      SetConsoleMode(hOut, newMode);
  }
#endif
}

static void EmitColor(const char* colorsequence)
{
  if (EmitColors)
  {
    EnsureConsoleCanHandleColors();
    printf("%s", colorsequence);
  }
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

static void PrintDiagnosticPrefix(const char* title, const char* color = YEL)
{
    EmitColor(color);
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

void PrintConcludingMessage(bool success, const char* formatString, ...)
{
    EmitColor(success ? GRN : RED);
    va_list args;
    va_start(args, formatString);
    vfprintf(stdout, formatString, args);
    va_end(args);
    EmitColor(RESET);
    printf("\n");
}

static void PrintBufferTrimmed(OutputBufferData* buffer)
{
  auto isNewLine = [](char c) {return c == 0x0A || c == 0x0D; };

  int trimmedCursor = buffer->cursor;
  while (isNewLine(*(buffer->buffer + trimmedCursor -1)) && trimmedCursor > 0)
    trimmedCursor--;

  if (EmitColors)
  {
    fwrite(buffer->buffer, 1, trimmedCursor, stdout);
  } else {
    buffer->buffer[trimmedCursor] = 0;
    StripAnsiColors(buffer->buffer);
    fwrite(buffer->buffer, 1, strlen(buffer->buffer), stdout);
  }
  printf("\n");
}


void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose, uint64_t time_exec_started, ValidationResult validationResult)
{
    int processedNodeCount = ++queue->m_ProcessedNodeCount;
    bool failed = result->m_ReturnCode != 0 || result->m_WasSignalled || validationResult == ValidationResult::Fail;
    bool verbose = (failed && !result->m_WasAborted) || always_verbose;

    int maxDigits = ceil(log10(queue->m_Config.m_MaxNodes+1)); 

    uint64_t now = TimerGet();
    int duration = TimerDiffSeconds(time_exec_started, now);
    
    EmitColor(failed ? RED : GRN);
    printf("[");
    if (failed && !EmitColors)
      printf("!FAILED! ");
    printf("%*d/%d ", maxDigits, processedNodeCount, queue->m_Config.m_MaxNodes);
    printf("%2ds] ", duration);
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
              fclose(f);
            }
            PrintDiagnostic(titleBuffer, content_buffer);
            HeapFree(queue->m_Config.m_Heap, content_buffer);
        }


        if (node_data->m_EnvVars.GetCount() > 0)
          PrintDiagnosticPrefix("Custom Environment Variables");
        for (int i=0; i!=node_data->m_EnvVars.GetCount(); i++)
        {
           auto& entry = node_data->m_EnvVars[i];
           printf("%s=%s\n", entry.m_Name.Get(), entry.m_Value.Get() );
        }
        if (validationResult == ValidationResult::Fail && result->m_ReturnCode == 0 && !result->m_WasSignalled)
        {
          PrintDiagnosticPrefix("Failed because this command wrote something to the output that wasnt expected. We were expecting any of the following strings:", RED);
          int count = node_data->m_AllowedOutputSubstrings.GetCount();
          for (int i=0; i!=count ; i++)
            printf("%s\n", (const char*)node_data->m_AllowedOutputSubstrings[i]);
          if (count == 0)
            printf("<< no allowed strings >>\n");
        }

        if (result->m_WasSignalled)
          PrintDiagnostic("Was Signaled", "Yes");
        if (result->m_WasAborted)
          PrintDiagnostic("Was Aborted", "Yes");
        if (result->m_ReturnCode !=0)
          PrintDiagnostic("ExitCode", result->m_ReturnCode);
    }

    bool anyOutput = result->m_OutputBuffer.cursor>0;

    if (anyOutput && verbose)
    {
      PrintDiagnosticPrefix("Output");
      PrintBufferTrimmed(&result->m_OutputBuffer);
    } else if (anyOutput && 0 != (validationResult != ValidationResult::SwallowStdout))
        PrintBufferTrimmed(&result->m_OutputBuffer);
    
    total_number_node_results_printed++;
    last_progress_message_of_any_job = now;
    last_progress_message_job = node_data;

    fflush(stdout);
}

int PrintNodeInProgress(const NodeData* node_data, uint64_t time_of_start, const BuildQueue* queue)
{
  uint64_t now = TimerGet();
  int seconds_job_has_been_running_for = TimerDiffSeconds(time_of_start, now);
  double seconds_since_last_progress_message_of_any_job = TimerDiffSeconds(last_progress_message_of_any_job, now);

  int acceptable_time_since_last_message = last_progress_message_job == node_data ? 10 : (total_number_node_results_printed == 0 ? 0 : 5) ;
  int only_print_if_slower_than = seconds_since_last_progress_message_of_any_job > 30 ? 0 : 5;

  if (seconds_since_last_progress_message_of_any_job > acceptable_time_since_last_message && seconds_job_has_been_running_for > only_print_if_slower_than)
  {
    int maxDigits = ceil(log10(queue->m_Config.m_MaxNodes+1));

    EmitColor(YEL);
    printf("[BUSY %*ds] ", maxDigits*2-1, seconds_job_has_been_running_for);
    EmitColor(RESET);
    printf("%s\n", (const char*)node_data->m_Annotation);
    last_progress_message_of_any_job = now;
    last_progress_message_job = node_data;

    fflush(stdout);
  }

  return 1;
}
}