#include <sys/cdefs.h>
#ifndef lint
#ifndef NOID
__unused static const char	elsieid[] = "@(#)ialloc.c	8.29";
#endif /* !defined NOID */
#endif /* !defined lint */

#ifndef lint
__unused static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/zic/ialloc.c,v 1.6 2000/11/28 18:18:56 charnier Exp $";
#endif /* not lint */

/*LINTLIBRARY*/

#include "private.h"

#define nonzero(n)	(((n) == 0) ? 1 : (n))

char *
imalloc(const size_t n)
{
	return malloc(nonzero(n));
}

char *
icalloc(size_t nelem, size_t elsize)
{
	if (nelem == 0 || elsize == 0)
		nelem = elsize = 1;
	return calloc(nelem, elsize);
}

void *
irealloc(void * const pointer, const size_t size)
{
	if (pointer == NULL)
		return imalloc(size);
	return realloc((void *) pointer, nonzero(size));
}

char *
icatalloc(char * const old, const char * const new)
{
	char *	result;
	size_t	oldsize, newsize;

	newsize = (new == NULL) ? 0 : strlen(new);
	if (old == NULL)
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else	oldsize = strlen(old);
	if ((result = irealloc(old, oldsize + newsize + 1)) != NULL)
		if (new != NULL)
			(void) strcpy(result + oldsize, new);
	return result;
}

char *
icpyalloc(const char * const string)
{
	return icatalloc((char *) NULL, string);
}

void
ifree(char * const p)
{
	if (p != NULL)
		(void) free(p);
}

void
icfree(char * const p)
{
	if (p != NULL)
		(void) free(p);
}
