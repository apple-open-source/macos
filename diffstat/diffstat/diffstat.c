/******************************************************************************
 * Copyright 1994,1995,1996,1998 by Thomas E. Dickey <dickey@clark.net>       *
 * All Rights Reserved.                                                       *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software and its      *
 * documentation for any purpose and without fee is hereby granted, provided  *
 * that the above copyright notice appear in all copies and that both that    *
 * copyright notice and this permission notice appear in supporting           *
 * documentation, and that the name of the above listed copyright holder(s)   *
 * not be used in advertising or publicity pertaining to distribution of the  *
 * software without specific, written prior permission.                       *
 *                                                                            *
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD   *
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND  *
 * FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE  *
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES          *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR *
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.                *
 ******************************************************************************/

#ifndef	NO_IDENT
static	char	*Id = "$Id: diffstat.c,v 1.2 1999/12/08 03:47:54 wsanchez Exp $";
#endif

/*
 * Title:	diffstat.c
 * Author:	T.E.Dickey
 * Created:	02 Feb 1992
 * Modified:
 *		17 May 1998, handle Debian diff files, which do not contain
 *			     dates on the header lines.
 *		16 Jan 1998, accommodate patches w/o tabs in header lines (e.g.,
 *			     from cut/paste).  Strip suffixes such as ".orig".
 *		24 Mar 1996, corrected -p0 logic, more fixes in merge_name.
 *		16 Mar 1996, corrected state-change for "Binary".  Added -p
 *			     option.
 *		17 Dec 1995, corrected matching algorithm in 'merge_name()'
 *		11 Dec 1995, mods to accommodate diffs against /dev/null or
 *			     /tmp/XXX (tempfiles).
 *		06 May 1995, limit scaling -- only shrink-to-fit.
 *		29 Apr 1995, recognize 'rcsdiff -u' format.
 *		26 Dec 1994, strip common pathname-prefix.
 *		13 Nov 1994, added '-n' option.  Corrected logic of 'match'.
 *		17 Jun 1994, ifdef-<string.h>
 *		12 Jun 1994, recognize unified diff, and output of makepatch.
 *		04 Oct 1993, merge multiple diff-files, busy message when the
 *			     output is piped to a file.
 *
 * Function:	this program reads the output of 'diff' and displays a histogram
 *		of the insertions/deletions/modifications per-file.
 */

#include "patchlev.h"

#if	defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#define strchr index
#define strrchr rindex
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
extern	int	atoi();
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#else
extern int	isatty();
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#else
#if NEED_CHECK_FOR_MALLOC
extern	char	*malloc();
#endif
#endif

#if HAVE_GETOPT_H
#include <getopt.h>
#else
#if NEED_CHECK_FOR_GETOPT
extern	int	getopt();
extern	char	*optarg;
extern	int	optind;
#endif
#endif

#if !defined(TRUE) || (TRUE != 1)
#undef  TRUE
#undef  FALSE
#define	TRUE		1
#define	FALSE		0
#endif

#if !defined(EXIT_SUCCESS)
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif

/******************************************************************************/

#define PATHSEP '/'
#define EOS     '\0'
#define BLANK   ' '

#ifdef DEBUG
#define TRACE(p) printf p;
#else
#define TRACE(p) /*nothing*/
#endif

#define contain_any(s,reject) (strcspn(s,reject) != strlen(s))

#define HAVE_NOTHING 0
#define HAVE_GENERIC 1	/* e.g., "Index: foo" w/o pathname */
#define HAVE_PATH    2	/* reference-file from "diff dirname/foo" */
#define HAVE_PATH2   4	/* comparison-file from "diff dirname/foo" */

typedef	enum comment { Normal, Only, Binary } Comment;

typedef	struct	_data	{
	struct	_data	*link;
	char		*name;	/* the filename */
	int		base;	/* beginning of name if -p option used */
	Comment		cmt;
	long		ins,	/* "+" count inserted lines */
			del,	/* "-" count deleted lines */
			mod;	/* "!" count modified lines */
	} DATA;

static	DATA	*all_data;
static	int	piped_output;
static	int	max_width;	/* the specified width-limit */
static	int	name_wide;	/* the amount reserved for filenames */
static	int	prefix_opt = -1;/* if positive, controls stripping of PATHSEP */
static	int	plot_width;	/* the amount left over for histogram */
static	long	plot_scale;	/* the effective scale (1:maximum) */

/******************************************************************************/
#if	__STDC__
static	DATA* 	new_data (char* name);
static	char* 	merge_name (DATA* data, char* path);
static	char* 	new_string (char* s);
static	int	HadDiffs (DATA* p);
static	int	begin_data (DATA* p);
static	int	can_be_merged (char* path);
static	int	edit_range (char* s);
static	int	is_leaf (char *leaf, char *path);
static	int	match (char* s, char* p);
static	int	version_num (char* s);
static	long	plot_num (long num_value, int c, long extra);
static	void	blip (int c);
static	void	delink (DATA* p);
static	void	do_file (FILE* fp);
static	void	failed (char* s);
static	void	summarize (void);
static	void	usage (void);

extern	int	main(int argc, char *argv[]);
#endif
/******************************************************************************/

static
void	failed(s)
	char	*s;
{
	perror(s);
	exit(EXIT_FAILURE);
}

static
void	blip(c)
	int	c;
{
	if (piped_output) {
		(void)fputc(c, stderr);
		(void)fflush(stderr);
	}
}

static
char *	new_string(s)
	char	*s;
{
	return strcpy(malloc((unsigned)(strlen(s)+1)), s);
}

static
DATA *	new_data(name)
	char	*name;
{
	register DATA *p, *q, *r;

	TRACE(("new_data(%s)\n", name))

	/* insert into sorted list */
	for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
		int	cmp = strcmp(p->name, name);
		if (cmp == 0)
			return p;
		if (cmp > 0) {
			break;
		}
	}
	r = (DATA *)malloc(sizeof(DATA));
	if (q != 0)
		q->link = r;
	else
		all_data = r;

	r->link = p;
	r->name = new_string(name);
	r->base = 0;
	r->cmt = Normal;
	r->ins =
	r->del =
	r->mod = 0;

	return r;
}

/*
 * Remove a unneeded data item from the linked list.  Don't free the name,
 * since we may want it in another context.
 */
static
void	delink(data)
	DATA	*data;
{
	register DATA *p, *q;

	TRACE(("delink '%s'\n", data->name))

	for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
		if (p == data) {
			if (q != 0)
				q->link = p->link;
			else
				all_data = p->link;
			return;
		}
	}
}

/* like strncmp, but without the 3rd argument */
static
int	match(s, p)
	char	*s;
	char	*p;
{
	int	ok = FALSE;
	while (*s != EOS) {
		if (*p == EOS) {
			ok = TRUE;
			break;
		}
		if (*s++ != *p++)
			break;
	}
	return ok;
}

static
int	version_num(s)
	char	*s;
{
	int	main_ver, sub_ver;
	char	temp[2];
	return (sscanf(s, "%d.%d%c", &main_ver, &sub_ver, temp) == 2);
}

static
int	edit_range(s)
	char	*s;
{
	int	first, last;
	char	temp[2];
	return (sscanf(s, "%d,%d%c", &first, &last, temp) == 2);
}

static
int	HadDiffs(data)
	DATA	*data;
{
	return data->ins != 0
	  ||   data->del != 0
	  ||   data->mod != 0;
}

/*
 * If the given path is not one of the "ignore" paths, then return true.
 */
static
int	can_be_merged(path)
	char	*path;
{
	if (strcmp(path, "")
	 && strcmp(path, "/dev/null")
	 && strncmp(path, "/tmp/", 5))
	 	return TRUE;
	return FALSE;
}

static
int	is_leaf(leaf, path)
	char	*leaf;
	char	*path;
{
	char	*s;

	if (strchr(leaf, PATHSEP) == 0
	 && (s = strrchr(path, PATHSEP)) != 0
	 && !strcmp(++s, leaf))
	 	return TRUE;
	return FALSE;
}

static
char *	merge_name(data, path)
	DATA	*data;
	char	*path;
{
	TRACE(("merge_name(%s,%s) diffs:%d\n", data->name, path, HadDiffs(data)))

	if (!HadDiffs(data)) { /* the data was the first of 2 markers */
		if (is_leaf(data->name, path)) {
			TRACE(("is_leaf: %s vs %s\n", data->name, path))
			delink(data);
		} else if (can_be_merged(data->name)
		    &&     can_be_merged(path)) {
			size_t	len1 = strlen(data->name);
			size_t	len2 = strlen(path);
			int	n;
			int	matched = 0;
			int	diff = 0;

			/* strip suffixes such as ".orig", ".bak" */
			if (len1 > len2) {
				if (!strncmp(data->name, path, len2)) {
					data->name[len1 = len2] = EOS;
				}
			} else if (len1 < len2) {
				if (!strncmp(data->name, path, len1)) {
					path[len2 = len1] = EOS;
				}
			}

			for (n = 1; n <= len1 && n <= len2; n++) {
				if (data->name[len1-n] != path[len2-n]) {
					diff = n;
					break;
				}
				if (path[len2-n] == PATHSEP)
					matched = n;
			}

			if (prefix_opt < 0
			 && matched != 0
			 && diff)
				path += len2 - matched + 1;

			delink(data);
			TRACE(("merge @%d, prefix_opt=%d matched=%d diff=%d\n",
				__LINE__, prefix_opt, matched, diff))
		} else if (!can_be_merged(path)) {
			TRACE(("merge @%d\n", __LINE__))
			/* must not merge, retain existing name */
			path = data->name;
		} else {
			TRACE(("merge @%d\n", __LINE__))
			delink(data);
		}
	} else if (!can_be_merged(path)) {
		path = data->name;
	}
	return path;
}

static
int	begin_data(p)
	DATA	*p;
{
	if (!can_be_merged(p->name)
	 && strchr(p->name, PATHSEP) != 0) {
		TRACE(("begin_data:HAVE_PATH\n"))
		return HAVE_PATH;
	}
	TRACE(("begin_data:HAVE_GENERIC\n"))
	return HAVE_GENERIC;
}

static
void	do_file(fp)
	FILE	*fp;
{
	DATA	dummy, *this = &dummy;
	char	buffer[BUFSIZ];
	int	ok = HAVE_NOTHING;
	register char *s;

	dummy.name = "";
	dummy.ins =
	dummy.del =
	dummy.mod = 0;

	while (fgets(buffer, sizeof(buffer), fp)) {
		/*
		 * Trim trailing blanks (e.g., newline)
		 */
		for (s = buffer + strlen(buffer); s > buffer; s--) {
			if (isspace(s[-1]))
				s[-1] = EOS;
			else
				break;
		}

		/*
		 * The markers for unified diff are a little different from the
		 * normal context-diff.  Also, the edit-lines in a unified diff
		 * won't have a space in column 2.
		 */
		if (match(buffer, "+++ ")
		 || match(buffer, "--- "))
		 	(void)strncpy(buffer, "***", 3);

		/*
		 * Use the first character of the input line to determine its
		 * type:
		 */
		switch (*buffer) {
		case 'O':	/* Only */
			if (match(buffer, "Only in ")) {
				char *path = buffer + 8;
				int found = FALSE;
				for (s = path; *s != EOS; s++) {
					if (match(s, ": ")) {
						found = TRUE;
						*s++ = PATHSEP;
						while ((s[0] = s[1]) != EOS)
							s++;
						break;
					}
				}
				if (found) {
					blip('.');
					this = new_data(path);
					this->cmt = Only;
					ok = HAVE_NOTHING;
				}
			}
			break;

			/*
			 * Several different scripts produce "Index:" lines
			 * (e.g., "makepatch").  Not all bother to put the
			 * pathname of the files; some put only the leaf names.
			 */
		case 'I':
			if (!match(buffer, "Index: "))
				break;
			s = strrchr(buffer, BLANK); /* last token is name */
			blip('.');
			this = new_data(s+1);
			ok = begin_data(this);
			break;

		case 'd':	/* diff command trace */
			if (!match(buffer, "diff "))
				break;
			s = strrchr(buffer, BLANK);
			blip('.');
			this = new_data(s+1);
			ok = begin_data(this);
			break;

		case '*':
			TRACE(("@%d, ok=%d:%s\n", __LINE__, ok, buffer))
			if (!(ok & HAVE_PATH)) {
				char	fname[BUFSIZ];
				char	skip[BUFSIZ];
				char	wday[BUFSIZ], mmm[BUFSIZ];
				int	ddd, hour, minute, second;
				int	day, month, year;

				/* check for tab-delimited first, so we can
				 * accept filenames containing spaces.
				 */
				if (sscanf(buffer,
				    "*** %[^\t]\t%[^ ] %[^ ] %d %d:%d:%d %d",
				    fname,
				    wday, mmm, &ddd,
				    &hour, &minute, &second, &year) == 8
				|| (sscanf(buffer,
				    "*** %[^\t]\t%d/%d/%d %d:%d:%d",
				    	fname,
					&year, &month, &day,
					&hour, &minute, &second) == 7
				  && !version_num(fname))
				|| sscanf(buffer,
				    "*** %[^\t ]%[\t ]%[^ ] %[^ ] %d %d:%d:%d %d",
				    fname,
				    skip,
				    wday, mmm, &ddd,
				    &hour, &minute, &second, &year) == 9
				|| (sscanf(buffer,
				    "*** %[^\t ]%[\t ]%d/%d/%d %d:%d:%d",
				    	fname,
					skip,
					&year, &month, &day,
					&hour, &minute, &second) == 8
				  && !version_num(fname))
				|| (sscanf(buffer,
				    "*** %[^\t ]%[\t ]",
				    	fname,
					skip) == 1
				  && !version_num(fname)
				  && !contain_any(fname, "*")
				  && !edit_range(fname))
				   ) {
					s = merge_name(this, fname);
					this = new_data(s);
					ok = begin_data(this);
					TRACE(("after merge:%d:%s\n", ok, s))
				}
			}
			break;

		case '+':
			if (buffer[1] == buffer[0])
				break;
			/* FALL-THRU */
		case '>':
			if (!ok)
				break;
			this->ins += 1;
			break;

		case '-':
			if (!ok)
				break;
			if (buffer[1] == buffer[0])
				break;
			/* fall-thru */
		case '<':
			if (!ok)
				break;
			this->del += 1;
			break;

		case '!':
			if (!ok)
				break;
			this->mod += 1;
			break;

			/* Expecting "Binary files XXX and YYY differ" */
		case 'B':	/* Binary */
			/* FALL-THRU */
		case 'b':	/* binary */
			if (match(buffer+1, "inary files ")) {
				s = strrchr(buffer, BLANK);
				if (!strcmp(s, " differ")) {
					*s = EOS;
					s = strrchr(buffer, BLANK);
					blip('.');
					this = new_data(s+1);
					this->cmt = Binary;
					ok = HAVE_NOTHING;
				}
			}
			break;
		}
	}
	blip('\n');
}

/*
 * Each call to 'plot_num()' prints a scaled bar of 'c' characters.  The
 * 'extra' parameter is used to keep the accumulated error in the bar's total
 * length from getting large.
 */
static
long	plot_num(num_value, c, extra)
	long	num_value;	/* the value to plot */
	int	c;		/* character to display in the bar */
	long	extra;		/* accumulated error in the bar */
{
	long	product	= (plot_width * num_value) + extra;
	long	count	= (product / plot_scale);
	extra = product - (count * plot_scale);
	while (--count >= 0)
		(void)putchar(c);
	return extra;
}

static
void	summarize()
{
	register DATA *p;
	long	total_ins = 0,
		total_del = 0,
		total_mod = 0,
		temp;
	int	num_files = 0,
		shortest_name = -1,
		longest_name  = -1,
		prefix_len    = -1;

	plot_scale = 0;
	for (p = all_data; p; p = p->link) {
		int	len = strlen(p->name);

		/*
		 * "-p0" gives the whole pathname unmodified.  "-p1" strips
		 * through the first path-separator, etc.
		 */
		if (prefix_opt >= 0) {
			int n, base;
			for (n = prefix_opt, base = 0; n > 0; n--) {
				char *s = strchr(p->name+base, PATHSEP);
				if (s == 0 || *++s == EOS)
					break;
				base = (int)(s - p->name);
			}
			p->base = base;
			if (name_wide < (len - base))
				name_wide = (len - base);
		} else {
			if (len < prefix_len || prefix_len < 0)
				prefix_len = len;
			while (prefix_len > 0) {
				if (p->name[prefix_len-1] != PATHSEP)
					prefix_len--;
				else if (strncmp(all_data->name, p->name, (size_t) prefix_len))
					prefix_len--;
				else
					break;
			}

			if (len > longest_name)
				longest_name = len;
			if (len < shortest_name || shortest_name < 0)
				shortest_name = len;
		}

		num_files++;
		total_ins += p->ins;
		total_del += p->del;
		total_mod += p->mod;
		temp = p->ins + p->del + p->mod;
		if (temp > plot_scale)
			plot_scale = temp;
	}

	if (prefix_opt < 0) {
		if (prefix_len < 0)
			prefix_len = 0;
		if ((longest_name - prefix_len) > name_wide)
			name_wide = (longest_name - prefix_len);
	}

	name_wide++;	/* make sure it's nonzero */
	plot_width = (max_width - name_wide - 8);
	if (plot_width < 10)
		plot_width = 10;

	if (plot_scale < plot_width)
		plot_scale = plot_width;	/* 1:1 */

	for (p = all_data; p; p = p->link) {
		printf(" %-*.*s|",
			name_wide, name_wide,
			p->name + (prefix_opt >= 0 ? p->base : prefix_len));
		switch (p->cmt) {
		default:
		case Normal:
			temp = 0;
			printf("%5ld ", p->ins + p->del + p->mod);
			temp = plot_num(p->ins, '+', temp);
			(void) plot_num(p->del, '-', temp);
			(void) plot_num(p->mod, '!', temp);
			break;
		case Binary:
			printf("binary");
			break;
		case Only:
			printf("only");
			break;
		}
		printf("\n");
	}

	printf(" %d files changed", num_files);
#define PLURAL(n) n, n != 1 ? "s" : ""
	if (total_ins) printf(", %ld insertion%s",    PLURAL(total_ins));
	if (total_del) printf(", %ld deletion%s",     PLURAL(total_del));
	if (total_mod) printf(", %ld modification%s", PLURAL(total_mod));
	(void)putchar('\n');
}

static
void	usage()
{
	static	char	*msg[] = {
	"Usage: diffstat [options] [files]",
	"",
	"Reads from one or more input files which contain output from 'diff',",
	"producing a histogram of total lines changed for each file referenced.",
	"If no filename is given on the command line, reads from stdin.",
	"",
	"Options:",
	"  -n NUM  specify minimum width for the filenames (default: auto)",
	"  -p NUM  specify number of pathname-separators to strip (default: common)",
	"  -w NUM  specify maximum width of the output (default: 80)",
	"  -V      prints the version number"
	};
	register int j;
	for (j = 0; j < sizeof(msg)/sizeof(msg[0]); j++)
		fprintf(stderr, "%s\n", msg[j]);
	exit (EXIT_FAILURE);
}

int	main(argc, argv)
	int	argc;
	char	*argv[];
{
	register int	j;
	char	version[80];

	max_width = 80;
	piped_output = !isatty(fileno(stdout))
		     && isatty(fileno(stderr));

	while ((j = getopt(argc, argv, "n:p:w:V")) != EOF) {
		switch (j) {
		case 'n':
			name_wide = atoi(optarg);
			break;
		case 'p':
			prefix_opt = atoi(optarg);
			break;
		case 'w':
			max_width = atoi(optarg);
			break;
		case 'V':
			if (!sscanf(Id, "%*s %*s %s", version))
				(void)strcpy(version, "?");
			printf("diffstat version %s (patch %d)\n",
				version,
				PATCHLEVEL);
			exit(EXIT_SUCCESS);
		default:
			usage();
			/*NOTREACHED*/
		}
	}

	if (optind < argc) {
		while (optind < argc) {
			FILE *fp;
			char *name = argv[optind++];
			if ((fp = fopen(name, "r")) != 0) {
				if (piped_output) {
					(void)fprintf(stderr, "%s\n", name);
					(void)fflush(stderr);
				}
				do_file(fp);
			} else {
				failed(name);
			}
		}
	} else {
		do_file(stdin);
	}
	summarize();
	exit(EXIT_SUCCESS);
	/*NOTREACHED*/
}
