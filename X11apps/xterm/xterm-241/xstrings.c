/* $XTermId: xstrings.c,v 1.28 2008/12/30 17:10:37 tom Exp $ */

/* $XFree86: xc/programs/xterm/xstrings.c,v 1.10 2006/02/13 01:14:59 dickey Exp $ */

/************************************************************

Copyright 2000-2007,2008 by Thomas E. Dickey

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

char *
x_getenv(const char *name)
{
    return x_nonempty(getenv(name));
}

/*
 * Check if the given string is nonnull/nonempty.  If so, return a pointer
 * to the beginning of its content, otherwise return null.
 */
char *
x_nonempty(char *s)
{
    if (s != 0) {
	if (*s == '\0') {
	    s = 0;
	} else {
	    s = x_skip_blanks(s);
	    if (*s == '\0')
		s = 0;
	}
    }
    return s;
}

char *
x_skip_blanks(char *s)
{
    while (isspace(CharOf(*s)))
	++s;
    return s;
}

char *
x_skip_nonblanks(char *s)
{
    while (*s != '\0' && !isspace(CharOf(*s)))
	++s;
    return s;
}

int
x_strcasecmp(const char *s1, const char *s2)
{
    unsigned len = strlen(s1);

    if (len != strlen(s2))
	return 1;

    return x_strncasecmp(s1, s2, len);
}

int
x_strncasecmp(const char *s1, const char *s2, unsigned n)
{
    while (n-- != 0) {
	char c1 = x_toupper(*s1);
	char c2 = x_toupper(*s2);
	if (c1 != c2)
	    return 1;
	if (c1 == 0)
	    break;
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
	char *t = CastMallocN(char, strlen(s));
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
 * Trims leading/trailing spaces from a copy of the string.
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
	s = x_skip_blanks(s);
	while ((*d++ = *s++) != '\0') {
	    ;
	}
	if (*t != '\0') {
	    s = t + strlen(t);
	    while (s != t && isspace(CharOf(s[-1]))) {
		*--s = '\0';
	    }
	}
	base = t;
    }
    return base;
}

/*
 * Avoid using system locale for upper/lowercase conversion, since there are
 * a few locales where toupper(tolower(c)) != c.
 */
char
x_toupper(int ch)
{
    static char table[256];
    char result = table[CharOf(ch)];

    if (result == '\0') {
	unsigned n;
	const char *s = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (n = 0; n < sizeof(table); ++n) {
	    table[n] = (char) n;
	}
	for (n = 0; s[n] != '\0'; ++n) {
	    table[CharOf(s[n])] = s[n % 26];
	}
	result = table[CharOf(ch)];
    }

    return result;
}
