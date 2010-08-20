#include <stdio.h>

int main(int argc, char *argv[])
{
	FILE *f;

	if (1 > argc || NULL == (f = fopen(argv[1], "w")))
		return 1;

	fprintf(f, "/* a silly generated file */\n");
	fprintf(f, "int generated_value = 10;\n");
	fclose(f);

	return 0;
}
