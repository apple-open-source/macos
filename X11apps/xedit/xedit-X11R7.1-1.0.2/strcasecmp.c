/* $XFree86: xc/programs/xedit/strcasecmp.c,v 1.1 1999/06/14 02:38:07 dawes Exp $ */

#include <ctype.h>
#include <sys/types.h>

#ifndef LISP
#include "xedit.h"
#endif

/* Just like the BSD version.  It assumes that tolower() is ANSI-compliant */

int
strcasecmp(const char *s1, const char *s2)
{
	const unsigned char *us1 = (const unsigned char *)s1;
	const unsigned char *us2 = (const unsigned char *)s2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return 0;
	return tolower(*us1) - tolower(*--us2);
}

int
strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (n != 0) {
		const unsigned char *us1 = (const unsigned char *)s1;
		const unsigned char *us2 = (const unsigned char *)s2;

		do {
			if (tolower(*us1) != tolower(*us2++))
				return tolower(*us1) - tolower(*--us2);
			if (*us1++ == '\0')
				break;
		} while (--n != 0);
	}
	return 0;
}

