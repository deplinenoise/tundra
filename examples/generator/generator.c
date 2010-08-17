#include <stdio.h>
#include <ctype.h>

/* silly source generator example */

int main(int argc, char *argv[])
{
	FILE *in, *out;
	int ch;

	if (argc < 2)
		return 1;
	
	if (NULL == (in = fopen(argv[1], "r")))
		return 1;

	if (NULL == (out = fopen(argv[2], "w")))
		return 1;

	fprintf(out, "/* a silly file, generated from %s */\n\n", argv[1]);
	fprintf(out, "const char *generated_string = \"");
	putc('"', in);

	while (EOF != (ch = getc(in)))
	{
		if ('\n' == ch)
			putc('\\', out);
		putc(toupper(ch), out);
	}

	fprintf(out, "\";\n");

	fclose(out);
	fclose(in);

	return 0;
}
