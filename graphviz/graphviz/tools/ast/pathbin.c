#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Bell Laboratories
 *
 * return current PATH
 */

#include <ast.h>

char*
pathbin(void)
{
	register char*	bin;

	static char*	val;

	if ((!(bin = getenv("PATH")) || !*bin) && !(bin = val))
	{
		bin = "/bin:/usr/bin:/usr/local/bin";
		val = bin;
	}
	return bin;
}
