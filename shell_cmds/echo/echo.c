/*-
 * Copyright (c) 1989, 1993
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

#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)echo.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/bin/echo/echo.c,v 1.18 2005/01/10 08:39:22 imp Exp $");

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void
flush_and_exit(void)
{
	if (fflush(stdout) != 0)
		err(1, "fflush");
	exit(0);
}

static char *
print_one_char(char *cur, int posix, int *bytes_len_out)
{
	char *next;
	wchar_t wc;
	int bytes_len = mbtowc(&wc, cur, MB_CUR_MAX);
	if (bytes_len <= 0) {
		putchar(*cur);
		bytes_len = 1;
		goto out;
	}

	/* If this is not an escape sequence, just print the character */
	if (wc != '\\') {
		putwchar(wc);
		goto out;
	}

	next = cur + bytes_len;

	if (!posix) {
		/* In non-POSIX mode, the only valid escape sequence is \c */
		if (*next == 'c') {
			flush_and_exit();
		} else {
			putchar(wc);
			goto out;
		}
	} else {
		cur = next;
		bytes_len = 1;
	}

	switch (*cur) {
		case 'a':
			putchar('\a');
			goto out;

		case 'b':
			putchar('\b');
			goto out;

		case 'c':
			flush_and_exit();

		case 'f':
			putchar('\f');
			goto out;

		case 'n':
			putchar('\n');
			goto out;

		case 'r':
			putchar('\r');
			goto out;

		case 't':
			putchar('\t');
			goto out;

		case 'v':
			putchar('\v');
			goto out;

		case '\\':
			putchar('\\');
			goto out;

		case '0': {
			int j = 0, num = 0;
			while ((*++cur >= '0' && *cur <= '7') &&
			       j++ < 3) {
				num <<= 3;
				num |= (*cur - '0');
			}
			putchar(num);
			--cur;
			goto out;
		}
		default:
			--cur;
			putchar(*cur);
			goto out;
	}

 out:
	if (bytes_len_out)
		*bytes_len_out = bytes_len;
	return cur;
}

int
main(int argc, char *argv[])
{
	int nflag = 0;
	int posix = (getenv("POSIXLY_CORRECT") != NULL || getenv("POSIX_PEDANTIC") != NULL);

	if (!posix && argv[1] && strcmp(argv[1], "-n") == 0)
		nflag = 1;

	for (int i = 0; i < argc; i++) {
		/* argv[0] == progname */
		int ignore_arg = (i == 0 || (i == 1 && nflag == 1));
		int last_arg = (i == (argc - 1));
		if (!ignore_arg) {
			char *cur = argv[i];
			size_t arg_len = strlen(cur);
			int bytes_len = 0;

			for (const char *end = cur + arg_len; cur < end; cur += bytes_len) {
				cur = print_one_char(cur, posix, &bytes_len);
			}
		}
		if (last_arg && !nflag)
			putchar('\n');
		else if (!last_arg && !ignore_arg)
			putchar(' ');

		if (fflush(stdout) != 0)
			err(1, "fflush");
	}

	return 0;
}
