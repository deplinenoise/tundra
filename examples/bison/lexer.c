/* The lexical analyzer returns a double floating point
   number on the stack and the token NUM, or the numeric code
   of the character read if not a number.  It skips all blanks
   and tabs, and returns 0 for end-of-input.  */

#include <ctype.h>
#include <stdio.h>
#define YYSTYPE double
#include "bisongen_parser.h"

int yylex (void)
{
	int c;

	/* Skip white space.  */
	while ((c = getchar ()) == ' ' || c == '\t')
		;
	/* Process numbers.  */
	if (c == '.' || isdigit (c))
	{
		ungetc (c, stdin);
		scanf ("%lf", &yylval);
		return NUM;
	}
	/* Return end-of-input.  */
	if (c == EOF)
		return 0;
	/* Return a single char.  */
	return c;
}

