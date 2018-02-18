#include "NodeResultPrinting.hpp"
#include "DagData.hpp"
#include "BuildQueue.hpp"
#include "Exec.hpp"
#include <stdio.h>
#include <sstream>
 #include <unistd.h>

namespace t2
{

static bool EmitColors = false;

void InitNodeResultPrinting()
{
    if (isatty(fileno(stdout)))
    {
        EmitColors = true;
        return;
    }

    char* value = getenv("DOWNSTREAM_STDOUT_CONSUMER_SUPPORTS_COLOR");
    if (value == nullptr)
        return;

    if (*value == '1')
        EmitColors = true;
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

void PrintNodeResult(ExecResult* result, const NodeData* node_data, const char* cmd_line, BuildQueue* queue, bool always_verbose)
{
    int processedNodeCount = ++queue->m_ProcessedNodeCount;
    bool failed = result->m_ReturnCode != 0;
    bool verbose = failed || always_verbose;
    EmitColor(failed ? RED : GRN);
    printf("[%d/%d] ", processedNodeCount, queue->m_Config.m_MaxNodes);
    EmitColor(RESET); 
    printf("%s\n", (const char*)node_data->m_Annotation);   
    if (verbose)
    {
        PrintDiagnostic("CommandLine", cmd_line);
        PrintDiagnostic("ExitCode", result->m_ReturnCode);
    }

    bool anyStdOut = result->m_StdOutBuffer.cursor>0;
    bool anyStdErr = result->m_StdErrBuffer.cursor>0;
    
    if (anyStdOut && verbose)
        PrintDiagnosticPrefix("stdout");
    if (anyStdOut)
    {
        fwrite(result->m_StdOutBuffer.buffer, 1, result->m_StdOutBuffer.cursor, stdout);
        printf("\n");
    }
    if (anyStdErr && verbose)
        PrintDiagnosticPrefix("stderr");
    if (anyStdErr)
    {
        fwrite(result->m_StdErrBuffer.buffer, 1, result->m_StdErrBuffer.cursor, stdout);
        printf("\n");
    }
}
}