/*
 * Copyright (c) 1987, 1993
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
static char sccsid[] = "@(#)setenv.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/setenv.c,v 1.9 2002/03/22 21:53:10 obrien Exp $");

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <crt_externs.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <malloc/malloc.h>

#define	ZONE_OWNS_PTR(zone, ptr)	(malloc_zone_from_ptr((ptr)) == zone)

extern malloc_zone_t *__zone0;
extern void __malloc_check_env_name(const char *);

__private_extern__ char *__findenv(const char *, int *, char **);
__private_extern__ int __setenv(const char *, const char *, int, int, char ***, malloc_zone_t *);
__private_extern__ void __unsetenv(const char *, char **, malloc_zone_t *);

#ifndef BUILDING_VARIANT
/*
 * The copy flag may have 3 values:
 *  1 - make a copy of the name/value pair
 *  0 - take the name as a user-supplied name=value string
 * -1 - like 0, except we copy of the name=value string in name
 */
__private_extern__ int
__setenv(name, value, rewrite, copy, environp, envz)
	const char *name;
	const char *value;
	int rewrite, copy;
	char ***environp;
	malloc_zone_t *envz;
{
	char *c;
	int offset;

	if ((c = __findenv(name, &offset, *environp))) { /* find if already exists */
		char *e;
		if (!rewrite)
			return (0);
		/*
		 * In UNIX03, we can overwrite only if we allocated the
		 * string.  Then we can realloc it if it is too small.
		 */
		e = (*environp)[offset];
		if (copy > 0 && ZONE_OWNS_PTR(envz, e)) {
			size_t l_value = strlen(value);
			if (strlen(c) < l_value) {	/* old smaller; resize*/
				char *r;
				size_t len = c - e;
				if ((r = realloc(e, l_value + len + 1)) == NULL)
					return (-1);
				if (r != e) {
					(*environp)[offset] = r;
					c = r + len;
				}
			}
			while ( (*c++ = *value++) );
			return (0);
		}
	} else {					/* create new slot */
		int cnt;
		char **p;

		for (p = *environp, cnt = 0; *p; ++p, ++cnt);
		if (ZONE_OWNS_PTR(envz, *environp)) {	/* just increase size */
			p = (char **)realloc((char *)*environp,
			    (size_t)(sizeof(char *) * (cnt + 2)));
			if (!p)
				return (-1);
			*environp = p;
		}
		else {				/* get new space */
						/* copy old entries into it */
			p = malloc_zone_malloc(envz, (size_t)(sizeof(char *) * (cnt + 2)));
			if (!p)
				return (-1);
			bcopy(*environp, p, cnt * sizeof(char *));
			*environp = p;
		}
		(*environp)[cnt + 1] = NULL;
		offset = cnt;
	}
	/* For non Unix03, or UnixO3 setenv(), we make a copy of the user's
	 * strings.  For Unix03 putenv(), we put the string directly in
	 * the environment. */
	if (copy > 0) {
		for (c = (char *)name; *c && *c != '='; ++c);	/* no `=' in name */
		if (!((*environp)[offset] =			/* name + `=' + value */
		    malloc_zone_malloc(envz, (size_t)((int)(c - name) + strlen(value) + 2))))
			return (-1);
		for (c = (*environp)[offset]; (*c = *name++) && *c != '='; ++c);
		for (*c++ = '='; (*c++ = *value++); );
	} else {
		/* the legacy behavior copies the string */
		if (copy < 0) {
			size_t len = strlen(name);
			if((c = malloc_zone_malloc(envz, len + 1)) == NULL)
				return (-1);
			memcpy(c, name, len + 1);
			name = c;
		}
		/* if we malloc-ed the previous value, free it first */
		if ((*environp)[offset] != NULL && ZONE_OWNS_PTR(envz, (*environp)[offset]))
			free((*environp)[offset]);
		(*environp)[offset] = (char *)name;
	}
	return (0);
}

__private_extern__ void
__unsetenv(const char *name, char **environ, malloc_zone_t *envz)
{
	char **p;
	int offset;

	while (__findenv(name, &offset, environ)) { /* if set multiple times */
		/* if we malloc-ed it, free it first */
		if (ZONE_OWNS_PTR(envz, environ[offset]))
			free(environ[offset]);
		for (p = &environ[offset];; ++p)
			if (!(*p = *(p + 1)))
				break;
	}
}

/****************************************************************************/
/*
 * _allocenvstate -- SPI that creates a new state (opaque)
 */
void *
_allocenvstate(void)
{
	return (void *)malloc_create_zone(1000 /* unused */, 0 /* unused */);
}

/*
 * _copyenv -- SPI that copies a NULL-tereminated char * array in a newly
 * allocated buffer, compatible with the other SPI env routines.  If env
 * is NULL, a char * array composed of a single NULL is returned.  NULL
 * is returned on error.  (This isn't needed anymore, as __setenv will
 * automatically make a copy in the zone.)
 */
char **
_copyenv(char **env)
{
	char **p;
	int cnt = 1;

	if (env)
		for (p = env; *p; ++p, ++cnt);
	p = (char **)malloc((size_t)(sizeof(char *) * cnt));
	if (!p)
		return (NULL);
	if (env)
		bcopy(env, p, cnt * sizeof(char *));
	else
		*p = NULL;
	return p;
}

/*
 * _deallocenvstate -- SPI that frees all the memory associated with the state
 * and all allocated strings, including the environment array itself if it
 * was copied.
 */
int
_deallocenvstate(void *state)
{
	malloc_zone_t *envz;

	if (!(envz = (malloc_zone_t *)state) || envz == __zone0) {
		errno = EINVAL;
		return -1;
	}
	malloc_destroy_zone(envz);
	return 0;
}

/*
 * setenvp -- SPI using an arbitrary pointer to string array and an env state,
 * created by _allocenvstate().  Initial checking is not done.
 *
 *	Set the value of the environmental variable "name" to be
 *	"value".  If rewrite is set, replace any current value.
 */
int
_setenvp(const char *name, const char *value, int rewrite, char ***envp, void *state)
{
	/* insure __zone0 is set up */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
	return (__setenv(name, value, rewrite, 1, envp, (state ? (malloc_zone_t *)state : __zone0)));
}

/*
 * unsetenv(name) -- SPI using an arbitrary pointer to string array and an env
 * state, created by _allocenvstate().  Initial checking is not done.
 *
 *	Delete environmental variable "name".
 */
int
_unsetenvp(const char *name, char ***envp, void *state)
{
	/* insure __zone0 is set up */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
	__unsetenv(name, *envp, (state ? (malloc_zone_t *)state : __zone0));
	return 0;
}

#endif /* !BUILD_VARIANT */

/*
 * setenv --
 *	Set the value of the environmental variable "name" to be
 *	"value".  If rewrite is set, replace any current value.
 */
int
setenv(name, value, rewrite)
	const char *name;
	const char *value;
	int rewrite;
{
	/* no null ptr or empty str */
	if(name == NULL || *name == 0) {
		errno = EINVAL;
		return (-1);
	}

#if __DARWIN_UNIX03
	/* no '=' in name */
	if (strchr(name, '=')) {
		errno = EINVAL;
		return (-1);
	}
#endif /* __DARWIN_UNIX03 */

	if (*value == '=')			/* no `=' in value */
		++value;
	/* insure __zone0 is set up before calling __malloc_check_env_name */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
	__malloc_check_env_name(name); /* see if we are changing a malloc environment variable */
	return (__setenv(name, value, rewrite, 1, _NSGetEnviron(), __zone0));
}

/*
 * unsetenv(name) --
 *	Delete environmental variable "name".
 */
#if __DARWIN_UNIX03
int
#else /* !__DARWIN_UNIX03 */
void
#endif /* __DARWIN_UNIX03 */
unsetenv(name)
	const char *name;
{
#if __DARWIN_UNIX03
	/* no null ptr or empty str */
	if(name == NULL || *name == 0) {
		errno = EINVAL;
		return (-1);
	}

	/* no '=' in name */
	if (strchr(name, '=')) {
		errno = EINVAL;
		return (-1);
	}
	/* insure __zone0 is set up before calling __malloc_check_env_name */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0) {
		    errno = ENOMEM;
		    return (-1);
	    }
	}
#else /* !__DARWIN_UNIX03 */
	/* no null ptr or empty str */
	if(name == NULL || *name == 0)
		return;
	/* insure __zone0 is set up before calling __malloc_check_env_name */
	if (!__zone0) {
	    __zone0 = malloc_create_zone(0, 0);
	    if (!__zone0)
		    return;
	}
#endif /* __DARWIN_UNIX03 */
	__malloc_check_env_name(name); /* see if we are changing a malloc environment variable */
	__unsetenv(name, *_NSGetEnviron(), __zone0);
#if __DARWIN_UNIX03
	return 0;
#endif /* __DARWIN_UNIX03 */
}
