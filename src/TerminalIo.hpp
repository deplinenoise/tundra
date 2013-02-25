#ifndef TERMINALIO_HPP
#define TERMINALIO_HPP

namespace t2
{

void TerminalIoInit(void);
void TerminalIoDestroy(void);

void TerminalIoEmit(int job_id, int is_stderr, int sort_key, const char *data, int len);
void TerminalIoJobExit(int job_id);
void TerminalIoPrintf(int job_id, int sort_key, const char *format, ...);

}

#endif
