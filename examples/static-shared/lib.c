#include <stdio.h>

#if defined(SHARED)
# if defined(_MSC_VER)
#  define DECORATION __declspec(dllexport)
# elif defined(__GNUC__)
#  define DECORATION __attribute__((visibility("default")))
# endif
#endif

#ifndef DECORATION
#define DECORATION
#endif

void DECORATION LibFunction(void)
{
  printf("I'm a library function!\n");
}
