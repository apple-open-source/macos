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
static char sccsid[] = "@(#)getenv.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/stdlib/getenv.c,v 1.8 2007/05/01 16:02:41 ache Exp $");

#include <errno.h>
#include <os/lock_private.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <crt_externs.h>

#include "libc_private.h"

__private_extern__ char *__findenv_locked(const char *, int *, char **);

/*
 * __findenv_locked --
 *	Returns pointer to value associated with name, if any, else NULL.
 *	Sets offset to be the offset of the name/value combination in the
 *	environmental array, for use by setenv(3) and unsetenv(3).
 *	Explicitly removes '=' in argument name.
 *
 *	This routine *should* be a static; don't use it.
 */
__private_extern__ char *
__findenv_locked(const char *name, int *offset, char **environ)
{
	int len, i;
	const char *np;
	char **p, *cp;

	if (name == NULL || environ == NULL)
		return (NULL);
	for (np = name; *np && *np != '='; ++np)
		continue;
	len = np - name;
	for (p = environ; (cp = *p) != NULL; ++p) {
		for (np = name, i = len; i && *cp; i--)
			if (*cp++ != *np++)
				break;
		if (i == 0 && *cp++ == '=') {
			*offset = p - environ;
			return (cp);
		}
	}
	return (NULL);
}

static os_unfair_lock __environ_lock_obj = OS_UNFAIR_LOCK_INIT;
void
environ_lock_np(void)
{
	os_unfair_lock_lock_with_options(
			&__environ_lock_obj, OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION);
}
void
environ_unlock_np(void)
{
	os_unfair_lock_unlock(&__environ_lock_obj);
}
__private_extern__ void
__environ_lock_fork_child(void)
{
	__environ_lock_obj = OS_UNFAIR_LOCK_INIT;
}

/*
 * _getenvp -- SPI using an arbitrary pointer to string array (the array must
 * have been created with malloc) and an env state, created by _allocenvstate().
 *	Returns ptr to value associated with name, if any, else NULL.
 */
char *
_getenvp(const char *name, char ***envp, void *state __unused)
{
	// envp is passed as an argument, so the lock is not protecting everything
	int offset;
	environ_lock_np();
	char *result = (__findenv_locked(name, &offset, *envp));
	environ_unlock_np();
	return result;
}

/*
 * getenv --
 *	Returns ptr to value associated with name, if any, else NULL.
 */
char *
getenv(const char *name)
{
	int offset;
	environ_lock_np();
	char *result = __findenv_locked(name, &offset, *_NSGetEnviron());
	environ_unlock_np();
	return result;
}

/*
 * getenv_copy_np --
 *	Returns ptr to copy of value associated with name, if any, else NULL.
 *	Caller is responsible for freeing the return value.
 *	Sets errno == ENOMEM and returns NULL if memory allocation fails.
 *	Sets errno == EAGAIN and returns NULL if 5 attempts to copy the
 *	string fails.
 */
char *
getenv_copy_np(const char *name)
{
	/*
	 * malloc() calls getenv() (which takes the environ_lock_np()), so
	 * strdup() can not be called with environ_lock_np() held. Instead,
	 * perform a two-step check for the length of the value (under lock),
	 * then allocate a buffer, then try to copy the value into the buffer
	 * (under lock). If the value of the environment variable changed such
	 * that it no longer fits in the buffer, then try again up to four more
	 * times. This algorithm is prescribed when using getenv_s() (ISO-C
	 * Annex K), so this function abstracts that from the user since it is
	 * difficult to implement correctly.
	 */
	unsigned attempts = 0;
	char *result_buffer = NULL;
	size_t result_buffer_length = 0;
	int unused_offset;
	char *value;
	size_t value_length;

	/* 1. Get the length of the value. */
	environ_lock_np();
	value = __findenv_locked(name, &unused_offset, *_NSGetEnviron());
	value_length = value ? strlen(value) : 0;
	environ_unlock_np();

	if (!value)
		return NULL;

	do {
		if (++attempts > 5) {
			errno = EAGAIN;
			return NULL;
		}

		/* 2. Allocate a buffer for the copy.  Round up to nearest/next 16 bytes. */
		result_buffer_length = value_length + 1;
		result_buffer_length += (16 - (result_buffer_length % 16));
		result_buffer = (char *)reallocf((void *)result_buffer, result_buffer_length);
		if (result_buffer == NULL) {
			/* reallocf() sets errno = ENOMEM on failure. */
			return NULL;
		}

		/* 3. Get the variable again and make a copy. */
		environ_lock_np();
		value = __findenv_locked(name, &unused_offset, *_NSGetEnviron());
		value_length = value ? strlcpy(result_buffer, value, result_buffer_length) : 0;
		environ_unlock_np();

		if (!value)
			return NULL;

		/* 4. If value was too long for buffer, try again. */
	} while (value_length >= result_buffer_length);

	return result_buffer;
}
