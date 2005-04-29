#pragma prototyped
/*
 * Glenn Fowler
 * AT&T Bell Laboratories
 *
 * single dir support for pathaccess()
 */

#include <ast.h>

char*
pathcat(char* path, register const char* dirs, int sep, const char* a, register const char* b)
{
	register char*	s;

	s = path;
	while (*dirs && *dirs != sep) *s++ = *dirs++;
	if (s != path) *s++ = '/';
	if (a)
	{
		while ((*s = *a++)) s++;
		if (b) *s++ = '/';
	}
	else if (!b) b = ".";
	if (b) while ((*s++ = *b++));
	return(*dirs ? (char*)++dirs : 0);
}
