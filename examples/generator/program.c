#include <stdio.h>

extern const char* generated_string;

int main(int argc, char** argv)
{
	puts(generated_string);
	return 0;
}
