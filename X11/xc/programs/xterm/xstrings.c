/* $XFree86: xc/programs/xterm/xstrings.c,v 1.7 2003/11/13 01:16:38 dickey Exp $ */

/************************************************************

Copyright 2000-2002,2003 by Thomas E. Dickey

                        All Rights Reserved

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization.

********************************************************/

#include <xterm.h>

#include <sys/types.h>
#include <string.h>
#include <ctype.h>

#include <xstrings.h>

char *
x_basename(char *name)
{
    char *cp;

    cp = strrchr(name, '/');
#ifdef __UNIXOS2__
    if (cp == 0)
	cp = strrchr(name, '\\');
#endif
    return (cp ? cp + 1 : name);
}

int
x_strcasecmp(const char *s1, const char *s2)
{
    unsigned len = strlen(s1);

    if (len != strlen(s2))
	return 1;

    while (len-- != 0) {
	if (toupper(CharOf(*s1)) != toupper(CharOf(*s2)))
	    return 1;
	s1++, s2++;
    }

    return 0;
}

/*
 * Allocates a copy of a string
 */
char *
x_strdup(const char *s)
{
    char *result = 0;

    if (s != 0) {
	char *t = (char *) malloc(strlen(s) + 1);
	if (t != 0) {
	    strcpy(t, s);
	}
	result = t;
    }
    return result;
}

/*
 * Returns a pointer to the first occurrence of s2 in s1,
 * or NULL if there are none.
 */
char *
x_strindex(char *s1, char *s2)
{
    char *s3;
    size_t s2len = strlen(s2);

    while ((s3 = strchr(s1, *s2)) != NULL) {
	if (strncmp(s3, s2, s2len) == 0)
	    return (s3);
	s1 = ++s3;
    }
    return (NULL);
}

/*
 * Trims leading/trailing spaces from the string, returns a copy of it if it
 * is modified.
 */
char *
x_strtrim(char *s)
{
    char *base = s;
    char *d;

    if (s != 0 && *s != '\0') {
	char *t = x_strdup(base);
	s = t;
	d = s;
	while (isspace(CharOf(*s))) {
	    ++s;
	}
	while ((*d++ = *s++) != '\0') {
	    ;
	}
	if (*t != '\0') {
	    s = t + strlen(t);
	    while (s != t && isspace(CharOf(s[-1]))) {
		*--s = '\0';
	    }
	}
	if (!strcmp(t, base)) {
	    free(t);
	} else {
	    base = t;
	}
    }
    return base;
}
