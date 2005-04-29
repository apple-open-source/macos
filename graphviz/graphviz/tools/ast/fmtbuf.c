#pragma prototyped

#include <ast.h>

/*
 * return small format buffer chunk of size n
 * spin lock for thread access
 * format buffers are short lived
 */

static char		buf[16 * 1024];
static char*		nxt = buf;
static int		lck = -1;

char*
fmtbuf(size_t n)
{
	register char*	cur;

	while (++lck)
		lck--;
	if (n > (&buf[elementsof(buf)] - nxt))
		nxt = buf;
	cur = nxt;
	nxt += n;
	lck--;
	return cur;
}
