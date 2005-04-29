/*	$NetBSD: split.c,v 1.6 1997/10/19 23:26:58 lukem Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)split.c	8.3 (Berkeley) 4/25/94";
#endif
__RCSID("$NetBSD: split.c,v 1.6 1997/10/19 23:26:58 lukem Exp $");
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define DEFLINE	1000			/* Default num lines per file. */
#define NUMBERBASE 26
#define SUFFIXFIRSTCHAR 'a'

long	 bytecnt;			/* Byte count to split on. */
long	 numlines;			/* Line count to split on. */
int	 file_open;			/* If a file open. */
int	 ifd = -1, ofd = -1;		/* Input/output file descriptors. */
char	 bfr[MAXBSIZE];			/* I/O buffer. */
char	 fname[MAXPATHLEN];		/* File name prefix. */
long	gSuffixLen;				/* length of generated file suffix */
double	gMaxFiles;				/* maximum number of output files that can be generated */

int  main __P((int, char **));
void newfile __P((void));
void split1 __P((void));
void split2 __P((void));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *ep, *p;

	while ((ch = getopt(argc, argv, "-0123456789b:l:a:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines == 0) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					numlines = strtol(++p, &ep, 10);
				else
					numlines =
					    strtol(argv[optind] + 1, &ep, 10);
				if (numlines <= 0 || *ep)
					errx(1,
					    "%s: illegal line count.", optarg);
			}
			break;
		case '-':		/* Undocumented: historic stdin flag. */
			if (ifd != -1)
				usage();
			ifd = 0;
			break;
		case 'b':		/* Byte count. */
			if ((bytecnt = strtol(optarg, &ep, 10)) <= 0 ||
			    (*ep != '\0' && *ep != 'k' && *ep != 'm'))
				errx(1, "%s: illegal byte count.", optarg);
			if (*ep == 'k')
				bytecnt *= 1024;
			else if (*ep == 'm')
				bytecnt *= 1048576;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			if ((numlines = strtol(optarg, &ep, 10)) <= 0 || *ep)
				errx(1, "%s: illegal line count.", optarg);
			break;
		case 'a':		/* suffix length */
			if (gSuffixLen != 0)
				usage();
			if ((gSuffixLen = strtol(optarg, &ep, 10)) <= 0 || *ep)
				errx(1, "%s: illegal suffix length.", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (*argv != NULL)
		if (ifd == -1) {		/* Input file. */
			if ((ifd = open(*argv, O_RDONLY, 0)) < 0)
				err(1, "%s", *argv);
			++argv;
		}
	if (*argv != NULL)			/* File name prefix. */
		(void)strcpy(fname, *argv++);
	if (*argv != NULL)
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt)
		usage();

	if (gSuffixLen == 0)
		gSuffixLen = 2;			/* default suffix length */

	if (fname[0] == '\0') {
		strcpy(fname, "x");		/* default prefix */
	}
	
	if ((strlen(fname) + gSuffixLen) > MAXPATHLEN)
		errx(1, "%s: prefix+suffix filename too long.");

	gMaxFiles = pow(NUMBERBASE, gSuffixLen);	/* maximum # of output files possible */

	if (ifd == -1)				/* Stdin by default. */
		ifd = 0;

	if (bytecnt) {
		split1();
		exit (0);
	}
	split2();
	exit(0);
}

/*
 * split1 --
 *	Split the input by bytes.
 */
void
split1()
{
	long bcnt;
	int dist, len;
	char *C;

	for (bcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				newfile();
				file_open = 1;
			}
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (write(ofd, bfr, dist) != dist)
					err(1, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				    len -= bytecnt, C += bytecnt) {
					newfile();
					if (write(ofd,
					    C, (int)bytecnt) != bytecnt)
						err(1, "write");
				}
				if (len) {
					newfile();
					if (write(ofd, C, len) != len)
						err(1, "write");
				} else
					file_open = 0;
				bcnt = len;
			} else {
				bcnt += len;
				if (write(ofd, bfr, len) != len)
					err(1, "write");
			}
		}
}

/*
 * split2 --
 *	Split the input by lines.
 */
void
split2()
{
	long lcnt;
	int len, bcnt;
	char *Ce, *Cs;

	for (lcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				newfile();
				file_open = 1;
			}
			for (Cs = Ce = bfr; len--; Ce++)
				if (*Ce == '\n' && ++lcnt == numlines) {
					bcnt = Ce - Cs + 1;
					if (write(ofd, Cs, bcnt) != bcnt)
						err(1, "write");
					lcnt = 0;
					Cs = Ce + 1;
					if (len)
						newfile();
					else
						file_open = 0;
				}
			if (Cs < Ce) {
				bcnt = Ce - Cs;
				if (write(ofd, Cs, bcnt) != bcnt)
					err(1, "write");
			}
		}
}

/*
 * generateSuffix --
 *  create the suffix for the output filename
 *
 */
static void
generateSuffix(char *suffixPointer, double fileNum)
{
	long	counter;
	
	/* loop through all powers of number base, up to length of filename suffix */
	for (counter = gSuffixLen-1; counter > 0; --counter) {
		double	power = pow(NUMBERBASE, counter);	/* calculate value of digit position */
		long factored = fileNum / power;			/* calculate valud of digit at that position */
		*suffixPointer++ = factored + SUFFIXFIRSTCHAR;	/* generate the "digit" */
		fileNum -= factored * power;				/* adjust remainder of file number */
	}
	*suffixPointer = fileNum + SUFFIXFIRSTCHAR;		/* generate the remaining ones digit */
	return;
}

/*
 * newfile --
 *	Open a new output file.
 */
void
newfile()
{
	static double fnum;
	static char *fpnt;

	if (ofd == -1) {
		fpnt = fname + strlen(fname);
		ofd = fileno(stdout);
	}
	if (fnum >= gMaxFiles) {
		errx(1, "too many files.");
	}
	generateSuffix(fpnt, fnum);
	fnum += 1.0;

	if (!freopen(fname, "w", stdout))
		err(1, "%s", fname);
}

void
usage()
{
	(void)fprintf(stderr,
"usage: split [-b byte_count] [-l line_count] [-a suffix_length] [file [prefix]]\n");
	exit(1);
}
