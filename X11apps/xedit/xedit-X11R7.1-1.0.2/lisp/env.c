/*
 * Provide setenv() and unsetenv() on platforms that don't have them.
 * From FreeBSD's libc.
 */

/*
 * Copyright (c) 1987, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $XFree86$ */


#include <stdlib.h>
#include <stddef.h>
#include <string.h>

extern char **environ;

extern int setenv(const char *name, const char *value, int overwrite);
extern void unsetenv(const char *name);

static char *
findenv(const char *name, int *offset)
{
    int len, i;
    const char *np;
    char **p, *cp;

    if (name == NULL || environ == NULL)
	return NULL;

    for (np = name; *np && *np != '='; ++np)
	continue;
    len = np - name;
    for (p = environ; (cp = *p) != NULL; ++p) {
	for (np = name, i = len; i && *cp; i--)
	    if (*cp++ != *np++)
		break;
	if (i == 0 && *cp++ == '=') {
	    *offset = p - environ;
	    return cp;
	}
    }
    return NULL;
}

/*
 * setenv --
 *      Set the value of the environmental variable "name" to be
 *      "value".  If overwrite is set, replace any current value.
 */

int
setenv(const char *name, const char *value, int overwrite)
{
    static char **alloced;			/* if allocated space before */
    char *c;
    int l_value, offset;

    if (*value == '=')				/* no '=' in value */
	++value;
    l_value = strlen(value);
    if ((c = findenv(name, &offset))) {		/* find if already exists */
	if (!overwrite)
	    return 0;
	if (strlen(c) >= l_value) {		/* old larger; copy over */
	    while ((*c++ = *value++))
		;
	    return 0;
	}
    } else {					/* create new slot */
	int cnt;
	char **p;

	for (p = environ, cnt = 0; *p; ++p, ++cnt)
	    ;
	if (alloced == environ) {		/* just increase size */
	    p = (char **)realloc((char *)environ,
				 sizeof(char *) * (cnt + 2));
	    if (!p)
		return -1;
	    alloced = environ = p;
	} else {				/* get new space */
						/* copy old entries into it */
	    p = malloc(sizeof(char *) * (cnt + 2));
	    if (!p)
		return -1;
	    memcpy(p, environ, cnt * sizeof(char *));
	    alloced = environ = p;
	}
	environ[cnt + 1] = NULL;
	offset = cnt;
    }
    for (c = (char *)name; *c && *c != '='; ++c)	/* no '=' in name */
	;
    if (!(environ[offset] =				/* name + '=' + value */
			malloc((int)(c - name) + l_value + 2)))
	return -1;
    for (c = environ[offset]; (*c = *name++) && *c != '='; ++c)
	;
    for (*c++ = '='; (*c++ = *value++); )
	;
    return 0;
}

/*
 * unsetenv(name) --
 *      Delete environmental variable "name".
 */

void
unsetenv(const char *name)
{
    char **p;
    int offset;

    while (findenv(name, &offset))		/* if set multiple times */
	for (p = &environ[offset];; ++p)
	    if (!(*p = *(p + 1)))
		break;
}

