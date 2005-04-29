/******************************************************************************
 * Copyright 1994-2002,2003 by Thomas E. Dickey                               *
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
static char *Id = "$Id: diffstat.c,v 1.34 2003/11/09 18:45:00 tom Exp $";
#endif

/*
 * Title:	diffstat.c
 * Author:	T.E.Dickey
 * Created:	02 Feb 1992
 * Modified:
 *		09 Nov 2003, modify check for lines beginning with '-' or '+'
 *			     to treat only "---" in old-style diffs as a
 *			     special case.
 *		14 Feb 2003, modify check for filenames to allow for some cases
 *			     of incomplete dates (the reported example omitted
 *			     the day of the month).  Correct a typo in usage().
 *			     Add -e, -h, -o options.
 *		04 Jan 2003, improve tracking of chunks in unified diff, in
 *			     case the original files contained a '+' or '-' in
 *			     the first column (Debian #155000).  Add -v option
 *			     (Debian #170947).  Modify to allocate buffers big
 *			     enough for long input lines.  Do additional
 *			     merging to handle unusual Index/diff constructs in
 *			     recent makepatch script.
 *		20 Aug 2002, add -u option to tell diffstat to preserve the
 *			     order of filenames as given rather than sort them
 *			     (request by H Peter Anvin <hpa@zytor.com>).  Add
 *			     -k option for completeness.
 *		09 Aug 2002, allow either '/' or '-' as delimiters in dates,
 *			     to accommodate diffutils 2.8 (report by Rik van
 *			     Riel <riel@conectiva.com.br>).
 *		10 Oct 2001, add bzip2 (.bz2) suffix as suggested by
 *			     Gregory T Norris <haphazard@socket.net> in Debian
 *			     bug report #82969).
 *			     add check for diff from RCS archive where the
 *			     "diff" lines do not reference a filename.
 *		29 Mar 2000, add -c option.  Check for compressed input, read
 *			     via pipe.  Change to ANSI C.  Adapted change from
 *			     Troy Engel to add option that displays a number
 *			     only, rather than a histogram.
 *		17 May 1998, handle Debian diff files, which do not contain
 *			     dates on the header lines.
 *		16 Jan 1998, accommodate patches w/o tabs in header lines (e.g.,
 *			     from cut/paste).  Strip suffixes such as ".orig".
 *		24 Mar 1996, corrected -p0 logic, more fixes in do_merging.
 *		16 Mar 1996, corrected state-change for "Binary".  Added -p
 *			     option.
 *		17 Dec 1995, corrected matching algorithm in 'do_merging()'
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

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#ifdef WIN32
#define HAVE_STDLIB_H
#define HAVE_STRING_H
#define HAVE_MALLOC_H
#define HAVE_GETOPT_H
#endif

#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#define strchr index
#define strrchr rindex
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern int atoi();
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
extern int isatty();
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
extern char *malloc();
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
extern int getopt();
extern char *optarg;
extern int optind;
#endif

#if !defined(EXIT_SUCCESS)
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif

/******************************************************************************/

#ifdef WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

#define SQUOTE  '\''
#define EOS     '\0'
#define BLANK   ' '

#define UC(c)   ((unsigned char)(c))

#ifdef DEBUG
#define TRACE(p) printf p
#else
#define TRACE(p)		/*nothing */
#endif

#define contain_any(s,reject) (strcspn(s,reject) != strlen(s))

#define HAVE_NOTHING 0
#define HAVE_GENERIC 1		/* e.g., "Index: foo" w/o pathname */
#define HAVE_PATH    2		/* reference-file from "diff dirname/foo" */
#define HAVE_PATH2   4		/* comparison-file from "diff dirname/foo" */

typedef enum comment {
    Normal, Only, Binary
} Comment;

typedef struct _data {
    struct _data *link;
    char *name;			/* the filename */
    int base;			/* beginning of name if -p option used */
    Comment cmt;
    long ins;			/* "+" count inserted lines */
    long del;			/* "-" count deleted lines */
    long mod;			/* "!" count modified lines */
} DATA;

static DATA *all_data;
static char *comment_opt = "";
static int format_opt = 1;
static int max_width;		/* the specified width-limit */
static int merge_names = 1;	/* true if we merge similar filenames */
static int name_wide;		/* the amount reserved for filenames */
static int show_progress;	/* if not writing to tty, show progress */
static int plot_width;		/* the amount left over for histogram */
static int prefix_opt = -1;	/* if positive, controls stripping of PATHSEP */
static int sort_names = 1;	/* true if we sort filenames */
static int verbose = 0;		/* -q/-v options */
static long plot_scale;		/* the effective scale (1:maximum) */

/******************************************************************************/

static void
failed(char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

static void
blip(int c)
{
    if (show_progress) {
	(void) fputc(c, stderr);
	(void) fflush(stderr);
    }
}

static char *
new_string(char *s)
{
    return strcpy((char *) malloc((unsigned) (strlen(s) + 1)), s);
}

static DATA *
new_data(char *name)
{
    DATA *p, *q, *r;

    TRACE(("new_data(%s)\n", name));

    /* Insert into sorted list (usually sorted).  If we are not sorting or
     * merging names, we fall off the end and link the new entry to the end of
     * the list.
     */
    for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
	int cmp = strcmp(p->name, name);
	if (merge_names && (cmp == 0))
	    return p;
	if (sort_names && (cmp > 0))
	    break;
    }
    r = (DATA *) malloc(sizeof(DATA));
    if (q != 0)
	q->link = r;
    else
	all_data = r;

    r->link = p;
    r->name = new_string(name);
    r->base = 0;
    r->cmt = Normal;
    r->ins = 0;
    r->del = 0;
    r->mod = 0;

    return r;
}

/*
 * Remove a unneeded data item from the linked list.  Don't free the name,
 * since we may want it in another context.
 */
static void
delink(DATA * data)
{
    DATA *p, *q;

    TRACE(("delink '%s'\n", data->name));

    for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
	if (p == data) {
	    if (q != 0)
		q->link = p->link;
	    else
		all_data = p->link;
	    free(p);
	    return;
	}
    }
}

/* like strncmp, but without the 3rd argument */
static int
match(char *s, char *p)
{
    int ok = 0;

    while (*s != EOS) {
	if (*p == EOS) {
	    ok = 1;
	    break;
	}
	if (*s++ != *p++)
	    break;
    }
    return ok;
}

static int
version_num(char *s)
{
    int main_ver, sub_ver;
    char temp[2];
    return (sscanf(s, "%d.%d%c", &main_ver, &sub_ver, temp) == 2);
}

/*
 * Check for a range of line-numbers, used in editing scripts.
 */
static int
edit_range(char *s)
{
    int first, last;
    char temp[2];
    return (sscanf(s, "%d,%d%c", &first, &last, temp) == 2)
	|| (sscanf(s, "%d%c", &first, temp) == 1);
}

/*
 * Decode a range for unified diff.  Oddly, the comments in diffutils code
 * claim that both numbers are line-numbers.  However, inspection of the output
 * shows that the numbers are a line-number followed by a count.
 */
static int
decode_range(char *s, int *first, int *second)
{
    char check;
    if (sscanf(s, "%d,%d%c", first, second, &check) == 2) {
	return 1;
    } else if (sscanf(s, "%d%c", first, &check) == 1) {
	*second = *first;	/* diffutils 2.7 does this */
	return 1;
    }
    return 0;
}

static int
HadDiffs(DATA * data)
{
    return data->ins != 0
	|| data->del != 0
	|| data->mod != 0
	|| data->cmt != Normal;
}

/*
 * If the given path is not one of the "ignore" paths, then return true.
 */
static int
can_be_merged(char *path)
{
    if (strcmp(path, "")
	&& strcmp(path, "/dev/null")
	&& strncmp(path, "/tmp/", 5))
	return 1;
    return 0;
}

static int
is_leaf(char *leaf, char *path)
{
    char *s;

    if (strchr(leaf, PATHSEP) == 0
	&& (s = strrchr(path, PATHSEP)) != 0
	&& !strcmp(++s, leaf))
	return 1;
    return 0;
}

static char *
do_merging(DATA * data, char *path)
{
    TRACE(("do_merging(%s,%s) diffs:%d\n", data->name, path, HadDiffs(data)));

    if (!HadDiffs(data)) {	/* the data was the first of 2 markers */
	if (is_leaf(data->name, path)) {
	    TRACE(("is_leaf: %s vs %s\n", data->name, path));
	    delink(data);
	} else if (can_be_merged(data->name)
		   && can_be_merged(path)) {
	    size_t len1 = strlen(data->name);
	    size_t len2 = strlen(path);
	    unsigned n;
	    int matched = 0;
	    int diff = 0;

	    /* strip suffixes such as ".orig", ".bak" */
	    if (len1 > len2) {
		if (!strncmp(data->name, path, len2)) {
		    TRACE(("trimming data '%s' to '%.*s'\n",
			   data->name, (int) len2, data->name));
		    data->name[len1 = len2] = EOS;
		}
	    } else if (len1 < len2) {
		if (!strncmp(data->name, path, len1)) {
		    TRACE(("trimming path '%s' to '%.*s'\n",
			   path, (int) len1, path));
		    path[len2 = len1] = EOS;
		}
	    }

	    for (n = 1; n <= len1 && n <= len2; n++) {
		if (data->name[len1 - n] != path[len2 - n]) {
		    diff = n;
		    break;
		}
		if (path[len2 - n] == PATHSEP)
		    matched = n;
	    }

	    if (prefix_opt < 0
		&& matched != 0
		&& diff)
		path += len2 - matched + 1;

	    delink(data);
	    TRACE(("merge @%d, prefix_opt=%d matched=%d diff=%d\n",
		   __LINE__, prefix_opt, matched, diff));
	} else if (!can_be_merged(path)) {
	    TRACE(("do not merge, retain @%d\n", __LINE__));
	    /* must not merge, retain existing name */
	    path = data->name;
	} else {
	    TRACE(("merge @%d\n", __LINE__));
	    delink(data);
	}
    } else if (!can_be_merged(path)) {
	path = data->name;
    }
    TRACE(("...do_merging %s\n", path));
    return path;
}

static int
begin_data(DATA * p)
{
    if (!can_be_merged(p->name)
	&& strchr(p->name, PATHSEP) != 0) {
	TRACE(("begin_data:HAVE_PATH\n"));
	return HAVE_PATH;
    }
    TRACE(("begin_data:HAVE_GENERIC\n"));
    return HAVE_GENERIC;
}

static char *
skip_blanks(char *s)
{
    while (isspace(UC(*s)))
	++s;
    return s;
}

static char *
skip_options(char *params)
{
    while (*params != '\0') {
	params = skip_blanks(params);
	if (*params == '-') {
	    while (isgraph(UC(*params)))
		params++;
	} else {
	    break;
	}
    }
    return params;
}

/*
 * Strip single-quotes from a name (needed for recent makepatch versions).
 */
static void
dequote(char *s)
{
    int len = strlen(s);

    if (*s == SQUOTE && len > 2 && s[len - 1] == SQUOTE) {
	strcpy(s, s + 1);
	s[len - 2] = EOS;
    }
}

/*
 * Allocate a fixed-buffer
 */
static void
fixed_buffer(char **buffer, size_t want)
{
    if ((*buffer = malloc(want)) == 0)
	failed("malloc");
}

/*
 * Reallocate a fixed-buffer
 */
static void
adjust_buffer(char **buffer, size_t want)
{
    if ((*buffer = realloc(*buffer, want)) == 0)
	failed("realloc");
}

/*
 * Read until newline or end-of-file, allocating the line-buffer so it is long
 * enough for the input.
 */
static int
get_line(char **buffer, size_t *have, FILE *fp)
{
    int ch;
    size_t used = 0;

    while ((ch = fgetc(fp)) != EOF) {
	if (used + 2 > *have) {
	    adjust_buffer(buffer, *have *= 2);
	}
	(*buffer)[used++] = ch;
	if (ch == '\n')
	    break;
    }
    (*buffer)[used] = '\0';
    return (used != 0);
}

#define date_delims(a,b) (((a)=='/' && (b)=='/') || ((a) == '-' && (b) == '-'))

static void
do_file(FILE *fp)
{
    DATA dummy, *that = &dummy;
    char *buffer = 0;
    char *b_fname = 0;
    char *b_temp1 = 0;
    char *b_temp2 = 0;
    char *b_temp3 = 0;
    size_t length = 0;
    size_t fixed = 0;
    int ok = HAVE_NOTHING;
    int marker = -1;
    int unified = 0;
    int old_unify = 0;
    int new_unify = 0;
    char *s;

    dummy.name = "";
    dummy.ins = 0;
    dummy.del = 0;
    dummy.mod = 0;

    fixed_buffer(&buffer, fixed = length = BUFSIZ);
    fixed_buffer(&b_fname, length);
    fixed_buffer(&b_temp1, length);
    fixed_buffer(&b_temp2, length);
    fixed_buffer(&b_temp3, length);

    while (get_line(&buffer, &length, fp)) {
	/*
	 * Adjust size of fixed-buffers so that a sscanf cannot overflow.
	 */
	if (length > fixed) {
	    fixed = length;
	    adjust_buffer(&b_fname, length);
	    adjust_buffer(&b_temp1, length);
	    adjust_buffer(&b_temp2, length);
	    adjust_buffer(&b_temp3, length);
	}

	/*
	 * Trim trailing blanks (e.g., newline)
	 */
	for (s = buffer + strlen(buffer); s > buffer; s--) {
	    if (isspace(UC(s[-1])))
		s[-1] = EOS;
	    else
		break;
	}

	/*
	 * The markers for unified diff are a little different from the normal
	 * context-diff.  Also, the edit-lines in a unified diff won't have a
	 * space in column 2.  Because of the missing space, we have to count
	 * lines to ensure we do not confuse the marker lines.
	 */
	marker = -1;
	if (match(buffer, "*** ")) {
	    marker = 0;
	} else if ((old_unify + new_unify) == 0 && match(buffer, "--- ")) {
	    marker = unified = 1;
	} else if ((old_unify + new_unify) == 0 && match(buffer, "+++ ")) {
	    marker = unified = 2;
	} else if (unified == 2) {
	    unified = 0;
	    if (*buffer == '@') {
		int old_base, new_base, old_size, new_size;
		char test_at;

		old_unify = new_unify = 0;
		if (sscanf(buffer, "@@ -%[0-9,] +%[0-9,] @%c",
			   b_temp1,
			   b_temp2,
			   &test_at) == 3
		    && test_at == '@'
		    && decode_range(b_temp1, &old_base, &old_size)
		    && decode_range(b_temp2, &new_base, &new_size)) {
		    old_unify = old_size;
		    new_unify = new_size;
		    unified = -1;
		}
	    }
	} else if (old_unify + new_unify) {
	    switch (*buffer) {
	    case '-':
		if (old_unify)
		    --old_unify;
		break;
	    case '+':
		if (new_unify)
		    --new_unify;
		break;
	    case ' ':
		if (old_unify)
		    --old_unify;
		if (new_unify)
		    --new_unify;
		break;
	    default:
		old_unify = new_unify = 0;
		break;
	    }
	} else {
	    unified = 0;
	}
	if (marker > 0)
	    (void) strncpy(buffer, "***", 3);

	/*
	 * Use the first character of the input line to determine its
	 * type:
	 */
	switch (*buffer) {
	case 'O':		/* Only */
	    if (match(buffer, "Only in ")) {
		char *path = buffer + 8;
		int found = 0;
		for (s = path; *s != EOS; s++) {
		    if (match(s, ": ")) {
			found = 1;
			*s++ = PATHSEP;
			while ((s[0] = s[1]) != EOS)
			    s++;
			break;
		    }
		}
		if (found) {
		    blip('.');
		    that = new_data(path);
		    that->cmt = Only;
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
	    if (match(buffer, "Index: ")) {
		s = strrchr(buffer, BLANK);	/* last token is name */
		s = skip_blanks(s);
		dequote(s);
		blip('.');
		s = do_merging(that, s);
		that = new_data(s);
		ok = begin_data(that);
	    }
	    break;

	case 'd':		/* diff command trace */
	    if (match(buffer, "diff ")
		&& *(s = skip_options(buffer + 5)) != '\0') {
		s = strrchr(buffer, BLANK);
		s = skip_blanks(s);
		dequote(s);
		blip('.');
		s = do_merging(that, s);
		that = new_data(s);
		ok = begin_data(that);
	    }
	    break;

	case '*':
	    TRACE(("@%d, ok=%d:%s\n", __LINE__, ok, buffer));
	    if (!(ok & HAVE_PATH)) {
		int ddd, hour, minute, second;
		int day, month, year;
		char yrmon, monday;

		/* check for tab-delimited first, so we can
		 * accept filenames containing spaces.
		 */
		if (sscanf(buffer,
			   "*** %[^\t]\t%[^ ] %[^ ] %d %d:%d:%d %d",
			   b_fname,
			   b_temp2, b_temp3, &ddd,
			   &hour, &minute, &second, &year) == 8
		    || (sscanf(buffer,
			       "*** %[^\t]\t%d%c%d%c%d %d:%d:%d",
			       b_fname,
			       &year, &yrmon, &month, &monday, &day,
			       &hour, &minute, &second) == 9
			&& date_delims(yrmon, monday)
			&& !version_num(b_fname))
		    || sscanf(buffer,
			      "*** %[^\t ]%[\t ]%[^ ] %[^ ] %d %d:%d:%d %d",
			      b_fname,
			      b_temp1,
			      b_temp2, b_temp3, &ddd,
			      &hour, &minute, &second, &year) == 9
		    || (sscanf(buffer,
			       "*** %[^\t ]%[\t ]%d%c%d%c%d %d:%d:%d",
			       b_fname,
			       b_temp1,
			       &year, &yrmon, &month, &monday, &day,
			       &hour, &minute, &second) == 10
			&& date_delims(yrmon, monday)
			&& !version_num(b_fname))
		    || (sscanf(buffer,
			       "*** %[^\t ]%[\t ]",
			       b_fname,
			       b_temp1) >= 1
			&& !version_num(b_fname)
			&& !contain_any(b_fname, "*")
			&& !edit_range(b_fname))
		    ) {
		    s = do_merging(that, b_fname);
		    that = new_data(s);
		    ok = begin_data(that);
		    TRACE(("after merge:%d:%s\n", ok, s));
		}
	    }
	    break;

	case '+':
	    /* FALL-THRU */
	case '>':
	    if (ok)
		that->ins += 1;
	    break;

	case '-':
	    if (!ok)
		break;
	    if (!unified && !strcmp(buffer, "---"))
		break;
	    /* fall-thru */
	case '<':
	    if (ok)
		that->del += 1;
	    break;

	case '!':
	    if (ok)
		that->mod += 1;
	    break;

	    /* Expecting "Binary files XXX and YYY differ" */
	case 'B':		/* Binary */
	    /* FALL-THRU */
	case 'b':		/* binary */
	    if (match(buffer + 1, "inary files ")) {
		s = strrchr(buffer, BLANK);
		if (!strcmp(s, " differ")) {
		    *s = EOS;
		    s = strrchr(buffer, BLANK);
		    blip('.');
		    that = new_data(skip_blanks(s));
		    that->cmt = Binary;
		    ok = HAVE_NOTHING;
		}
	    }
	    break;
	}
    }
    blip('\n');

    if (buffer != 0) {
	free(buffer);
	free(b_fname);
	free(b_temp1);
	free(b_temp2);
	free(b_temp3);
    }
}

/*
 * Each call to 'plot_num()' prints a scaled bar of 'c' characters.  The
 * 'extra' parameter is used to keep the accumulated error in the bar's total
 * length from getting large.
 */
static long
plot_num(long num_value, int c, long extra)
{
    switch (format_opt) {
    case 0:
	printf("\t%ld %c", num_value, c);
	return 0;
    default:
	/* the value to plot */
	/* character to display in the bar */
	/* accumulated error in the bar */
	{
	    long product = (plot_width * num_value) + extra;
	    long count = (product / plot_scale);
	    extra = product - (count * plot_scale);
	    while (--count >= 0)
		(void) putchar(c);
	    return extra;
	}
    }
}

static void
summarize(void)
{
    DATA *p;
    long total_ins = 0, total_del = 0, total_mod = 0, temp;
    int num_files = 0, shortest_name = -1, longest_name = -1, prefix_len = -1;

    plot_scale = 0;
    for (p = all_data; p; p = p->link) {
	int len = strlen(p->name);

	/*
	 * "-p0" gives the whole pathname unmodified.  "-p1" strips
	 * through the first path-separator, etc.
	 */
	if (prefix_opt >= 0) {
	    int n, base;
	    for (n = prefix_opt, base = 0; n > 0; n--) {
		char *s = strchr(p->name + base, PATHSEP);
		if (s == 0 || *++s == EOS)
		    break;
		base = (int) (s - p->name);
	    }
	    p->base = base;
	    if (name_wide < (len - base))
		name_wide = (len - base);
	} else {
	    if (len < prefix_len || prefix_len < 0)
		prefix_len = len;
	    while (prefix_len > 0) {
		if (p->name[prefix_len - 1] != PATHSEP)
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

    name_wide++;		/* make sure it's nonzero */
    plot_width = (max_width - name_wide - 8);
    if (plot_width < 10)
	plot_width = 10;

    if (plot_scale < plot_width)
	plot_scale = plot_width;	/* 1:1 */

    for (p = all_data; p; p = p->link) {
	printf("%s %-*.*s|",
	       comment_opt,
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

    printf("%s %d files changed", comment_opt, num_files);
#define PLURAL(n) n, n != 1 ? "s" : ""
    if (total_ins)
	printf(", %ld insertion%s(+)", PLURAL(total_ins));
    if (total_del)
	printf(", %ld deletion%s(-)", PLURAL(total_del));
    if (total_mod)
	printf(", %ld modification%s(!)", PLURAL(total_mod));
    (void) putchar('\n');
}

#ifdef HAVE_POPEN
static char *
is_compressed(char *name)
{
    char *verb = 0;
    char *result = 0;
    size_t len = strlen(name);

    if (len > 2 && !strcmp(name + len - 2, ".Z")) {
	verb = "compress -dc %s";
    } else if (len > 3 && !strcmp(name + len - 3, ".gz")) {
	verb = "gzip -dc %s";
    } else if (len > 4 && !strcmp(name + len - 4, ".bz2")) {
	verb = "bzip2 -dc %s";
    }
    if (verb != 0) {
	result = (char *) malloc(strlen(verb) + len);
	sprintf(result, verb, name);
    }
    return result;

}
#endif

static void
usage(FILE *fp)
{
    static char *msg[] =
    {
	"Usage: diffstat [options] [files]",
	"",
	"Reads from one or more input files which contain output from 'diff',",
	"producing a histogram of total lines changed for each file referenced.",
	"If no filename is given on the command line, reads from stdin.",
	"",
	"Options:",
	"  -c      prefix each line with comment (#)",
	"  -e FILE redirect standard error to FILE",
	"  -f NUM  format (0=concise, 1=normal)",
	"  -h      print this message",
	"  -k      do not merge filenames",
	"  -n NUM  specify minimum width for the filenames (default: auto)",
	"  -o FILE redirect standard output to FILE",
	"  -p NUM  specify number of pathname-separators to strip (default: common)",
	"  -u      do not sort the input list",
	"  -v      makes output more verbose",
	"  -V      prints the version number",
	"  -w NUM  specify maximum width of the output (default: 80)",
    };
    unsigned j;
    for (j = 0; j < sizeof(msg) / sizeof(msg[0]); j++)
	fprintf(fp, "%s\n", msg[j]);
}

int
main(int argc, char *argv[])
{
    int j;
    char version[80];

    max_width = 80;

    while ((j = getopt(argc, argv, "ce:f:hkn:o:p:uvVw:")) != EOF) {
	switch (j) {
	case 'c':
	    comment_opt = "#";
	    break;
	case 'e':
	    if (freopen(optarg, "w", stderr) == 0)
		failed(optarg);
	    break;
	case 'f':
	    format_opt = atoi(optarg);
	    break;
	case 'h':
	    usage(stdout);
	    return (EXIT_SUCCESS);
	case 'k':
	    merge_names = 0;
	    break;
	case 'n':
	    name_wide = atoi(optarg);
	    break;
	case 'o':
	    if (freopen(optarg, "w", stdout) == 0)
		failed(optarg);
	    break;
	case 'p':
	    prefix_opt = atoi(optarg);
	    break;
	case 'u':
	    sort_names = 0;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'V':
	    if (!sscanf(Id, "%*s %*s %s", version))
		(void) strcpy(version, "?");
	    printf("diffstat version %s\n", version);
	    return (EXIT_SUCCESS);
	case 'w':
	    max_width = atoi(optarg);
	    break;
	default:
	    usage(stderr);
	    return (EXIT_FAILURE);
	}
    }
    show_progress = verbose && (!isatty(fileno(stdout))
				&& isatty(fileno(stderr)));

    if (optind < argc) {
	while (optind < argc) {
	    FILE *fp;
	    char *name = argv[optind++];
#ifdef HAVE_POPEN
	    char *command = is_compressed(name);
	    if (command != 0) {
		if ((fp = popen(command, "r")) != 0) {
		    if (show_progress) {
			(void) fprintf(stderr, "%s\n", name);
			(void) fflush(stderr);
		    }
		    do_file(fp);
		    (void) pclose(fp);
		    free(command);
		}
	    } else
#endif
	    if ((fp = fopen(name, "r")) != 0) {
		if (show_progress) {
		    (void) fprintf(stderr, "%s\n", name);
		    (void) fflush(stderr);
		}
		do_file(fp);
		(void) fclose(fp);
	    } else {
		failed(name);
	    }
	}
    } else {
	do_file(stdin);
    }
    summarize();
#if defined(DEBUG) || defined(NO_LEAKS)
    while (all_data != 0) {
	DATA *p = all_data;
	all_data = all_data->link;
	free(p->name);
	free(p);
    }
#endif
    return (EXIT_SUCCESS);
}
