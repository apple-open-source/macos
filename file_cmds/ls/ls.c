/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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

#include <sys/cdefs.h>
#ifndef lint
__used static const char copyright[] =
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static char sccsid[] = "@(#)ls.c	8.5 (Berkeley) 4/2/94";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifndef __APPLE__
#include <sys/mac.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <getopt.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef COLORLS
#include <termcap.h>
#include <signal.h>
#endif
#ifdef __APPLE__
#include <sys/acl.h>
#include <sys/xattr.h>
#include <sys/param.h>
#include <get_compat.h>
#include <sys/sysctl.h>
#include <System/sys/fsctl.h>
#endif /* __APPLE__ */
#include "ls.h"
#include "extern.h"

/*
 * Upward approximation of the maximum number of characters needed to
 * represent a value of integral type t as a string, excluding the
 * NUL terminator, with provision for a sign.
 */
#define	STRBUF_SIZEOF(t)	(1 + CHAR_BIT * sizeof(t) / 3 + 1)

#define	IS_DATALESS(sp)		(f_dataless && (sp) && ((sp)->st_flags & SF_DATALESS))

/*
 * MAKENINES(n) turns n into (10**n)-1.  This is useful for converting a width
 * into a number that wide in decimal.
 * XXX: Overflows are not considered.
 */
#define MAKENINES(n)							\
	do {								\
		intmax_t __i;						\
									\
		/* Use a loop as all values of n are small. */		\
		for (__i = 1; n > 0; __i *= 10)				\
			n--;						\
		n = __i - 1;						\
	} while(0)

static void	 display(const FTSENT *, FTSENT *, int);
static int	 mastercmp(const FTSENT **, const FTSENT **);
static void	 traverse(int, char **, int);

#define	COLOR_OPT	(CHAR_MAX + 1)

static const struct option long_opts[] =
{
#ifdef COLORLS
        {"color",       optional_argument,      NULL, COLOR_OPT},
#endif
        {NULL,          no_argument,            NULL, 0}
};

static void (*printfcn)(const DISPLAY *);
static int (*sortfcn)(const FTSENT *, const FTSENT *);

long blocksize;			/* block size units */
int termwidth = 80;		/* default terminal width */

/* flags */
       int f_accesstime;	/* use time of last access */
       int f_birthtime;		/* use time of birth */
       int f_flags;		/* show flags associated with a file */
       int f_humanval;		/* show human-readable file sizes */
       int f_inode;		/* print inode */
static int f_kblocks;		/* print size in kilobytes */
#ifndef __APPLE__
       int f_label;		/* show MAC label */
#endif
static int f_listdir;		/* list actual directory, not contents */
static int f_listdot;		/* list files beginning with . */
       int f_longform;		/* long listing format */
static int f_noautodot;		/* do not automatically enable -A for root */
static int f_nofollow;		/* don't follow symbolic link arguments */
       int f_nonprint;		/* show unprintables as ? */
static int f_nosort;		/* don't sort output */
       int f_notabs;		/* don't use tab-separated multi-col output */
       int f_numericonly;	/* don't convert uid/gid to name */
       int f_octal;		/* show unprintables as \xxx */
       int f_octal_escape;	/* like f_octal but use C escapes if possible */
static int f_recursive;		/* ls subdirectories also */
static int f_reversesort;	/* reverse whatever sort is used */
       int f_samesort;		/* sort time and name in same direction */
       int f_sectime;		/* print full time information */
static int f_singlecol;		/* use single column output */
       int f_size;		/* list size in short listing */
static int f_sizesort;
       int f_slash;		/* similar to f_type, but only for dirs */
       int f_sortacross;	/* sort across rows, not down columns */
       int f_statustime;	/* use time of last mode change */
static int f_stream;		/* stream the output, separate with commas */
       int f_thousands;		/* show file sizes with thousands separators */
       char *f_timeformat;	/* user-specified time format */
static int f_timesort;		/* sort by time vice name */
       int f_type;		/* add type character for non-regular files */
static int f_whiteout;		/* show whiteout entries */
#ifdef __APPLE__
       int f_acl;		/* show ACLs in long listing */
       int f_xattr;		/* show extended attributes in long listing */
       int f_group;		/* show group */
       int f_owner;		/* show owner */
       int f_dataless;          /* distinguish dataless files in long listing,
				   and don't materialize dataless directories. */
#endif
#ifdef COLORLS
       int colorflag = COLORFLAG_NEVER;		/* passed in colorflag */
       int f_color;		/* add type in color for non-regular files */
       bool explicitansi;	/* Explicit ANSI sequences, no termcap(5) */
char *ansi_bgcol;		/* ANSI sequence to set background colour */
char *ansi_fgcol;		/* ANSI sequence to set foreground colour */
char *ansi_coloff;		/* ANSI sequence to reset colours */
char *attrs_off;		/* ANSI sequence to turn off attributes */
char *enter_bold;		/* ANSI sequence to set color to bold mode */
#endif

#ifdef __APPLE__
bool unix2003_compat;
#endif

static int rval;

static bool
do_color_from_env(void)
{
	const char *p;
	bool doit;

	doit = false;
	p = getenv("CLICOLOR");
	if (p == NULL) {
		/*
		 * COLORTERM is the more standard name for this variable.  We'll
		 * honor it as long as it's both set and not empty.
		 */
		p = getenv("COLORTERM");
		if (p != NULL && *p != '\0')
			doit = true;
	} else
		doit = true;

	return (doit &&
	    (isatty(STDOUT_FILENO) || getenv("CLICOLOR_FORCE")));
}

static bool
do_color(void)
{

#ifdef COLORLS
	if (colorflag == COLORFLAG_NEVER)
		return (false);
	else if (colorflag == COLORFLAG_ALWAYS)
		return (true);
#endif
	return (do_color_from_env());
}

#ifdef COLORLS
static bool
do_color_always(const char *term)
{

	return (strcmp(term, "always") == 0 || strcmp(term, "yes") == 0 ||
	    strcmp(term, "force") == 0);
}

static bool
do_color_never(const char *term)
{

	return (strcmp(term, "never") == 0 || strcmp(term, "no") == 0 ||
	    strcmp(term, "none") == 0);
}

static bool
do_color_auto(const char *term)
{

	return (strcmp(term, "auto") == 0 || strcmp(term, "tty") == 0 ||
	    strcmp(term, "if-tty") == 0);
}
#endif	/* COLORLS */

int
main(int argc, char *argv[])
{
	static char dot[] = ".", *dotav[] = {dot, NULL};
	struct winsize win;
	int ch, fts_options, notused;
	char *p;
	const char *errstr = NULL;
#ifdef COLORLS
	char termcapbuf[1024];	/* termcap definition buffer */
	char tcapbuf[512];	/* capability buffer */
	char *bp = tcapbuf, *term;
#endif

	if (argc < 1)
		usage();
	(void)setlocale(LC_ALL, "");

#ifdef __APPLE__
	unix2003_compat = COMPAT_MODE("bin/ls", "Unix2003");
#endif

	/* Terminal defaults to -Cq, non-terminal defaults to -1. */
	if (isatty(STDOUT_FILENO)) {
		termwidth = 80;
		if ((p = getenv("COLUMNS")) != NULL && *p != '\0')
			termwidth = strtonum(p, 0, INT_MAX, &errstr);
		else if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) != -1 &&
		    win.ws_col > 0)
			termwidth = win.ws_col;
		f_nonprint = 1;
	} else {
		f_singlecol = 1;
		/* retrieve environment variable, in case of explicit -C */
		p = getenv("COLUMNS");
		if (p)
			termwidth = strtonum(p, 0, INT_MAX, &errstr);
	}

	if (errstr)
		termwidth = 80;

	fts_options = FTS_PHYSICAL;
	if (getenv("LS_SAMESORT"))
		f_samesort = 1;

	/*
	 * For historical compatibility, we'll use our autodetection if CLICOLOR
	 * is set.
	 */
#ifdef COLORLS
	if (getenv("CLICOLOR"))
		colorflag = COLORFLAG_AUTO;
#endif
	while ((ch = getopt_long(argc, argv,
#ifdef __APPLE__
	    "+@1ABCD:FGHILOPRSTUWabcdefghiklmnopqrstuvwxy%,", long_opts,
#else
	    "+1ABCD:FGHILPRSTUWXZabcdfghiklmnopqrstuwxy,", long_opts,
#endif
	    NULL)) != -1) {
		switch (ch) {
		/*
		 * The -1, -C, -x and -l options all override each other so
		 * shell aliasing works right.
		 */
		case '1':
			f_singlecol = 1;
			f_longform = 0;
			f_stream = 0;
			break;
		case 'C':
			f_sortacross = f_longform = f_singlecol = 0;
			break;
		case 'l':
			f_longform = 1;
			f_singlecol = 0;
			f_stream = 0;
			break;
		case 'x':
			f_sortacross = 1;
			f_longform = 0;
			f_singlecol = 0;
			break;
		/* The -c, -u, and -U options override each other. */
		case 'c':
			f_statustime = 1;
			f_accesstime = 0;
			f_birthtime = 0;
			break;
		case 'u':
			f_accesstime = 1;
			f_statustime = 0;
			f_birthtime = 0;
			break;
		case 'U':
			f_birthtime = 1;
			f_accesstime = 0;
			f_statustime = 0;
			break;
		case 'f':
			f_nosort = 1;
		       /* FALLTHROUGH */
		case 'a':
			fts_options |= FTS_SEEDOT;
			/* FALLTHROUGH */
		case 'A':
			f_listdot = 1;
			break;
		/* The -t and -S options override each other. */
		case 'S':
			/* Darwin 1.4.1 compatibility */
			f_sizesort = 1;
			f_timesort = 0;
			break;
		case 't':
			f_timesort = 1;
			f_sizesort = 0;
			break;
		/* Other flags.  Please keep alphabetic. */
		case ',':
			f_thousands = 1;
			break;
		case 'B':
			f_nonprint = 0;
			f_octal = 1;
			f_octal_escape = 0;
			break;
		case 'D':
			f_timeformat = optarg;
			break;
		case 'F':
			f_type = 1;
			f_slash = 0;
			break;
		case 'G':
			/*
			 * We both set CLICOLOR here and set colorflag to
			 * COLORFLAG_AUTO, because -G should not force color if
			 * stdout isn't a tty.
			 */
			setenv("CLICOLOR", "", 1);
#ifdef COLORLS
			colorflag = COLORFLAG_AUTO;
#endif
			break;
		case 'H':
			if (unix2003_compat) {
				fts_options &= ~FTS_LOGICAL;
				fts_options |= FTS_PHYSICAL;
				fts_options |= FTS_COMFOLLOWDIR;
			} else {
				fts_options |= FTS_COMFOLLOW;
			}
			f_nofollow = 0;
			break;
		case 'I':
			f_noautodot = 1;
			break;
		case 'L':
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
			f_nofollow = 0;
			if (unix2003_compat) {
				fts_options &= ~(FTS_COMFOLLOW|FTS_COMFOLLOWDIR);
			}
			break;
		case 'P':
			fts_options &= ~(FTS_COMFOLLOW|FTS_COMFOLLOWDIR);
			fts_options &= ~FTS_LOGICAL;
			fts_options |= FTS_PHYSICAL;
			f_nofollow = 1;
			break;
		case 'R':
			f_recursive = 1;
			break;
		case 'T':
			f_sectime = 1;
			break;
		case 'W':
			f_whiteout = 1;
			break;
#ifndef __APPLE__
		case 'Z':
			f_label = 1;
			break;
#endif
		case 'b':
			f_nonprint = 0;
			f_octal = 0;
			f_octal_escape = 1;
			break;
		/* The -d option turns off the -R option. */
		case 'd':
			f_listdir = 1;
			f_recursive = 0;
			break;
		case 'g':	/* Compatibility with Unix03 */
#ifdef __APPLE__
			if (unix2003_compat) {
				f_group = 1;
				f_longform = 1;
				f_singlecol = 0;
				f_stream = 0;
			}
#endif
			break;
		case 'h':
			f_humanval = 1;
			break;
		case 'i':
			f_inode = 1;
			break;
		case 'k':
			f_humanval = 0;
			f_kblocks = 1;
			break;
		case 'm':
			f_stream = 1;
			f_singlecol = 0;
			f_longform = 0;
			break;
		case 'n':
			f_numericonly = 1;
			if (unix2003_compat) {
				f_longform = 1;
				f_singlecol = 0;
				f_stream = 0;
			}
			break;
		case 'o':
			if (unix2003_compat) {
#ifdef __APPLE__
				f_owner = 1;
				f_longform = 1;
				f_singlecol = 0;
				f_stream = 0;
#endif
			} else {
				f_flags = 1;
			}
			break;
		case 'p':
			f_slash = 1;
			f_type = 1;
			break;
		case 'q':
			f_nonprint = 1;
			f_octal = 0;
			f_octal_escape = 0;
			break;
		case 'r':
			f_reversesort = 1;
			break;
		case 's':
			f_size = 1;
			break;
		case 'v':
			/* Darwin 1.4.1 compatibility */
			f_nonprint = 0;
			break;
		case 'w':
			f_nonprint = 0;
			f_octal = 0;
			f_octal_escape = 0;
			break;
		case 'y':
			f_samesort = 1;
			break;
#ifdef __APPLE__
		case 'e':
			f_acl = 1;
			break;
		case '@':
			f_xattr = 1;
			break;
		case 'O':
			f_flags = 1;
			break;
		case '%':
			f_dataless = 1;
			break;
#endif
#ifdef COLORLS
		case COLOR_OPT:
			if (optarg == NULL || do_color_always(optarg))
				colorflag = COLORFLAG_ALWAYS;
			else if (do_color_auto(optarg))
				colorflag = COLORFLAG_AUTO;
			else if (do_color_never(optarg))
				colorflag = COLORFLAG_NEVER;
			else
				errx(2, "unsupported --color value '%s' (must be always, auto, or never)",
				    optarg);
			break;
#endif
		default:
		case '?':
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Root is -A automatically unless -I. */
	if (!f_listdot && getuid() == (uid_t)0 && !f_noautodot)
		f_listdot = 1;

	/*
	 * Enabling of colours is conditional on the environment in conjunction
	 * with the --color and -G arguments, if supplied.
	 */
	if (do_color()) {
#ifdef COLORLS
		if ((term = getenv("TERM")) != NULL &&
		    tgetent(termcapbuf, term) == 1) {
			ansi_fgcol = tgetstr("AF", &bp);
			ansi_bgcol = tgetstr("AB", &bp);
			attrs_off = tgetstr("me", &bp);
			enter_bold = tgetstr("md", &bp);

			/* To switch colours off use 'op' if
			 * available, otherwise use 'oc', or
			 * don't do colours at all. */
			ansi_coloff = tgetstr("op", &bp);
			if (!ansi_coloff)
				ansi_coloff = tgetstr("oc", &bp);
			if (ansi_fgcol && ansi_bgcol && ansi_coloff)
				f_color = 1;
		} else if (colorflag == COLORFLAG_ALWAYS) {
			/*
			 * If we're *always* doing color but we don't have
			 * a functional TERM supplied, we'll fallback to
			 * outputting raw ANSI sequences.
			 */
			f_color = 1;
			explicitansi = true;
		}
#else
		warnx("color support not compiled in");
#endif /*COLORLS*/
	}

#ifdef COLORLS
	if (f_color) {
		/*
		 * We can't put tabs and color sequences together:
		 * column number will be incremented incorrectly
		 * for "stty oxtabs" mode.
		 */
		f_notabs = 1;
		(void)signal(SIGINT, colorquit);
		(void)signal(SIGQUIT, colorquit);
		parsecolors(getenv("LSCOLORS"));
	}
#endif

	/*
	 * If not -F, -i, -l, -s, -S, -t or -% options, don't require stat
	 * information, unless in color mode in which case we do
	 * need this to determine which colors to display.
	 */
	if (!f_inode && !f_longform && !f_size && !f_timesort &&
	    !f_sizesort && !f_type
#ifdef __APPLE__
	    && !f_dataless
#endif
#ifdef COLORLS
	    && !f_color
#endif
	    )
		fts_options |= FTS_NOSTAT;

	/*
	 * If not -F, -P, -d, -i or -l options, follow any symbolic links listed on
	 * the command line, unless in color mode in which case we need to
	 * distinguish file type for a symbolic link itself and its target.
	 */
	if (!f_nofollow && !f_longform && !f_listdir && (!f_type || f_slash)
	    && !f_inode
#ifdef COLORLS
	    && !f_color
#endif
	    )
		fts_options |= FTS_COMFOLLOW;

	/*
	 * If -W, show whiteout entries
	 */
#ifdef FTS_WHITEOUT
	if (f_whiteout)
		fts_options |= FTS_WHITEOUT;
#endif

	/* If -i, -l or -s, figure out block size. */
	if (f_inode || f_longform || f_size) {
		if (f_kblocks)
			blocksize = 2;
		else {
			(void)getbsize(&notused, &blocksize);
			blocksize /= 512;
		}
	}
	/* Select a sort function. */
	if (f_reversesort) {
		if (!f_timesort && !f_sizesort)
			sortfcn = revnamecmp;
		else if (f_sizesort)
			sortfcn = revsizecmp;
		else if (f_accesstime)
			sortfcn = revacccmp;
		else if (f_birthtime)
			sortfcn = revbirthcmp;
		else if (f_statustime)
			sortfcn = revstatcmp;
		else		/* Use modification time. */
			sortfcn = revmodcmp;
	} else {
		if (!f_timesort && !f_sizesort)
			sortfcn = namecmp;
		else if (f_sizesort)
			sortfcn = sizecmp;
		else if (f_accesstime)
			sortfcn = acccmp;
		else if (f_birthtime)
			sortfcn = birthcmp;
		else if (f_statustime)
			sortfcn = statcmp;
		else		/* Use modification time. */
			sortfcn = modcmp;
	}

	/* Select a print function. */
	if (f_singlecol)
		printfcn = printscol;
	else if (f_longform)
		printfcn = printlong;
	else if (f_stream)
		printfcn = printstream;
	else
		printfcn = printcol;

#ifdef __APPLE__
	if (f_dataless) {
		// don't materialize dataless directories from the cloud
		// (particularly useful when listing recursively)
		int state = 1;
		if (sysctlbyname("vfs.nspace.prevent_materialization", NULL, NULL, &state, sizeof(state)) < 0) {
			err(1, "prevent_materialization");
		}
	}
#endif /* __APPLE__ */
	if (argc)
		traverse(argc, argv, fts_options);
	else
		traverse(1, dotav, fts_options);
#ifdef __APPLE__
	if (rval == 0 && (ferror(stdout) != 0 || fflush(stdout) != 0))
		err(1, "stdout");
#endif
	exit(rval);
}

static int output;		/* If anything output. */

/*
 * Traverse() walks the logical directory structure specified by the argv list
 * in the order specified by the mastercmp() comparison function.  During the
 * traversal it passes linked lists of structures to display() which represent
 * a superset (may be exact set) of the files to be displayed.
 */
static void
traverse(int argc, char *argv[], int options)
{
	FTS *ftsp;
	FTSENT *p, *chp;
	int ch_options, error;

	if ((ftsp =
	    fts_open(argv, options, f_nosort ? NULL : mastercmp)) == NULL)
		err(1, "fts_open");

	/*
	 * We ignore errors from fts_children here since they will be
	 * replicated and signalled on the next call to fts_read() below.
	 */
	chp = fts_children(ftsp, 0);
	if (chp != NULL)
		display(NULL, chp, options);
	if (f_listdir) {
		fts_close(ftsp);
		return;
	}

	/*
	 * If not recursing down this tree and don't need stat info, just get
	 * the names.
	 */
	ch_options = !f_recursive &&
#ifndef __APPLE__
	    !f_label &&
#endif
	    options & FTS_NOSTAT ? FTS_NAMEONLY : 0;

	while ((void)(errno = 0), (p = fts_read(ftsp)) != NULL)
		switch (p->fts_info) {
		case FTS_DC:
			warnx("%s: directory causes a cycle", p->fts_name);
			if (unix2003_compat) {
				rval = 1;
			}
			break;
		case FTS_DNR:
		case FTS_ERR:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_D:
			if (p->fts_level != FTS_ROOTLEVEL &&
			    p->fts_name[0] == '.' && !f_listdot) {
				fts_set(ftsp, p, FTS_SKIP);
				break;
			}

#ifdef __APPLE__
			if (IS_DATALESS(p->fts_statp)) {
				fts_set(ftsp, p, FTS_SKIP);
				break;
			}
#endif

			/*
			 * If already output something, put out a newline as
			 * a separator.  If multiple arguments, precede each
			 * directory with its name.
			 */
			if (output) {
				putchar('\n');
				(void)printname(p->fts_path);
				puts(":");
			} else if (argc > 1) {
				(void)printname(p->fts_path);
				puts(":");
				output = 1;
			}
			chp = fts_children(ftsp, ch_options);
			if (unix2003_compat && ((options & FTS_LOGICAL) != 0)) {
				FTSENT *curr;
				for (curr = chp; curr; curr = curr->fts_link) {
					if (curr->fts_info == FTS_SLNONE)
						curr->fts_number = NO_PRINT;
				}
			}
			display(p, chp, options);

			if (!f_recursive && chp != NULL)
				(void)fts_set(ftsp, p, FTS_SKIP);
			break;
		case FTS_SLNONE:	/* Same as default unless Unix conformance */
			if (unix2003_compat) {
				if ((options & FTS_LOGICAL) != 0) {	/* -L was specified */
					warnx("%s: %s", p->fts_name, strerror(p->fts_errno ?: ENOENT));
					rval = 1;
				}
			}
			break;
		case FTS_DP:
			/* If we have fts_errno set from the preorder run, just bail out */
			if (p->fts_errno) {
				errno = p->fts_errno;
				goto out;
			}
			break;
		default:
			break;
		}
out:
	/* Map ENEEDAUTH to EPERM which is a well-known error code */
	error = (errno == ENEEDAUTH) ? EPERM : errno;
	fts_close(ftsp);
	errno = error;

	if (errno)
		err(1, "fts_read");
}

/*
 * Display() takes a linked list of FTSENT structures and passes the list
 * along with any other necessary information to the print function.  P
 * points to the parent directory of the display list.
 */
static void
display(const FTSENT *p, FTSENT *list, int options)
{
	struct stat *sp;
	DISPLAY d;
	FTSENT *cur;
	NAMES *np;
	off_t maxsize;
	u_int64_t maxblock;
	ino_t maxinode;
	u_int64_t btotal, labelstrlen, maxlen, maxnlink;
#ifndef __APPLE__
	u_int64_t maxlattr;
#endif // ! __APPLE__
	u_int64_t maxlabelstr;
	u_int sizelen;
	int maxflags;
	gid_t maxgroup;
	uid_t maxuser;
	size_t flen, ulen, glen;
	char *initmax;
	int entries, needstats;
	const char *user, *group;
	char *flags, *labelstr = NULL;
#ifndef __APPLE__
	char buf[STRBUF_SIZEOF(u_quad_t) + 1];
#endif // ! __APPLE__
	char ngroup[STRBUF_SIZEOF(uid_t) + 1];
	char nuser[STRBUF_SIZEOF(gid_t) + 1];
#ifdef __APPLE__
	acl_entry_t dummy;
	ssize_t xattr_size;
	char *filename;
	char path[MAXPATHLEN+1];
#endif // __APPLE__
	u_long width[9];
	int i;

	needstats = f_inode || f_longform || f_size;
	flen = 0;
	btotal = 0;

#define LS_COLWIDTHS_FIELDS	9
	initmax = getenv("LS_COLWIDTHS");

	for (i = 0 ; i < LS_COLWIDTHS_FIELDS; i++)
		width[i] = 0;

	if (initmax != NULL) {
		char *endp;

		for (i = 0; i < LS_COLWIDTHS_FIELDS && *initmax != '\0'; i++) {
			if (*initmax == ':') {
				width[i] = 0;
			} else {
				width[i] = strtoul(initmax, &endp, 10);
				initmax = endp;
				while (isspace(*initmax))
					initmax++;
				if (*initmax != ':')
					break;
				initmax++;
			}
		}
		if (i < LS_COLWIDTHS_FIELDS)
#ifdef COLORLS
			if (!f_color)
#endif
				f_notabs = 0;
	}

	/* Fields match -lios order.  New ones should be added at the end. */
	maxinode = width[0];
	maxblock = width[1];
	maxnlink = width[2];
	maxuser = width[3];
	maxgroup = width[4];
	maxflags = width[5];
	maxsize = width[6];
	maxlen = width[7];
	maxlabelstr = width[8];

	MAKENINES(maxinode);
	MAKENINES(maxblock);
	MAKENINES(maxnlink);
	MAKENINES(maxsize);

	d.s_size = 0;
	sizelen = 0;
	flags = NULL;
	for (cur = list, entries = 0; cur; cur = cur->fts_link) {
		if (cur->fts_info == FTS_ERR || cur->fts_info == FTS_NS) {
			warnx("%s: %s",
			    cur->fts_name, strerror(cur->fts_errno));
			cur->fts_number = NO_PRINT;
			rval = 1;
			continue;
		}
		/*
		 * P is NULL if list is the argv list, to which different rules
		 * apply.
		 */
		if (p == NULL) {
			/* Directories will be displayed later. */
			if (cur->fts_info == FTS_D && !f_listdir) {
				cur->fts_number = NO_PRINT;
				continue;
			}
		} else {
			/* Only display dot file if -a/-A set. */
			if (cur->fts_name[0] == '.' && !f_listdot) {
				cur->fts_number = NO_PRINT;
				continue;
			}
		}
		if (cur->fts_namelen > maxlen)
			maxlen = cur->fts_namelen;
		if (f_octal || f_octal_escape) {
			u_long t = len_octal(cur->fts_name, cur->fts_namelen);

			if (t > maxlen)
				maxlen = t;
		}
		if (needstats) {
			sp = cur->fts_statp;
			if (sp->st_blocks > maxblock)
				maxblock = sp->st_blocks;
			if (sp->st_ino > maxinode)
				maxinode = sp->st_ino;
			if (sp->st_nlink > maxnlink)
				maxnlink = sp->st_nlink;
			if (sp->st_size > maxsize)
				maxsize = sp->st_size;

			btotal += sp->st_blocks;
			if (f_longform) {
				if (f_numericonly) {
					(void)snprintf(nuser, sizeof(nuser),
					    "%u", sp->st_uid);
					(void)snprintf(ngroup, sizeof(ngroup),
					    "%u", sp->st_gid);
					user = nuser;
					group = ngroup;
				} else {
					user = user_from_uid(sp->st_uid, 0);
					/*
					 * user_from_uid(..., 0) only returns
					 * NULL in OOM conditions.  We could
					 * format the uid here, but (1) in
					 * general ls(1) exits on OOM, and (2)
					 * there is another allocation/exit
					 * path directly below, which will
					 * likely exit anyway.
					 */
					if (user == NULL)
						err(1, "user_from_uid");
					group = group_from_gid(sp->st_gid, 0);
					/* Ditto. */
					if (group == NULL)
						err(1, "group_from_gid");
				}
				if ((ulen = strlen(user)) > maxuser)
					maxuser = ulen;
				if ((glen = strlen(group)) > maxgroup)
					maxgroup = glen;
				if (f_flags) {
					flags = fflagstostr(sp->st_flags);
					if (flags != NULL && *flags == '\0') {
						free(flags);
						flags = strdup("-");
					}
					if (flags == NULL)
						err(1, "fflagstostr");
					flen = strlen(flags);
					if (flen > (size_t)maxflags)
						maxflags = flen;
				} else
					flen = 0;
				labelstr = NULL;
#ifndef __APPLE__
				if (f_label) {
					char name[PATH_MAX + 1];
					mac_t label;
					int error;

					error = mac_prepare_file_label(&label);
					if (error == -1) {
						warn("MAC label for %s/%s",
						    cur->fts_parent->fts_path,
						    cur->fts_name);
						goto label_out;
					}

					if (cur->fts_level == FTS_ROOTLEVEL)
						snprintf(name, sizeof(name),
						    "%s", cur->fts_name);
					else
						snprintf(name, sizeof(name),
						    "%s/%s", cur->fts_parent->
						    fts_accpath, cur->fts_name);

					if (options & FTS_LOGICAL)
						error = mac_get_file(name,
						    label);
					else
						error = mac_get_link(name,
						    label);
					if (error == -1) {
						warn("MAC label for %s/%s",
						    cur->fts_parent->fts_path,
						    cur->fts_name);
						mac_free(label);
						goto label_out;
					}

					error = mac_to_text(label,
					    &labelstr);
					if (error == -1) {
						warn("MAC label for %s/%s",
						    cur->fts_parent->fts_path,
						    cur->fts_name);
						mac_free(label);
						goto label_out;
					}
					mac_free(label);
label_out:
					if (labelstr == NULL)
						labelstr = strdup("-");
					labelstrlen = strlen(labelstr);
					if (labelstrlen > maxlabelstr)
						maxlabelstr = labelstrlen;
				} else
					labelstrlen = 0;
#else
				labelstrlen = 0;
#endif /* !__APPLE__ */

				if ((np = calloc(1, sizeof(NAMES) + labelstrlen +
				    ulen + glen + flen + 4)) == NULL)
					err(1, "malloc");

				np->user = &np->data[0];
				(void)strcpy(np->user, user);
				np->group = &np->data[ulen + 1];
				(void)strcpy(np->group, group);
#ifdef __APPLE__
				if (cur->fts_level == FTS_ROOTLEVEL) {
					filename = cur->fts_name;
				} else {
					snprintf(path, sizeof(path), "%s/%s", cur->fts_parent->fts_accpath, cur->fts_name);
					filename = path;
				}
				xattr_size = listxattr(filename, NULL, 0, XATTR_NOFOLLOW);
				if (xattr_size < 0) {
					xattr_size = 0;
				}
				if ((xattr_size > 0) && f_xattr) {
					/* collect sizes */
					np->xattr_names = malloc(xattr_size);
					listxattr(filename, np->xattr_names, xattr_size, XATTR_NOFOLLOW);
					for (char *name = np->xattr_names; name < np->xattr_names + xattr_size;
					     name += strlen(name)+1) {
						np->xattr_sizes = reallocf(np->xattr_sizes, (np->xattr_count+1) * sizeof(np->xattr_sizes[0]));
						np->xattr_sizes[np->xattr_count] = getxattr(filename, name, 0, 0, 0, XATTR_NOFOLLOW);
						np->xattr_count++;
					}
				}
				/* symlinks can not have ACLs */
				np->acl = acl_get_link_np(filename, ACL_TYPE_EXTENDED);
				if (np->acl) {
					if (acl_get_entry(np->acl, ACL_FIRST_ENTRY, &dummy) == -1) {
						acl_free(np->acl);
						np->acl = NULL;
					}
				}
				if (xattr_size > 0) {
					np->mode_suffix = '@';
				} else if (np->acl) {
					np->mode_suffix = '+';
				} else {
					np->mode_suffix = ' ';
				}
				if (IS_DATALESS(sp)) {
					np->mode_suffix = '%';
				}
				if (!f_acl) {
					acl_free(np->acl);
					np->acl = NULL;
				}
#endif // __APPLE__
				if (S_ISCHR(sp->st_mode) ||
				    S_ISBLK(sp->st_mode)) {
					sizelen = snprintf(NULL, 0,
					    "%#jx", (uintmax_t)sp->st_rdev);
					if (d.s_size < sizelen)
						d.s_size = sizelen;
				}

				if (f_flags) {
					np->flags = &np->data[ulen + glen + 2];
					(void)strcpy(np->flags, flags);
					free(flags);
				}
#ifndef __APPLE__
				if (f_label) {
					np->label = &np->data[ulen + glen + 2
					    + (f_flags ? flen + 1 : 0)];
					(void)strcpy(np->label, labelstr);
					free(labelstr);
				}
#endif /* !__APPLE__ */
				cur->fts_pointer = np;
			}
		}
		++entries;
	}

	/*
	 * If there are no entries to display, we normally stop right
	 * here.  However, we must continue if we have to display the
	 * total block count.  In this case, we display the total only
	 * on the second (p != NULL) pass.
	 */
	if (!entries && (!(f_longform || f_size) || p == NULL))
		return;

	d.list = list;
	d.entries = entries;
	d.maxlen = maxlen;
	if (needstats) {
		d.btotal = btotal;
		d.s_block = snprintf(NULL, 0, "%llu", howmany(maxblock, blocksize));
		d.s_flags = maxflags;
#ifndef __APPLE__
		d.s_label = maxlabelstr;
#endif
		d.s_group = maxgroup;
		d.s_inode = snprintf(NULL, 0, "%llu", maxinode);
		d.s_nlink = snprintf(NULL, 0, "%llu", maxnlink);
		sizelen = f_humanval ? HUMANVALSTR_LEN :
		snprintf(NULL, 0, "%lld", maxsize);
		if (d.s_size < sizelen)
			d.s_size = sizelen;
		d.s_user = maxuser;
	}
	if (f_thousands)			/* make space for commas */
		d.s_size += (d.s_size - 1) / 3;
	printfcn(&d);
	output = 1;

	if (f_longform) {
		for (cur = list; cur; cur = cur->fts_link) {
			np = cur->fts_pointer;
			if (np) {
				if (np->acl) {
					acl_free(np->acl);
				}
				free(np->xattr_names);
				free(np->xattr_sizes);
				free(np);
				cur->fts_pointer = NULL;
			}
		}
	}
}

/*
 * Ordering for mastercmp:
 * If ordering the argv (fts_level = FTS_ROOTLEVEL) return non-directories
 * as larger than directories.  Within either group, use the sort function.
 * All other levels use the sort function.  Error entries remain unsorted.
 */
static int
mastercmp(const FTSENT **a, const FTSENT **b)
{
	int a_info, b_info;

	a_info = (*a)->fts_info;
	if (a_info == FTS_ERR)
		return (0);
	b_info = (*b)->fts_info;
	if (b_info == FTS_ERR)
		return (0);

	if (a_info == FTS_NS || b_info == FTS_NS)
		return (namecmp(*a, *b));

	if (a_info != b_info &&
	    (*a)->fts_level == FTS_ROOTLEVEL && !f_listdir) {
		if (a_info == FTS_D)
			return (1);
		if (b_info == FTS_D)
			return (-1);
	}
	return (sortfcn(*a, *b));
}
