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

int main(int argc, char *argv[1])
{
	int i, count;

	count = (argc - 1) / 2;

	puts("/* automatically generated, do not edit */\n#include \"gen_lua_data.h\"\n");
	printf("const int td_lua_file_count = %d;\n", count);
	printf("const td_lua_file td_lua_files[%d] = {\n", count);

	for (i = 1; (i + 1) < argc; i += 2)
	{
		const char *module_name = argv[i];
		const char *fn = argv[i+1];
		size_t read_size;
		size_t bytes_total = 0;
		char buffer[2048];
		FILE* f = fopen(fn, "rb");

		if (!f)
		{
			fprintf(stderr, "couldn't open '%s' for input\n", fn);
			exit(1);
		}

		printf("{ \"%s\", ", module_name);

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

		fclose(f);
	}
	puts("};");

	return 0;
}
