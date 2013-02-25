#ifndef EXEC_HPP
#define EXEC_HPP

namespace t2
{
  struct EnvVariable
  {
    const char *m_Name;
    const char *m_Value;
  };

  struct ExecResult
  {
    int     m_ReturnCode;
    bool    m_WasSignalled;
  };

  void ExecInit(void);

  ExecResult ExecuteProcess(
        const char*         cmd_line,
        int                 env_count,
        const EnvVariable*  env_vars,
        int                 job_id,
        int                 echo_cmdline,
        const char*         annotation);
}

#endif
