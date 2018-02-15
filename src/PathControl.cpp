
// This is a command-line utility used by the Windows installer to manipulate
// the system path.

#include <windows.h>
#include <stdlib.h>

const WCHAR g_UsageText[] =
  L"Usage:\r\n"
  L"Add PATH directory: PathControl.exe /ADD <path>\r\n"
  L"Remove PATH directory: PathControl.exe /REMOVE <path>\r\n";

static WCHAR** TokenizePath(WCHAR* buffer, DWORD* token_count_out)
{
  // Always allocate one more seg in case we're going to be adding an element later.
  int max_segs = 2;

  WCHAR* p = buffer;
  while (WCHAR ch = *p++)
    max_segs += ';' == ch;

  *token_count_out = 0;

  DWORD token_count = 0;
  WCHAR** result = (WCHAR**) malloc(sizeof(WCHAR*) * max_segs);

  p = buffer;

  do
  {
    if (p[0] == ';')
    {
      p[0] = '\0';
      ++p;
    }

    result[token_count++] = p;

    p = wcschr(p, ';');
  } while (p);

  *token_count_out = token_count;

  return result;
}

static WCHAR* GetRegString(HKEY key, const WCHAR* value_name)
{
  DWORD value_type;
  DWORD buffer_size = 0;

  if (ERROR_SUCCESS != RegGetValueW(key, NULL, value_name, RRF_RT_REG_SZ, &value_type, NULL, &buffer_size))
    return NULL;

  // Buffer size is in bytes, so no need to * by WCHAR
  WCHAR* buffer = (WCHAR*) malloc(buffer_size);

  if (ERROR_SUCCESS != RegGetValueW(key, NULL, value_name, RRF_RT_REG_SZ, &value_type, buffer, &buffer_size))
    return NULL;

  return buffer;
}

static void CmdlineError()
{
  MessageBox(NULL, g_UsageText, L"Error", MB_OK|MB_ICONEXCLAMATION);
  ExitProcess(1);
}

static void PermissionError()
{
  MessageBox(NULL, L"Error accessing the registry. Do you have permission?", L"Error", MB_OK|MB_ICONEXCLAMATION);
  ExitProcess(1);
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
  LPWSTR* arg_list;
  int arg_count;

  if (NULL == (arg_list = CommandLineToArgvW(GetCommandLineW(), &arg_count)))
  {
    MessageBox(NULL, L"Couldn't parse command line", L"Error", MB_OK|MB_ICONEXCLAMATION);
    return 1;
  }

  if (arg_count != 3)
    CmdlineError();

  HKEY env_key;
  DWORD env_err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_ALL_ACCESS|KEY_WOW64_64KEY, &env_key);
  if (ERROR_SUCCESS != env_err)
    PermissionError();

  WCHAR* buffer = GetRegString(env_key, L"PATH");
  if (!buffer)
    PermissionError();

  // Tokenize the value into strings separated by ';'
  DWORD token_count;
  WCHAR** tokens = TokenizePath(buffer, &token_count);

  for (DWORD i = 0; i < token_count; ++i)
  {
    if (0 == _wcsicmp(tokens[i], arg_list[2]))
    {
      if (i + 1 < token_count)
      {
        memmove(&tokens[i], &tokens[i + 1], sizeof(WCHAR*) * (token_count - i - 1));
      }
      token_count--;
      break;
    }
  }

  if (0 == wcscmp(arg_list[1], L"/ADD"))
  {
    tokens[token_count++] = arg_list[2];
  }
  else if (0 == wcscmp(arg_list[1], L"/REMOVE"))
  {
    // OK - path already removed if it existed.
  }
  else
  {
    CmdlineError();
  }

  // Make a new command line by merging the tokens together.
  DWORD new_nchars = token_count ? (token_count - 1) : 0;
  for (DWORD i = 0; i < token_count; ++i)
  {
    new_nchars += wcslen(tokens[i]);
  }

  DWORD out_size_bytes = sizeof(WCHAR) * (new_nchars + 1);
  WCHAR* out_buf = (WCHAR*) malloc(out_size_bytes);
  WCHAR* p = out_buf;
  for (DWORD i = 0; i < token_count; ++i)
  {
    if (i > 0)
      *p++ = ';';
    size_t len = wcslen(tokens[i]);
    memcpy(p, tokens[i], sizeof(WCHAR) * len);
    p += len;
  }
  *p++ = L'\0';

  if (ERROR_SUCCESS != RegSetValueExW(env_key, L"PATH", 0, REG_EXPAND_SZ, (BYTE*) out_buf, out_size_bytes))
    PermissionError();

  if (0 == SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) L"Environment", SMTO_BLOCK, 100, NULL))
  {
    if (ERROR_TIMEOUT == GetLastError())
    {
      MessageBox(NULL, L"Timed out trying to update environment variables.\r\nYou will need a reboot.", L"Warning", MB_OK|MB_ICONWARNING);
    }
    else
    {
      MessageBox(NULL, L"Something went wrong trying to notify Windows about the environment change.\r\nYou will need a reboot.", L"Warning", MB_OK|MB_ICONWARNING);
    }
  }

  return 0;
}
