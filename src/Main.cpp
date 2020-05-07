#include "Driver.hpp"
#include "Common.hpp"
#include "Stats.hpp"
#include "Exec.hpp"
#include "SignalHandler.hpp"
#include "DagGenerator.hpp"
#include "Profiler.hpp"

#include <stdio.h>
#include <stdlib.h>

#ifdef TUNDRA_WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef HAVE_GIT_INFO
extern "C" char g_GitVersion[];
extern "C" char g_GitBranch[];
#endif

namespace OptionType
{
  enum Enum
  {
    kBool,
    kInt,
    kString
  };
}

static const struct OptionTemplate
{
  char              m_ShortName;
  const char       *m_LongName;
  OptionType::Enum  m_Type;
  size_t            m_Offset;
  const char       *m_Help;
} g_OptionTemplates[] = {
  { 'j', "threads", OptionType::kInt, offsetof(t2::DriverOptions, m_ThreadCount),
  "Specify number of build threads" },
  { 'n', "dry-run", OptionType::kBool, offsetof(t2::DriverOptions, m_DryRun),
  "Don't actually execute any build actions" },
  { 'f', "force-dag-regen", OptionType::kBool, offsetof(t2::DriverOptions, m_ForceDagRegen),
    "Force regeneration of DAG data" },
  { 'G', "dag-regen-only", OptionType::kBool, offsetof(t2::DriverOptions, m_GenDagOnly),
    "Quit after generating DAG (for debugging)" },
  { 't', "show-targets", OptionType::kBool, offsetof(t2::DriverOptions, m_ShowTargets),
    "Show available targets and exit" },
  { 'v', "verbose", OptionType::kBool, offsetof(t2::DriverOptions, m_Verbose),
    "Enable verbose build messages" },
  { 'q', "quiet", OptionType::kBool, offsetof(t2::DriverOptions, m_Quiet),
    "Be quiet" },
  { 'c', "clean", OptionType::kBool, offsetof(t2::DriverOptions, m_Clean),
    "Clean targets (remove output files)" },
  { 'l', "rebuild", OptionType::kBool, offsetof(t2::DriverOptions, m_Rebuild),
    "Rebuild targets (clean and build again)" },
  { 'w', "spammy-verbose", OptionType::kBool, offsetof(t2::DriverOptions, m_SpammyVerbose),
    "Enable spammy verbose build messages" },
  { 'D', "debug", OptionType::kBool, offsetof(t2::DriverOptions, m_DebugMessages),
    "Enable debug messages" },
  { 'S', "debug-signing", OptionType::kBool, offsetof(t2::DriverOptions, m_DebugSigning),
    "Generate an extensive log of signature generation" },
  { 's', "stats", OptionType::kBool, offsetof(t2::DriverOptions, m_DisplayStats),
    "Display stats" },
  { 'p', "profile", OptionType::kString, offsetof(t2::DriverOptions, m_ProfileOutput),
    "Output build profile" },
  { 'C', "working-dir", OptionType::kString, offsetof(t2::DriverOptions, m_WorkingDir),
    "Set working directory before building" },
  { 'R', "dagfile", OptionType::kString, offsetof(t2::DriverOptions, m_DAGFileName),
    "filename of where tundra should store the mmapped dag file"},
  { 'h', "help", OptionType::kBool, offsetof(t2::DriverOptions, m_ShowHelp),
    "Show help" },
  { 'k', "continue", OptionType::kBool, offsetof(t2::DriverOptions, m_ContinueOnError),
    "Continue building on error" },
#if defined(TUNDRA_WIN32)
  { 'U', "unprotected", OptionType::kBool, offsetof(t2::DriverOptions, m_RunUnprotected), "Run unprotected (same process group - for debugging)" },
#endif
  { 'g', "ide-gen", OptionType::kBool, offsetof(t2::DriverOptions, m_IdeGen),
    "Run IDE file generator and quit" },
  { 'Q', "quickstart", OptionType::kBool, offsetof(t2::DriverOptions, m_QuickstartGen),
    "Generate tundra.lua file for a new project" }
};

static int AssignOptionValue(char* option_base, const OptionTemplate* templ, const char* value, bool is_short)
{
  char* dest = option_base + templ->m_Offset;

  switch (templ->m_Type)
  {
    case OptionType::kBool:
      *(bool*)dest = true;
      return 1;

    case OptionType::kInt:
      if (value)
      {
        *(int*)dest = atoi(value);
        return is_short ? 2 : 1;
      }
      else
      {
        if (is_short)
          fprintf(stderr, "option requires an argument: %c\n", templ->m_ShortName);
        else
          fprintf(stderr, "option requires an argument: --%s\n", templ->m_LongName);
        return 0;
      }
      break;

    case OptionType::kString:
      if (value)
      {
        *(const char**)dest = value;
        return is_short ? 2 : 1;
      }
      else
      {
        if (is_short)
          fprintf(stderr, "option requires an argument: %c\n", templ->m_ShortName);
        else
          fprintf(stderr, "option requires an argument: --%s\n", templ->m_LongName);
        return 0;
      }
      break;

    default:
      return 0;
  }
}

static bool InitOptions(t2::DriverOptions* options, int* argc, char*** argv)
{
  int opt = 1;
  char* option_base = (char*) options;

  while (opt < *argc)
  {
    bool        found         = false;
    const char *opt_str       = (*argv)[opt];
    int         advance_count = 0;

    if ('-' != opt_str[0])
      break;

    if (opt_str[1] != '-')
    {
      const char* opt_arg = opt + 1 < *argc ? (*argv)[opt + 1] : nullptr;

      if (opt_str[2])
      {
        fprintf(stderr, "bad option: %s\n", opt_str);
        return false;
      }

      for (size_t i = 0; !found && i < ARRAY_SIZE(g_OptionTemplates); ++i)
      {
        const OptionTemplate* templ = g_OptionTemplates + i;

        if (opt_str[1] == templ->m_ShortName)
        {
          found = true;
          advance_count = AssignOptionValue(option_base, templ, opt_arg, true);
        }
      }
    }
    else
    {
      const char *equals  = strchr(opt_str, '=');
      size_t      optlen  = equals ? equals - opt_str - 2 : strlen(opt_str + 2);
      const char *opt_arg = equals ? equals + 1 : nullptr;

      for (size_t i = 0; !found && i < ARRAY_SIZE(g_OptionTemplates); ++i)
      {
        const OptionTemplate* templ = g_OptionTemplates + i;

        if (strlen(templ->m_LongName) == optlen && 0 == memcmp(opt_str + 2, templ->m_LongName, optlen))
        {
          found = true;
          advance_count = AssignOptionValue(option_base, templ, opt_arg, false);
        }
      }
    }

    if (0 == advance_count)
      return false;

    if (!found)
    {
      fprintf(stderr, "unrecognized option: %s\n", opt_str);
      return false;
    }

    opt += advance_count;
  }

  *argc -= opt;
  *argv += opt;

  return true;
}

static void ShowHelp()
{
  printf("\nTundra Build Processor 2.0\n");
  printf("Copyright (C) 2010-2018 Andreas Fredriksson\n\n");

#ifdef HAVE_GIT_INFO
  printf("Git branch: %s\n", g_GitBranch);
  printf("Git commit: %s\n\n", g_GitVersion);
#endif

  printf("This program comes with ABSOLUTELY NO WARRANTY.\n");

  printf("Usage: tundra2 [options...] [targets...]\n\n");
  printf("Options:\n");

  size_t max_opt_len = 0;
  for (size_t i = 0; i < ARRAY_SIZE(g_OptionTemplates); ++i)
  {
    size_t opt_len = strlen(g_OptionTemplates[i].m_LongName) + 12;
    if (opt_len > max_opt_len)
      max_opt_len = opt_len;
  }

  for (size_t i = 0; i < ARRAY_SIZE(g_OptionTemplates); ++i)
  {
    const OptionTemplate* t = g_OptionTemplates + i;

    if (!t->m_Help)
      continue;

    char long_text[256];
    if (t->m_Type == OptionType::kInt)
      snprintf(long_text, sizeof long_text, "%s=<integer>", t->m_LongName);
    else if (t->m_Type == OptionType::kString)
      snprintf(long_text, sizeof long_text, "%s=<string>", t->m_LongName);
    else
      snprintf(long_text, sizeof long_text, "%s          ", t->m_LongName);

    printf("  -%c, --%-*s %s\n", t->m_ShortName, (int) max_opt_len, long_text, t->m_Help);
  }
}


int main(int argc, char* argv[])
{
  using namespace t2;

  InitCommon();

  Driver driver;
  DriverOptions options;

  // Set default options
  DriverOptionsInit(&options);

  // Scan options from command line, update argc/argv
  if (!InitOptions(&options, &argc, &argv))
  {
    ShowHelp();
    return 1;
  }

  DriverInitializeTundraFilePaths(&options);
#if defined(TUNDRA_WIN32)
  if (!options.m_RunUnprotected && nullptr == getenv("_TUNDRA2_PARENT_PROCESS_HANDLE"))
  {
    // Re-launch Tundra2 as a child in a new process group. The child will be passed a handle to our process so it can
    // watch for us dying, but not be affected by sudden termination itself.
    // This ridiculous tapdance is there to prevent hard termination by things like visual studio and the mingw shell.
    // Because Tundra needs to save build state when shutting down, we need this.

    HANDLE myproc = GetCurrentProcess();
    HANDLE self_copy = NULL;
    if (!DuplicateHandle(myproc, myproc, myproc, &self_copy, 0, TRUE, DUPLICATE_SAME_ACCESS))
    {
      CroakErrno("DuplicateHandle() failed");
    }

    // Expose handle in the environment for the child process.
    {
      char handle_value[128];
      _snprintf(handle_value, sizeof handle_value, "_TUNDRA2_PARENT_PROCESS_HANDLE=%016I64x", uint64_t(self_copy));
      _putenv(handle_value);
    }

    STARTUPINFOA startup_info;
    PROCESS_INFORMATION proc_info;
    ZeroMemory(&startup_info, sizeof startup_info);
    ZeroMemory(&proc_info, sizeof proc_info);
    startup_info.cb = sizeof startup_info;

    HANDLE job_handle = CreateJobObject(NULL,  NULL);

    // Set job object limits so children can break out.
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    ZeroMemory(&limits, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;

    if (!SetInformationJobObject(job_handle, JobObjectExtendedLimitInformation, &limits, sizeof limits) )
      CroakErrno("couldn't set job info");

    if (!CreateProcessA(NULL, GetCommandLineA(), NULL, NULL, TRUE, CREATE_BREAKAWAY_FROM_JOB|CREATE_NEW_PROCESS_GROUP|CREATE_SUSPENDED, NULL, NULL, &startup_info, &proc_info))
      CroakErrno("CreateProcess() failed");

    AssignProcessToJobObject(job_handle, proc_info.hProcess);
    ResumeThread(proc_info.hThread);

    WaitForSingleObject(proc_info.hProcess, INFINITE);

    DWORD exit_code = 1;
    GetExitCodeProcess(proc_info.hProcess, &exit_code);

    CloseHandle(proc_info.hThread);
    CloseHandle(proc_info.hProcess);
    ExitProcess(exit_code);
  }
  else if (const char* handle_str = getenv("_TUNDRA2_PARENT_PROCESS_HANDLE"))
  {
    HANDLE h = (HANDLE) _strtoi64(handle_str, NULL, 16);
    SignalHandlerInitWithParentProcess(h);
  }
  else
  {
    SignalHandlerInit();
  }
#else
  SignalHandlerInit();
#endif

  uint64_t start_time = TimerGet();

  ExecInit();

  if (options.m_WorkingDir)
  {
    if (!SetCwd(options.m_WorkingDir))
      Croak("couldn't change directory to %s", options.m_WorkingDir);
  }

  if (options.m_ShowHelp)
  {
    ShowHelp();
    return 0;
  }

  // Initialize logging
  int log_flags = kWarning | kError;

  if (options.m_DebugMessages)
    log_flags |= kInfo | kDebug;

  if (options.m_SpammyVerbose)
    log_flags |= kSpam | kInfo | kDebug;

  SetLogFlags(log_flags);

  if (options.m_IdeGen)
  {
    // FIXME: How to detect build file for other type of generators?
    GenerateIdeIntegrationFiles("tundra.lua", argc, (const char**) argv);
    return 0;
  }

  if (options.m_QuickstartGen)
  {
    GenerateTemplateFiles(argc, (const char**) argv);
    return 0;
  }

  // Protect against running two or more instances simultaneously in the same directory.
  // This can happen if Visual Studio is trying to launch more than one copy of tundra.
#if defined(TUNDRA_WIN32)
  {
    char mutex_name[MAX_PATH];
    char cwd[MAX_PATH];
    char cwd_nerfed[MAX_PATH];
    GetCwd(cwd, sizeof cwd);
    const char *i = cwd;
    char *o = cwd_nerfed;
    char ch;
    do
    {
      ch = *i++;
      switch (ch)
      {
      case '\\':
      case ':':
        ch = '^';
        break;
      }
      *o++ = ch;
    } while(ch);

    _snprintf(mutex_name, sizeof mutex_name, "Global\\Tundra--%s-%s", cwd_nerfed, options.m_DAGFileName);
    mutex_name[sizeof(mutex_name)-1] = '\0';
    bool warning_printed = false;
    HANDLE mutex = CreateMutexA(nullptr, false, mutex_name);

    while (WAIT_TIMEOUT == WaitForSingleObject(mutex, 100))
    {
      if (!warning_printed)
      {
        Log(kWarning, "More than one copy of Tundra running in %s -- PID %u waiting", cwd, GetCurrentProcessId());
        warning_printed = true;
      }
      Sleep(100);
    }

    Log(kDebug, "PID %u successfully locked %s", GetCurrentProcessId(), cwd);
  }
#endif

  // Initialize driver
  if (!DriverInit(&driver, &options))
    return 1;

  // Initialize profiler if needed
  if (driver.m_Options.m_ProfileOutput)
    ProfilerInit(driver.m_Options.m_ProfileOutput, driver.m_Options.m_ThreadCount);


  BuildResult::Enum build_result = BuildResult::kSetupError;

  if (!DriverInitData(&driver))
    goto leave;

  if (driver.m_Options.m_GenDagOnly)
  {
    Log(kDebug, "Only generating DAG - quitting");
    build_result = BuildResult::kOk;
    goto leave;
  }

  if (driver.m_Options.m_ShowTargets)
  {
    DriverShowTargets(&driver);
    Log(kDebug, "Only showing targets - quitting");
    build_result = BuildResult::kOk;
    goto leave;
  }

  DriverRemoveStaleOutputs(&driver);

  // Prepare list of nodes to build/clean/rebuild
  if (!DriverPrepareNodes(&driver, (const char**) argv, argc))
  {
    Log(kError, "couldn't set up list of targets to build");
    goto leave;
  }

  if (driver.m_Options.m_Clean || driver.m_Options.m_Rebuild)
  {
    DriverCleanOutputs(&driver);

    if (!driver.m_Options.m_Rebuild)
    {
      build_result = BuildResult::kOk;
      goto leave;
    }
  }

  build_result = DriverBuild(&driver);

  if (!DriverSaveBuildState(&driver))
    Log(kError, "Couldn't save build state");

  if (!DriverSaveScanCache(&driver))
    Log(kWarning, "Couldn't save header scanning cache");

  if (!DriverSaveDigestCache(&driver))
    Log(kWarning, "Couldn't save SHA1 digest cache");

leave:
  DriverDestroy(&driver);

  // Dump/close profiler
  if (driver.m_Options.m_ProfileOutput)
    ProfilerDestroy();

  // Dump stats
  if (options.m_DisplayStats)
  {
    printf("output cleanup:    %10.2f ms\n", TimerToSeconds(g_Stats.m_StaleCheckTimeCycles) * 1000.0);
    printf("json parse time:   %10.2f ms\n", TimerToSeconds(g_Stats.m_JsonParseTimeCycles) * 1000.0);
    printf("scan cache:\n");
    printf("  hits (new):      %10u\n", g_Stats.m_NewScanCacheHits);
    printf("  hits (frozen):   %10u\n", g_Stats.m_OldScanCacheHits);
    printf("  misses:          %10u\n", g_Stats.m_ScanCacheMisses);
    printf("  inserts:         %10u\n", g_Stats.m_ScanCacheInserts);
    printf("  save time:       %10.2f ms\n", TimerToSeconds(g_Stats.m_ScanCacheSaveTime) * 1000.0);
    printf("  entries dropped: %10u\n", g_Stats.m_ScanCacheEntriesDropped);
    printf("file signing:\n");
    printf("  cache hits:      %10u\n", g_Stats.m_DigestCacheHits);
    printf("  cache get time:  %10.2f ms\n", TimerToSeconds(g_Stats.m_DigestCacheGetTimeCycles) * 1000.0);
    printf("  cache save time: %10.2f ms\n", TimerToSeconds(g_Stats.m_DigestCacheSaveTimeCycles) * 1000.0);
    printf("  digests:         %10u\n", g_Stats.m_FileDigestCount);
    printf("  digest time:     %10.2f ms\n", TimerToSeconds(g_Stats.m_FileDigestTimeCycles) * 1000.0);
    printf("stat cache:\n");
    printf("  hits:            %10u\n", g_Stats.m_StatCacheHits);
    printf("  misses:          %10u\n", g_Stats.m_StatCacheMisses);
    printf("  dirty:           %10u\n", g_Stats.m_StatCacheDirty);
    printf("building:\n");
    printf("  old records:     %10u\n", g_Stats.m_StateSaveOld);
    printf("  new records:     %10u\n", g_Stats.m_StateSaveNew);
    printf("  dropped records: %10u\n", g_Stats.m_StateSaveDropped);
    printf("  state save time: %10.2f ms\n", TimerToSeconds(g_Stats.m_StateSaveTimeCycles) * 1000.0);
    printf("  exec() count:    %10u\n", g_Stats.m_ExecCount);
    printf("  exec() time:     %10.2f s\n", TimerToSeconds(g_Stats.m_ExecTimeCycles));
    printf("low-level syscalls:\n");
    printf("  mmap() calls:    %10u\n", g_Stats.m_MmapCalls);
    printf("  mmap() time:     %10.2f ms\n", TimerToSeconds(g_Stats.m_MmapTimeCycles) * 1000.0);
    printf("  munmap() calls:  %10u\n", g_Stats.m_MunmapCalls);
    printf("  munmap() time:   %10.2f ms\n", TimerToSeconds(g_Stats.m_MunmapTimeCycles) * 1000.0);
    printf("  stat() calls:    %10u\n", g_Stats.m_StatCount);
    printf("  stat() time:     %10.2f ms\n", TimerToSeconds(g_Stats.m_StatTimeCycles) * 1000.0);
  }

  if (!options.m_Quiet)
  {
    double total_time = TimerDiffSeconds(start_time, TimerGet());
    if (total_time < 60.0)
    {
      printf("*** %s (%.2f seconds)\n", BuildResult::Names[build_result], total_time);
    }
    else
    {
      int t = (int)total_time;
      int h = t / 3600; t -= h * 3600;
      int m = t / 60; t -= m * 60;
      int s = t;
      printf("*** %s (%.2f seconds - %d:%02d:%02d)\n", BuildResult::Names[build_result], total_time, h, m, s);
    }
  }

  return build_result == BuildResult::kOk ? 0 : 1;

  // Match up nodes to nodes in the build state
  //   Walk DAG node array in parallel with build state node array
  //   If a node is not present in the DAG but is in the old build state:
  //     Loop over its outputs and delete them
  // For all DAG nodes:
  //   Loop over their aux output references and flag them as in use
  //
  // Loop over aux outputs in union of (build state | dag)
  //   Delete file if it is not flagged as in use
  //
  // Clean up empty output directories:
  //   If any file was deleted due to cleanup:
  //     Walk all directories known to be created and rmdir() them
  //      (If any valid files are left in there we'll get an error and move on)
  //
  // Find out what nodes we want to build this time (match command line args against DAG info)
  //
  //   Construct config set:
  //     If config list on cmdline is empty, use default config
  //     Otherwise, look up each config and add to set
  //
  //   For each alias on the command line:
  //      Add the set of nodes referenced by the alias to the build queue
  //        Filtered by config set
  //
  //   If set of nodes to build is empty:
  //     Add all default sets (filter by config set) to the build queue
  //
  // Set up node state for each node to build
  //
  // Set up nodes for pass barriers
  //
  // Build
  //
  // Generate build state data
  //
  // Save header scanning state
  // Report stats
  // Exit
}
