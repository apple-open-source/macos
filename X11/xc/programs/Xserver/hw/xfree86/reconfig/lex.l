/* $XFree86: xc/programs/Xserver/hw/xfree86/reconfig/lex.l,v 3.7 1996/12/23 06:51:42 dawes Exp $ */




/* $XConsortium: lex.l /main/5 1996/10/25 14:12:13 kaleb $ */

%{
#include "os.h"
#include "strlist.h"
#include "reconfig.h"

struct keyword { 
	int token;
	char *name;
};
	
struct keyword keywords[] = {
#include "keyword.i"
	-1, ""
}; 

/* strcasecmp */
int strcscmp(s1,s2)
char *s1, *s2;
{
	int c1, c2;

	while ( *s1 && *s2 ) {
		c1 = *s1; 
		c2 = *s2; 
		if ( isupper(c1) ) c1 = tolower(c1);
		if ( isupper(c2) ) c2 = tolower(c2);
		if ( c1 != c2 ) return c1 - c2;
		s1++; s2++;
	}
	return *s1 - *s2;
}

int lookup(s)
char *s;
{
	struct keyword *kp;

	for ( kp = keywords; kp->token >= 0 ; kp++ ) {
		if ( strcscmp(s,kp->name) == 0 ) return kp->token;
	}
	return WRONG;
}

char *sdup(s)
char *s;
{
	char *s2;
	int n = strlen(s); /* Excluding terminating '\0' */

	s2 = malloc(n+1);
	strcpy(s2,s);
	return s2;
}

int line = 1;
%}

digit		[0-9]
hexdigit	[0-9a-fA-F]
octdigit	[0-7]
identifier	[-+]?[a-zA-Z][a-zA-Z0-9/]*
hexnum		0[xX]{hexdigit}+
octnum		0{octdigit}+
decnum		(([1-9]{digit}*)|0)
flonum		{decnum}"."{digit}*
string		\"[^\"\n]*\"

comment		\#([^\n]*)
number		({hexnum}|{octnum}|{decnum}|{flonum})
white		[ \t]+

%%

{identifier}	{ return lookup(yytext); }
{number}	{ yylval.string = sdup(yytext); return NUMBER; }
{string}	{ yylval.string = sdup(yytext); return STRING; }
{comment}	break;
{white}		break;
\n		{ line++; break; }

%%
