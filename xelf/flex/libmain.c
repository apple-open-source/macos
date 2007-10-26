/* libmain - flex run-time support library "main" function */

/* $Header: /Users/Shared/flex/flex/flex/libmain.c,v 1.2 2005/02/09 04:22:06 llattanz Exp $ */

extern int yylex();

int main( argc, argv )
int argc;
char *argv[];
	{
	while ( yylex() != 0 )
		;

	exit(0);
	}
