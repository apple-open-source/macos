/* $XFree86: xc/programs/xditview/lex.c,v 1.4 2001/08/01 00:45:03 tsi Exp $ */

#include <X11/Xos.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

int
DviGetAndPut(dw, cp)
    DviWidget	dw;
    int		*cp;
{
    if (dw->dvi.ungot)
    {
	dw->dvi.ungot = 0;
	*cp = getc (dw->dvi.file);
    }
    else
    {
	*cp = getc (dw->dvi.file);
	putc (*cp, dw->dvi.tmpFile);
    }
    return *cp;
}

char *
GetLine(dw, Buffer, Length)
	DviWidget	dw;
	char	*Buffer;
	int	Length;
{
	int 	i = 0, c;
	char	*p = Buffer;

	Length--;			    /* Save room for final NULL */

	while ((!p || i < Length) && DviGetC (dw, &c) != EOF && c != '\n')
		if (p)
			*p++ = c;
#if 0
	if (c == '\n' && p)		    /* Retain the newline like fgets */
		*p++ = c;
#endif
	if (c == '\n')
		DviUngetC(dw, c);
	if (p)
		*p = '\0';
	return (Buffer);
}

char *
GetWord(dw, Buffer, Length)
	DviWidget	dw;
	char	*Buffer;
	int	Length;
{
	int 	i = 0, c;
	char	*p = Buffer;

	Length--;			    /* Save room for final NULL */
	while (DviGetC(dw, &c) != EOF && isspace(c))
		;
	if (c != EOF)
		DviUngetC(dw, c);
	while (i < Length && DviGetC(dw, &c) != EOF && !isspace(c))
		if (p)
			*p++ = c;
	if (c != EOF)
		DviUngetC(dw, c);
	if (p)
		*p = '\0';
	return (Buffer);
}

int
GetNumber(dw)
	DviWidget	dw;
{
	int	i = 0,  c;

	while (DviGetC(dw, &c) != EOF && isspace(c))
		;
	if (c != EOF)
		DviUngetC(dw, c);
	while (DviGetC(dw, &c) != EOF && isdigit(c))
		i = i*10 + c - '0';
	if (c != EOF)
		DviUngetC(dw, c);
	return (i);
}
