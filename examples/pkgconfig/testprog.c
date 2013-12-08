#include <png.h>
#include <stdio.h>

int main()
{
  png_uint_32 version = png_access_version_number();

  printf("PNG library is version %08x\n", version);

  return 0;
}
