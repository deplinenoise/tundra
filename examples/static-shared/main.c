
#if defined(SHARED)
# if defined(_MSC_VER)
#  define DECORATION __declspec(dllimport)
# endif
#endif

#ifndef DECORATION
#define DECORATION
#endif

extern void DECORATION LibFunction(void);

int main()
{
  LibFunction();
}
