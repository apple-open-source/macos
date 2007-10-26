/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)putenv.c	8.2 (Berkeley) 3/27/94";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/putenv.c,v 1.2 2002/03/22 21:53:10 obrien Exp $");

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <db.h>
#include <crt_externs.h>
#include <malloc/malloc.h>
#include <errno.h> 

extern malloc_zone_t *__zone0;
extern void __malloc_check_env_name(const char *);

__private_extern__ int __setenv(const char *, const char *, int, int, char ***, malloc_zone_t *);

#ifndef BUILDING_VARIANT
/*
 * _putenvp -- SPI using an arbitrary pointer to string array (the array must
 * have been created with malloc) and an env state, created by _allocenvstate().
 *	Returns ptr to value associated with name, if any, else NULL.
 */
int
_putenvp(char *str, char ***envp, void *state)
{
	/* insure __zone0 is set up */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
	return (__setenv(str, NULL, 1, 0, envp, (state ? (malloc_zone_t *)state : __zone0)));
}
#endif /* BUILDING_VARIANT */

int
putenv(str)
	char *str;
{
#if __DARWIN_UNIX03
	if (str == NULL || *str == 0 || index(str, '=') == NULL) {
		errno = EINVAL;
		return (-1);
	}
#else /* !__DARWIN_UNIX03 */
	if (index(str, '=') == NULL)
		return (-1);
#endif /* __DARWIN_UNIX03 */
	/* insure __zone0 is set up before calling __malloc_check_env_name */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
	__malloc_check_env_name(str); /* see if we are changing a malloc environment variable */
	return (__setenv(str, NULL, 1,
#if __DARWIN_UNIX03
		0,
#else /* !__DARWIN_UNIX03 */
		-1,
#endif /* __DARWIN_UNIX03 */
	_NSGetEnviron(), __zone0));
}
