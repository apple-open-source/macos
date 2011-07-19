/* $XTermId: xstrings.c,v 1.37 2010/04/04 22:34:17 tom Exp $ */

/************************************************************

Copyright 2000-2009,2010 by Thomas E. Dickey

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
#include <stdio.h>
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

/*
 * Decode a hexadecimal string, returning the decoded string.
 * On return, 'next' points to the first character not part of the input.
 * The caller must free the result.
 */
char *
x_decode_hex(const char *source, const char **next)
{
    char *result = 0;
    int pass;
    size_t j, k;

    for (pass = 0; pass < 2; ++pass) {
	for (j = k = 0; isxdigit(CharOf(source[j])); ++j) {
	    if ((pass != 0) && (j & 1) != 0) {
		result[k++] = (char) ((x_hex2int(source[j - 1]) << 4)
				      | x_hex2int(source[j]));
	    }
	}
	*next = (source + j);
	if ((j & 1) == 0) {
	    if (pass) {
		result[k] = '\0';
	    } else {
		result = malloc(++j);
		if (result == 0)
		    break;	/* not enough memory */
	    }
	} else {
	    break;		/* must have an even number of digits */
	}
    }
    return result;
}

/*
 * Encode a string into hexadecimal, returning the encoded string.
 * The caller must free the result.
 */
char *
x_encode_hex(const char *source)
{
    size_t need = (strlen(source) * 2) + 1;
    char *result = malloc(need);

    if (result != 0) {
	unsigned j, k;
	for (j = k = 0; source[j] != '\0'; ++j) {
	    sprintf(result + k, "%02X", CharOf(source[j]));
	    k += 2;
	}
    }
    return result;
}

char *
x_getenv(const char *name)
{
    return x_strdup(x_nonempty(getenv(name)));
}

/*
 * Decode a single hex "nibble", returning the nibble as 0-15, or -1 on error.
 */ int
x_hex2int(int c)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    return -1;
}

/*
 * Check if the given string is nonnull/nonempty.  If so, return a pointer
 * to the beginning of its content, otherwise return null.
 */
String
x_nonempty(String s)
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

String
x_skip_blanks(String s)
{
    while (isspace(CharOf(*s)))
	++s;
    return s;
}

String
x_skip_nonblanks(String s)
{
    while (*s != '\0' && !isspace(CharOf(*s)))
	++s;
    return s;
}

int
x_strcasecmp(const char *s1, const char *s2)
{
    size_t len = strlen(s1);

    if (len != strlen(s2))
	return 1;

    return x_strncasecmp(s1, s2, (unsigned) len);
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
x_strindex(char *s1, const char *s2)
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
x_strtrim(const char *source)
{
    char *result;
    char *s;
    char *d;

    if (source != 0 && *source != '\0') {
	char *t = x_strdup(source);
	s = t;
	d = s;
	while (isspace(CharOf(*s)))
	    ++s;
	while ((*d++ = *s++) != '\0') {
	    ;
	}
	if (*t != '\0') {
	    s = t + strlen(t);
	    while (s != t && isspace(CharOf(s[-1]))) {
		*--s = '\0';
	    }
	}
	result = t;
    } else {
	result = x_strdup("");
    }
    return result;
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
