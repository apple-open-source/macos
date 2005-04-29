
#include	"vmhdr.h"
/*
 * return a copy of s using vmalloc
 */

char*
vmstrdup(Vmalloc_t* v, register const char* s)
{
	register char*	t;
	register int	n;

	return((t = vmalloc(v, n = strlen(s) + 1)) ? (char*)memcpy(t, s, n) : (char*)0);
}

