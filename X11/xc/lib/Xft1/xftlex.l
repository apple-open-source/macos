%{
/*
 * $XFree86: xc/lib/Xft1/xftlex.l,v 1.1.1.1 2002/02/15 01:26:15 keithp Exp $
 *
 * Copyright (c) 2000 Keith Packard, member of The XFree86 Project, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include "xftint.h"

#include "xftgram.h"

int	    XftConfigLineno;

static void _XftConfigSkipComment (void);

static void _XftConfigSkipLine (void);
    
static int _XftConfigPopInput (void);
    
    
#define		XFT_CONFIG_IN_DEEP	20
FILE	*XftConfigInStack[XFT_CONFIG_IN_DEEP];
FILE	**XftConfigInpt = XftConfigInStack + XFT_CONFIG_IN_DEEP;
FILE	*XftConfigInput;
int	XftConfigLinenos[XFT_CONFIG_IN_DEEP];
int 	*XftConfigLinenopt = XftConfigLinenos + XFT_CONFIG_IN_DEEP;
char	*XftConfigFile = "";
char	*XftConfigFileNames[XFT_CONFIG_IN_DEEP];
char	**XftConfigFileNamePt = XftConfigFileNames + XFT_CONFIG_IN_DEEP;
int	XftConfigFiledeep = 0;

#undef YY_INPUT

#define YY_INPUT(buf,result,max_size) \
{ \
    int c; \
    result = 0; \
    while (result < max_size) { \
	c = getc (XftConfigInput); \
	if (c < 0) { \
	    c = _XftConfigPopInput (); \
	    if (c < 0) \
		break; \
	} \
	buf[result++] = c; \
	if (c == '\n') \
	{ \
	    XftConfigLineno++; \
	    break; \
	} \
    } \
}
    
#ifndef FLEX_SCANNER
#undef		input
#undef		unput
static int input (void)
{
    char    buf[1];
    static  int	r = EOF;

    if (r == 0)
	return 0;
    YY_INPUT(buf, r, 1);
    if (r == 0)
	return EOF;
    return buf[0];
}

static void unput (char c)
{
    if (!c || c == EOF)
	return;
    if (c == '\n')
	XftConfigLineno--;
    ungetc (c, XftConfigInput);
}
#endif

%}
%%
"/\052"		_XftConfigSkipComment();
^#		_XftConfigSkipLine();
dir		return DIR;
cache		return CACHE;
include		return INCLUDE;
includeif	return INCLUDEIF;
match		return MATCH;
edit		return EDIT;
true		return TOK_TRUE;
false		return TOK_FALSE;
nil		return TOK_NIL;
any		return ANY;
all		return ALL;
"="		return EQUAL;
":"		return COLON;
";"		return SEMI;
"+"		return PLUS;
"-"		return MINUS;
"*"		return TIMES;
"/"		return DIVIDE;
"!"		return NOT;
"&&"		return ANDAND;
"||"		return OROR;
"<"		return LESS;
"<="		return LESSEQ;
"=="		return EQEQ;
">="		return MOREEQ;
">"		return MORE;
"!="		return NOTEQ;
"?"		return QUEST;
"["		return OS;
"]"		return CS;
\"([^\n\"]|\\\")*\"	{
    			yytext[yyleng - 1] = '\0';
			yylval.sval = yytext+1;
			return STRING;
		}
[0-9]+		{
		yylval.ival = strtol (yytext, 0, 10);
		return INTEGER;
		}
(([0-9]+((\.[0-9]*(\{[0-9]+\})?)?))|(\.[0-9]+)|(\.[0-9]*\{[0-9]+\}))(([Ee][-+]?[0-9]+)?) {
		yylval.dval = strtod (yytext, 0);
		return DOUBLE;
		}
[a-zA-Z][0-9a-zA-Z_]* {
		if (XftNameConstant (yytext, &yylval.ival))
		    return INTEGER;
		yylval.sval = yytext;
		return NAME;
		}
.		;
"\n"		;
%%
static void
_XftConfigSkipComment (void)
{
    int	c;

    c = input();
    for (;;) {
	while (c != EOF && c != '*')
	    c = input();
	if (c == EOF)
	    return;
	c = input();
	if (c == EOF || c == '/')
	    return;
    }
}

static void
_XftConfigSkipLine (void)
{
    int	    c;

    do {
	c = input();
    } while (c != EOF && c != '\n');
    if (c == '\n') unput('\n');
}

Bool
XftConfigLexFile(char *s)
{
    FILE    *f;
    
    f = fopen (s, "r");
    if (f == 0) 
    {
	fprintf (stderr, "cannot open file \"%s\"\n", s);
	return False;
    }
    ++XftConfigFiledeep;
    XftConfigInput = f;
    XftConfigFile = s;
    return True;
}
	
Bool
XftConfigPushInput (char *s, Bool complain)
{
    FILE	*f;
    char	*t;
    char	*h;

    if (XftConfigInpt == XftConfigInStack) 
    {
	(void) fprintf (stderr, "files nested too deeply\n");
	return False;
    }
    t = s;
    if (*s == '~')
    {
	h = getenv ("HOME");
	if (h)
	{
	    t = (char *) malloc (strlen (h) + strlen (s));
	    if (t)
	    {
		strcpy (t, h);
		strcat (t, s+1);
	    }
	    else
		t = s;
	}
    }
    f = fopen (t, "r");
    if (t != s)
	free (t);
    if (f == 0) 
    {
	if (complain)
	    (void) fprintf (stderr, "cannot open file %s\n", s);
	return False;
    }
    ++XftConfigFiledeep;
    *--XftConfigInpt = XftConfigInput;
    *--XftConfigLinenopt = XftConfigLineno;
    *--XftConfigFileNamePt = XftConfigFile;
    XftConfigInput = f;
    XftConfigLineno = 1;
    XftConfigFile = _XftSaveString (s);
    return True;
}

static int
_XftConfigPopInput (void)
{
    int	    c;

    for (;;)
    {
	c = getc (XftConfigInput);
	if (c >= 0)
	    return c;
	fclose (XftConfigInput);
	XftConfigInput = 0;
	if (XftConfigInpt == XftConfigInStack + XFT_CONFIG_IN_DEEP)
	    return EOF;
	XftConfigInput = *XftConfigInpt++;
	XftConfigLineno = *XftConfigLinenopt++;
	free (XftConfigFile);
	XftConfigFile = *XftConfigFileNamePt++;
	--XftConfigFiledeep;
    }
}

void
XftConfigLexDone (void)
{
#ifdef FLEX_SCANNER
    XftConfig_delete_buffer (XftConfig_current_buffer);
#endif
}
