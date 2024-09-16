/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * uudecode [file ...]
 *
 * create the specified file, decoding as you go.
 * used with uuencode.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#ifdef __APPLE__
#include <assert.h>
#endif
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <resolv.h>
#include <stdbool.h>
#ifdef __APPLE__
#include <stddef.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include "bintrans.h"
#endif

extern int main_decode(int, char *[]);
#ifdef __APPLE__
extern int main_base64_decode(const char *, const char *);
#else
extern int main_base64_decode(const char *);
#endif

#ifdef __APPLE__
#include <get_compat.h>
#endif

static const char *infile, *outfile;
static FILE *infp, *outfp;
static bool base64, cflag, iflag, oflag, pflag, rflag, sflag;
#ifdef __APPLE__
static bool unix2003compat;
#endif


static void	usage(void);
static int	decode(void);
static int	decode2(void);
static int	uu_decode(void);
static int	base64_decode(void);

int
#ifdef __APPLE__
main_base64_decode(const char *in, const char *out)
#else
main_base64_decode(const char *in)
#endif
{
	base64 = 1;
	rflag = 1;
#ifdef __APPLE__
	if (in != NULL && strcmp(in, "-") != 0) {
#else
	if (in != NULL) {
#endif
		infile = in;
		infp = fopen(infile, "r");
		if (infp == NULL)
			err(1, "%s", in);
	} else {
		infile = "stdin";
		infp = stdin;
	}
#ifdef __APPLE__
	if (out != NULL) {
		if (strcmp(out, "-") == 0)
			outfp = stdout;
		else if ((outfp = fopen(out, "w+")) == NULL)
			err(1, "%s", out);
	}
#endif
	exit(decode());
}

int
main_decode(int argc, char *argv[])
{
	int rval, ch;

	if (strcmp(basename(argv[0]), "b64decode") == 0)
		base64 = true;

	while ((ch = getopt(argc, argv, "cimo:prs")) != -1) {
		switch (ch) {
		case 'c':
			if (oflag || rflag)
				usage();
			cflag = true; /* multiple uudecode'd files */
			break;
		case 'i':
			iflag = true; /* ask before override files */
			break;
		case 'm':
			base64 = true;
			break;
		case 'o':
			if (cflag || pflag || rflag || sflag)
				usage();
			oflag = true; /* output to the specified file */
			sflag = true; /* do not strip pathnames for output */
			outfile = optarg; /* set the output filename */
			break;
		case 'p':
			if (oflag)
				usage();
			pflag = true; /* print output to stdout */
			break;
		case 'r':
			if (cflag || oflag)
				usage();
			rflag = true; /* decode raw data */
			break;
		case 's':
			if (oflag)
				usage();
			sflag = true; /* do not strip pathnames for output */
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

#ifdef __APPLE__
	unix2003compat = COMPAT_MODE("bin/uudecode", "Unix2003");
#endif

	if (*argv != NULL) {
		rval = 0;
		do {
			infp = fopen(infile = *argv, "r");
			if (infp == NULL) {
				warn("%s", *argv);
				rval = 1;
				continue;
			}
			rval |= decode();
			fclose(infp);
		} while (*++argv);
	} else {
		infile = "stdin";
		infp = stdin;
		rval = decode();
	}
	exit(rval);
}

static int
decode(void)
{
	int r, v;

	if (rflag) {
		/* relaxed alternative to decode2() */
#ifdef __APPLE__
		if (outfp == NULL) {
			outfile = "/dev/stdout";
			outfp = stdout;
		}
#else
		outfile = "/dev/stdout";
		outfp = stdout;
#endif
		if (base64)
			return (base64_decode());
		else
			return (uu_decode());
	}
	v = decode2();
	if (v == EOF) {
		warnx("%s: missing or bad \"begin\" line", infile);
		return (1);
	}
	for (r = v; cflag; r |= v) {
		v = decode2();
		if (v == EOF)
			break;
	}
	return (r);
}

static int
decode2(void)
{
	int flags, fd, mode;
	size_t n, m;
	char *p, *q;
	void *handle;
	struct passwd *pw;
	struct stat st;
	char buf[MAXPATHLEN + 1];

	base64 = false;
	/* search for header line */
	for (;;) {
		if (fgets(buf, sizeof(buf), infp) == NULL)
			return (EOF);
		p = buf;
		if (strncmp(p, "begin-base64 ", 13) == 0) {
			base64 = true;
			p += 13;
		} else if (strncmp(p, "begin ", 6) == 0)
			p += 6;
		else
			continue;
		/* p points to mode */
		q = strchr(p, ' ');
		if (q == NULL)
			continue;
		*q++ = '\0';
		/* q points to filename */
		n = strlen(q);
		while (n > 0 && (q[n-1] == '\n' || q[n-1] == '\r'))
			q[--n] = '\0';
		/* found valid header? */
		if (n > 0)
			break;
	}

	handle = setmode(p);
	if (handle == NULL) {
		warnx("%s: unable to parse file mode", infile);
		return (1);
	}
	mode = getmode(handle, 0)
#ifndef __APPLE__
	    & 0666
#endif
	    ;
	free(handle);

#ifdef __APPLE__
	/* POSIX says "/dev/stdout" is a 'magic cookie' not a special file. */
	if (strcmp(q, "/dev/stdout") == 0)
		outfp = stdout;
#endif

	if (sflag) {
		/* don't strip, so try ~user/file expansion */
		p = NULL;
		pw = NULL;
		if (*q == '~')
			p = strchr(q, '/');
		if (p != NULL) {
			*p = '\0';
			pw = getpwnam(q + 1);
			*p = '/';
		}
		if (pw != NULL) {
			n = strlen(pw->pw_dir);
			if (buf + n > p) {
				/* make room */
				m = strlen(p);
				if (sizeof(buf) < n + m) {
					warnx("%s: bad output filename",
					    infile);
					return (1);
				}
				p = memmove(buf + n, p, m);
			}
			q = memcpy(p - n, pw->pw_dir, n);
		}
	} else {
		/* strip down to leaf name */
		p = strrchr(q, '/');
		if (p != NULL)
			q = p + 1;
	}
	if (!oflag)
		outfile = q;

#ifdef __APPLE__
	if (!oflag && outfp != NULL) {
	} else
#endif
	/* POSIX says "/dev/stdout" is a 'magic cookie' not a special file. */
	if (pflag || strcmp(outfile, "/dev/stdout") == 0)
		outfp = stdout;
	else {
		flags = O_WRONLY | O_CREAT | O_EXCL;
		if (lstat(outfile, &st) == 0) {
			if (iflag
#ifdef __APPLE__
			    && !S_ISFIFO(st.st_mode)
#endif
			    ) {
				warnc(EEXIST, "%s: %s", infile, outfile);
				return (0);
			}
			switch (st.st_mode & S_IFMT) {
			case S_IFREG:
#ifdef __APPLE__
				flags |= O_NOFOLLOW | O_TRUNC;
				flags &= ~O_EXCL;
				break;
#endif
			case S_IFLNK:
				/* avoid symlink attacks */
#ifdef __APPLE__
				/*
				 * Section 2.9.1.4, P1003.3.2/D8 mandates
				 * following symlink.
				 */
				if (unix2003compat) {
					flags |= O_TRUNC;
					flags &= ~O_EXCL;
					break;
				}
#endif

				if (unlink(outfile) == 0 || errno == ENOENT)
					break;
				warn("%s: unlink %s", infile, outfile);
				return (1);
			case S_IFDIR:
				warnc(EISDIR, "%s: %s", infile, outfile);
				return (1);
#ifdef __APPLE__
			case S_IFIFO:
				flags &= ~O_EXCL;
				break;
#endif
			default:
				if (oflag) {
					/* trust command-line names */
					flags &= ~O_EXCL;
					break;
				}
				warnc(EEXIST, "%s: %s", infile, outfile);
				return (1);
			}
		} else if (errno != ENOENT) {
			warn("%s: %s", infile, outfile);
			return (1);
		}
		if ((fd = open(outfile, flags, mode)) < 0 ||
		    (outfp = fdopen(fd, "w")) == NULL) {
			warn("%s: %s", infile, outfile);
			return (1);
		}
#ifdef __APPLE__
		if (fchmod(fileno(outfp), mode) && EPERM != errno) {
			warn("%s: %s", infile, outfile);
			close(fd);
			return 1;
		}
#endif
	}

	if (base64)
		return (base64_decode());
	else
		return (uu_decode());
}

static int
get_line(char *buf, size_t size)
{

#ifdef __APPLE__
	assert(size <= INT_MAX);
	if (fgets(buf, (int)size, infp) != NULL)
#else
	if (fgets(buf, size, infp) != NULL)
#endif
		return (2);
	if (rflag)
		return (0);
	warnx("%s: %s: short file", infile, outfile);
	return (1);
}

static int
checkend(const char *ptr, const char *end, const char *msg)
{
	size_t n;

	n = strlen(end);
	if (strncmp(ptr, end, n) != 0 ||
	    strspn(ptr + n, " \t\r\n") != strlen(ptr + n)) {
		warnx("%s: %s: %s", infile, outfile, msg);
		return (1);
	}
	return (0);
}

static int
checkout(int rval)
{
	if (fflush(outfp) != 0) {
		warn("%s: %s", infile, outfile);
		rval = 1;
	}
	if (outfp != stdout) {
		(void)fclose(outfp);
		outfp = stdout;
	}
	outfile = "/dev/stdout";
	return (rval);
}

static int
uu_decode(void)
{
	int i, ch;
	char *p;
	char buf[MAXPATHLEN+1];

	/* for each input line */
	for (;;) {
		switch (get_line(buf, sizeof(buf))) {
		case 0:
			return (checkout(0));
		case 1:
			return (checkout(1));
		}

#define	DEC(c)		(((c) - ' ') & 077)	/* single character decode */
#define IS_DEC(c)	 ( (((c) - ' ') >= 0) && (((c) - ' ') <= 077 + 1) )

#define OUT_OF_RANGE do {						\
	warnx("%s: %s: character out of range: [%d-%d]",		\
	    infile, outfile, ' ', 077 + ' ' + 1);			\
	return (1);							\
} while (0)

		/*
		 * `i' is used to avoid writing out all the characters
		 * at the end of the file.
		 */
		p = buf;
		if ((i = DEC(*p)) <= 0)
			break;
		for (++p; i > 0; p += 4, i -= 3)
			if (i >= 3) {
				if (!(IS_DEC(*p) && IS_DEC(*(p + 1)) &&
				    IS_DEC(*(p + 2)) && IS_DEC(*(p + 3))))
					OUT_OF_RANGE;

				ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
				putc(ch, outfp);
				ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
				putc(ch, outfp);
				ch = DEC(p[2]) << 6 | DEC(p[3]);
				putc(ch, outfp);
			} else {
				if (i >= 1) {
					if (!(IS_DEC(*p) && IS_DEC(*(p + 1))))
						OUT_OF_RANGE;
					ch = DEC(p[0]) << 2 | DEC(p[1]) >> 4;
					putc(ch, outfp);
				}
				if (i >= 2) {
					if (!(IS_DEC(*(p + 1)) &&
					    IS_DEC(*(p + 2))))
						OUT_OF_RANGE;

					ch = DEC(p[1]) << 4 | DEC(p[2]) >> 2;
					putc(ch, outfp);
				}
				if (i >= 3) {
					if (!(IS_DEC(*(p + 2)) &&
					    IS_DEC(*(p + 3))))
						OUT_OF_RANGE;
					ch = DEC(p[2]) << 6 | DEC(p[3]);
					putc(ch, outfp);
				}
			}
	}
	switch (get_line(buf, sizeof(buf))) {
	case 0:
		return (checkout(0));
	case 1:
		return (checkout(1));
	default:
		return (checkout(checkend(buf, "end", "no \"end\" line")));
	}
}

static int
base64_decode(void)
{
#ifdef __APPLE__
	ptrdiff_t count4;
	int n, count;
#else
	int n, count, count4;
#endif
	char inbuf[MAXPATHLEN + 1], *p;
	unsigned char outbuf[MAXPATHLEN * 4];
	char leftover[MAXPATHLEN + 1];

	leftover[0] = '\0';
	for (;;) {
		strcpy(inbuf, leftover);
		switch (get_line(inbuf + strlen(inbuf),
		    sizeof(inbuf) - strlen(inbuf))) {
		case 0:
			return (checkout(0));
		case 1:
			return (checkout(1));
		}

		count = 0;
		count4 = -1;
		p = inbuf;
		while (*p != '\0') {
			/*
			 * Base64 encoded strings have the following
			 * characters in them: A-Z, a-z, 0-9 and +, / and =
			 */
#ifdef __APPLE__
			/* base64url may include - and _. */
			if (isalnum(*p) || *p == '+' || *p == '/' ||
			    *p == '=' || *p == '-' || *p == '_')
#else
			if (isalnum(*p) || *p == '+' || *p == '/' || *p == '=')
#endif
				count++;
			if (count % 4 == 0)
				count4 = p - inbuf;
			p++;
		}

		strcpy(leftover, inbuf + count4 + 1);
		inbuf[count4 + 1] = 0;

#ifdef __APPLE__
		n = apple_b64_pton(inbuf, outbuf, sizeof(outbuf));
#else
		n = b64_pton(inbuf, outbuf, sizeof(outbuf));
#endif

		if (n < 0)
			break;
		fwrite(outbuf, 1, n, outfp);
	}
	return (checkout(checkend(inbuf, "====", "error decoding base64 input stream")));
}

static void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: uudecode [-cimprs] [file ...]\n"
	    "       uudecode [-i] -o output_file [file]\n"
#ifndef __APPLE__
	    "       b64decode [-cimprs] [file ...]\n"
	    "       b64decode [-i] -o output_file [file]\n"
#endif
	    );
	exit(1);
}
