/*-
 * Copyright (c) 2015-2016 Nuxi, https://nuxi.nl/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

char *
#ifdef __APPLE__
basename_r(const char *path, char *bname)
#else
(basename)(char *path)
#endif
{
#ifdef __APPLE__
	const char *ptr, *endp;
	size_t len;
#else
	char *ptr;
#endif
	/*
	 * If path is a null pointer or points to an empty string,
	 * basename() shall return a pointer to the string ".".
	 */
	if (path == NULL || *path == '\0') {
#ifdef __APPLE__
		bname[0] = '.';
		bname[1] = '\0';
		return (bname);
#else
		return (__DECONST(char *, "."));
#endif
	}
	/* Find end of last pathname component and null terminate it. */
	ptr = path + strlen(path);
	while (ptr > path + 1 && *(ptr - 1) == '/')
		--ptr;
#ifdef __APPLE__
	endp = ptr--;
#else
	*ptr-- = '\0';
#endif
	/* Find beginning of last pathname component. */
	while (ptr > path && *(ptr - 1) != '/')
		--ptr;
#ifdef __APPLE__
	len = endp - ptr;
	if (len >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
	memcpy(bname, ptr, len);
	bname[len] = '\0';
	return (bname);
#else
	return (ptr);
#endif
}

#ifdef __APPLE__
#if __DARWIN_UNIX03
#define const /**/
#endif

char *
basename(const char *path)
{
	static char *bname = NULL;

	if (bname == NULL) {
		bname = (char *)malloc(MAXPATHLEN);
		if (bname == NULL)
			return (NULL);
	}
	return (basename_r(path, bname));
}
#endif
