#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Bell Laboratories
 *
 * convert \x character constants in s in place
 * the length of the converted s is returned (may have imbedded \0's)
 */

#include <ast.h>

int
stresc(register char* s)
{
	register char*	t;
	register int	c;
	char*		b;
	char*		p;

	b = t = s;
	for (;;)
	{
		switch (c = *s++)
		{
		case '\\':
			c = chresc(s - 1, &p);
			s = p;
			break;
		case 0:
			*t = 0;
			return(t - b);
		}
		*t++ = c;
	}
}
