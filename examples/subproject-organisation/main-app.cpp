
// "extlibs" is in the search path, so I can simply include the library directly
#include "libA/libHeader.h"

#include <stdio.h>

int main(int argc, char **argv) {
    printf("Hello World from the main app!\n");
    libA_process();
    return 0;
}
