#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * return lex name for op[subop]
 */

#include "exlib.h"
#include "exop.h"

#define TOTNAME		3
#define MAXNAME		16

char*
exlexname(int op, int subop)
{
	register char*	b;

	static int	n;
	static char	buf[TOTNAME][MAXNAME];

	if (op > MINTOKEN && op < MAXTOKEN)
		return (char*)exop[op - MINTOKEN];
	if (++n > TOTNAME)
		n = 0;
	b = buf[n];
	if (op == '=')
	{
		if (subop > MINTOKEN && subop < MAXTOKEN)
			sfsprintf(b, MAXNAME, "%s=", exop[subop - MINTOKEN]);
		else if (subop > ' ' && subop <= '~')
			sfsprintf(b, MAXNAME, "%c=", subop);
		else sfsprintf(b, MAXNAME, "(%d)=", subop);
	}
	else if (op > ' ' && op <= '~')
		sfsprintf(b, MAXNAME, "%c", op);
	else sfsprintf(b, MAXNAME, "(%d)", op);
	return b;
}
