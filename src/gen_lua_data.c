/*
   Copyright 2010 Andreas Fredriksson

   This file is part of Tundra.

   Tundra is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Tundra is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Tundra.  If not, see <http://www.gnu.org/licenses/>.
*/

/* gen_lua_data.c - Generate binary Lua data for inclusion in the Tundra executable */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void
print_fn(const char *fn)
{
	int i;
	static const char build_prefix[] = "tundra-output/";
	char buffer[260];

	strncpy(buffer, fn, sizeof(buffer));
	buffer[sizeof(buffer)-1] = 0;

	for (i = 0; ; ++i)
	{
		char ch = buffer[i];
		if (!ch)
			break;
		else if ('\\' == ch)
			buffer[i] = '/';
	}

	putchar('"');

	if (strstr(buffer, build_prefix) == buffer)
	{
		const char *second_slash = strchr(buffer + sizeof(build_prefix), '/');
		if (second_slash)
			fputs(second_slash+1, stdout);
		else
			fputs(buffer, stdout);
	}
	else
		fputs(buffer, stdout);

	putchar('"');
}

int main(int argc, char *argv[1])
{
	int i;

	puts("/* automatically generated, do not edit */\n#include \"gen_lua_data.h\"\n");
	printf("const int td_lua_file_count = %d;\n", argc - 1);
	printf("const td_lua_file td_lua_files[%d] = {\n", argc-1);

	for (i = 1; i < argc; ++i)
	{
		const char *fn = argv[i];
		size_t read_size;
		size_t bytes_total = 0;
		char buffer[2048];
		FILE* f = fopen(fn, "rb");

		if (!f)
		{
			fprintf(stderr, "couldn't open '%s' for input\n", fn);
			exit(1);
		}

		fputs("{ ", stdout);
		print_fn(fn);
		fputs(", ", stdout);

		while (0 != (read_size = fread(buffer, 1, sizeof(buffer), f)))
		{
			size_t k;

			bytes_total += read_size;

			printf("\"");
			for (k = 0; k < read_size; ++k)
			{
				char ch = buffer[k];
				switch (ch)
				{
					case ' ': fputs(" ", stdout); break;
					case '\t': fputs("\\t", stdout); break;
					case '\n': fputs("\\n", stdout); break;
					case '\r': fputs("\\r", stdout); break;
					case '"': fputs("\\\"", stdout); break;
					case '\\': fputs("\\\\", stdout); break;
					default:
							   {
								   if (isalnum(ch) || ispunct(ch))
									   putchar(ch);
								   else
								   {
									   printf("\\x%02x\"\"", ch & 0xff);
								   }
							   }
							   break;
				}
			}
			printf("\"\n");
		}

		printf(", %u }%s\n\n", (unsigned int) bytes_total, (i + 1) < argc ? "," : "");
	}
	puts("};");

	return 0;
}
