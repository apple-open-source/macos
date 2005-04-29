#pragma prototyped

#include <ast.h>

/*
 * copy t into s, return a pointer to the end of s ('\0')
 */

char*
strcopy(register char* s, register const char* t)
{
	if (!t) return(s);
	while ((*s++ = *t++));
	return(--s);
}
