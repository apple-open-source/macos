/*
 * Copyright (c) Christos Zoulas 2003.
 * All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "magic.h"
#ifdef __APPLE__
#include "../src/file.h"
static const char *
magic_buffer_name(struct magic_set *ms, const void *buf, size_t nb,
    const char *inname)
{
	if (ms == NULL)
		return NULL;
	if (file_reset(ms, 1) == -1)
		return NULL;
	/*
	 * The main work is done here!
	 * We have the file name and/or the data buffer to be identified.
	 */
	if (file_buffer(ms, -1, NULL, inname, buf, nb) == -1) {
		return NULL;
	}
	return file_getbuffer(ms);
}
static unsigned int
hex(char ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0' + 0x0);
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A' + 0xA);
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 0xa);
	return (ch);
}
#endif /* __APPLE__ */

static const char *prog;

static void *
xrealloc(void *p, size_t n)
{
	p = realloc(p, n);
	if (p == NULL) {
		(void)fprintf(stderr, "%s ERROR slurping file: %s\n",
			prog, strerror(errno));
		exit(10);
	}
	return p;
}

static char *
slurp(FILE *fp, size_t *final_len)
{
	size_t len = 256;
	int c;
	char *l = (char *)xrealloc(NULL, len), *s = l;

	for (c = getc(fp); c != EOF; c = getc(fp)) {
		if (s == l + len) {
			l = xrealloc(l, len * 2);
			s = l + len;
			len *= 2;
		}
		*s++ = c;
	}
	if (s == l + len) {
		l = (char *)xrealloc(l, len + 1);
		s = l + len;
	}
	*s++ = '\0';

	*final_len = s - l;
	l = (char *)xrealloc(l, s - l);
	return l;
}

int
main(int argc, char **argv)
{
	struct magic_set *ms;
	const char *result;
	size_t result_len, desired_len;
	char *desired = NULL;
	int e = EXIT_FAILURE;
	FILE *fp;


	prog = strrchr(argv[0], '/');
	if (prog)
		prog++;
	else
		prog = argv[0];

	ms = magic_open(MAGIC_NONE);
	if (ms == NULL) {
		(void)fprintf(stderr, "%s: ERROR opening MAGIC_NONE: %s\n",
		    prog, strerror(errno));
		return e;
	}
	if (magic_load(ms, NULL) == -1) {
		(void)fprintf(stderr, "%s: ERROR loading with NULL file: %s\n",
		    prog, magic_error(ms));
		goto bad;
	}

	if (argc == 1) {
		e = 0;
		goto bad;
	}

	if (argc != 3) {
		(void)fprintf(stderr, "Usage: %s TEST-FILE RESULT\n", prog);
		magic_close(ms);
		goto bad;
	}
#ifdef __APPLE__
	char *buf;
	size_t len, namelen = strlen(argv[1]);
	if (namelen > 5 && strcmp(argv[1] + namelen - 4, ".hex") == 0) {
		if ((fp = fopen(argv[1], "r")) == NULL ||
		    (buf = slurp(fp, &len)) == NULL ||
		    ferror(fp)) {
			(void)fprintf(stderr, "%s: ERROR loading file %s: %s\n",
			    prog, argv[1], strerror(errno));
			if (fp != NULL)
				fclose(fp);
			goto bad;
		}
		fclose(fp);
		for (unsigned int i = 0; i < len / 2; i++) {
			((unsigned char *)buf)[i] =
			    hex(buf[i * 2]) << 4 | hex(buf[i * 2 + 1]);
		}
		argv[1][namelen - 4] = '\0';
		result = magic_buffer_name(ms, buf, len / 2, argv[1]);
		if (result == NULL) {
			(void)fprintf(stderr, "%s: ERROR loading file %s: %s\n",
			    prog, argv[1], magic_error(ms));
			goto bad;
		}
	} else
#endif /* __APPLE__ */
	if ((result = magic_file(ms, argv[1])) == NULL) {
		(void)fprintf(stderr, "%s: ERROR loading file %s: %s\n",
		    prog, argv[1], magic_error(ms));
		goto bad;
	}
	fp = fopen(argv[2], "r");
	if (fp == NULL) {
		(void)fprintf(stderr, "%s: ERROR opening `%s': %s",
		    prog, argv[2], strerror(errno));
		goto bad;
	}
	desired = slurp(fp, &desired_len);
	fclose(fp);
	(void)printf("%s: %s\n", argv[1], result);
	if (strcmp(result, desired) != 0) {
	    result_len = strlen(result);
	    (void)fprintf(stderr, "%s: ERROR: result was (len %zu)\n%s\n"
		"expected (len %zu)\n%s\n", prog, result_len, result,
		desired_len, desired);
	    goto bad;
	}
	e = 0;
bad:
	free(desired);
	magic_close(ms);
	return e;
}
