#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Research
 *
 * return string with expanded escape chars
 */

#include <ast.h>
/* #include <ccode.h> */
#include <ctype.h>
#include <string.h>

/*
 * quote string as of length n with qb...qe
 * (flags&1) forces quote, otherwise quote output only if necessary
 * qe and the usual suspects are \... escaped
 * (flags&2) doesn't quote 8 bit chars
 */

char*
fmtquote(const char* as, const char* qb, const char* qe, size_t n, int flags)
{
	register unsigned char*	s = (unsigned char*)as;
	register unsigned char*	e = s + n;
	register char*		b;
	register int		c;
	int			k;
	int			q = 0;
	char*			buf;

	c = 4 * (n + 1);
	if (qb)
	{
		k = strlen((char*)qb);
		c += k;
	}
	else
	{
		k = 0;
		if (qe)
			c += strlen((char*)qe);
	}
	b = buf = fmtbuf(c);
	if (qb)
	{
		q = qb[0] == '$' && qb[1] == '\'' && qb[2] == 0 ? 1 : 0;
		while ((*b = *qb++))
			b++;
		k = (flags & 1) ? 0 : (b - buf);
	}
	while (s < e)
	{
		c = *s++;
		if (iscntrl(c) || !isprint(c) || c == '\\')
		{
			k = 0;
			*b++ = '\\';
			switch (c)
			{
			case CC_bel:
				c = 'a';
				break;
			case '\b':
				c = 'b';
				break;
			case '\f':
				c = 'f';
				break;
			case '\n':
				c = 'n';
				break;
			case '\r':
				c = 'r';
				break;
			case '\t':
				c = 't';
				break;
			case CC_vt:
				c = 'v';
				break;
			case CC_esc:
				c = 'E';
				break;
			case '\\':
				break;
			default:
				if (!(flags & 2) || !(c & 0200))
				{
					*b++ = '0' + ((c >> 6) & 07);
					*b++ = '0' + ((c >> 3) & 07);
					c = '0' + (c & 07);
				}
				else
					b--;
				break;
			}
		}
		else if (qe && strchr(qe, c))
		{
			k = 0;
			*b++ = '\\';
		}
		else if (qb && isspace(c))
			k = q;
		*b++ = c;
	}
	if (qb && k <= q && qe)
		while ((*b = *qe++))
			b++;
	*b = 0;
	return buf + k;
}

/*
 * escape the usual suspects and quote chars in qs
 * in length n string as
 */

char*
fmtnesq(const char* as, const char* qs, size_t n)
{
	return fmtquote(as, NiL, qs, n, 0);
}

/*
 * escape the usual suspects and quote chars in qs
 */

char*
fmtesq(const char* as, const char* qs)
{
	return fmtquote(as, NiL, qs, strlen((char*)as), 0);
}

/*
 * escape the usual suspects
 */

char*
fmtesc(const char* as)
{
	return fmtquote(as, NiL, NiL, strlen((char*)as), 0);
}
