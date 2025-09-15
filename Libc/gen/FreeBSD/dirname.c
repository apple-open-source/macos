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
dirname_r(const char *path, char *dname)
#else
dirname(char *path)
#endif
{
#ifdef __APPLE__
	const char *end;
	size_t len;
#else
	char *end;
#endif

	/*
	 * If path is a null pointer or points to an empty string,
	 * dirname() shall return a pointer to the string ".".
	 */
#ifdef __APPLE__
	if (path == NULL || *path == '\0') {
		dname[0] = '.';
		dname[1] = '\0';
		return (dname);
	}
#else
	if (path == NULL || *path == '\0')
		return (__DECONST(char *, "."));
#endif
	/* Find end of last pathname component. */
	end = path + strlen(path);
	while (end > path + 1 && *(end - 1) == '/')
		--end;

	/* Strip off the last pathname component. */
	while (end > path && *(end - 1) != '/')
		--end;

	/*
	 * If dname does not contain a '/', then dirname() shall return a
	 * pointer to the string ".".
	 */
	if (end == path) {
#ifdef __APPLE__
		dname[0] = '.';
		dname[1] = '\0';
		return (dname);
#else
		path[0] = '.';
		path[1] = '\0';
		return (path);
#endif
	}

	/*
	 * Remove trailing slashes from the resulting directory name. Ensure
	 * that at least one character remains.
	 */
	while (end > path + 1 && *(end - 1) == '/')
		--end;

#ifdef __APPLE__
	len = end - path;
	if (len >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return (NULL);
	}
#endif
	/* Null terminate directory name and return it. */
#ifdef __APPLE__
	memmove(dname, path, len);
	dname[len] = '\0';
	return (dname);
#else
	*end = '\0';
	return (path);
#endif
}

#ifdef __APPLE__
#if __DARWIN_UNIX03
#define const /**/
#endif

char *
dirname(const char *path)
{
	static char *dname = NULL;

	if (dname == NULL) {
		dname = (char *)malloc(MAXPATHLEN);
		if (dname == NULL)
			return (NULL);
	}
	return (dirname_r(path, dname));
}
#endif
