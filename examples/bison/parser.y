/* Reverse polish notation calculator. 
   Adapted from the GNU Bison manual for this example.
*/

%{
   #define YYSTYPE double
   #include <math.h>
   #include <stdio.h>
   int yylex (void);
   void yyerror (char const *);
%}

%token NUM

%% /* Grammar rules and actions follow.  */

input:    /* empty */
	 | input line
;

line:     '\n'
	| exp '\n'      { printf ("\t%.10g\n", $1); }
;

exp:      NUM           { $$ = $1;           }
   | exp exp '+'   { $$ = $1 + $2;      }
   | exp exp '-'   { $$ = $1 - $2;      }
   | exp exp '*'   { $$ = $1 * $2;      }
   | exp exp '/'   { $$ = $1 / $2;      }
   /* Exponentiation */
   | exp exp '^'   { $$ = pow ($1, $2); }
   /* Unary minus    */
   | exp 'n'       { $$ = -$1;          }
;

%%

