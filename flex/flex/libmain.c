/* libmain - flex run-time support library "main" function */

/* $Header: /cvs/Darwin/Commands/GNU/flex/flex/libmain.c,v 1.1.1.1 1999/04/23 00:46:30 wsanchez Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
