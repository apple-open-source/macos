#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Bell Laboratories
 *
 * convert a 0 terminated character constant string to an int
 */

#include <ast.h>

int
chrtoi(register const char* s)
{
	register int	c;
	register int	n;
	register int	x;
	char*		p;

	c = 0;
	for (n = 0; n < sizeof(int) * CHAR_BIT; n += CHAR_BIT)
	{
		switch (x = *((unsigned char*)s++))
		{
		case '\\':
			x = chresc(s - 1, &p);
			s = (const char*)p;
			break;
		case 0:
			return(c);
		}
		c = (c << CHAR_BIT) | x;
	}
	return(c);
}
