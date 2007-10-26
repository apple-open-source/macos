/******************************************************************************
 * Copyright 1994-2004,2005 by Thomas E. Dickey                               *
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
static const char *Id = "$Id: diffstat.c,v 1.41 2005/08/24 20:47:34 tom Exp $";
#endif

/*
 * Title:	diffstat.c
 * Author:	T.E.Dickey
 * Created:	02 Feb 1992
 * Modified:
 *		24 Aug 2005, update usage message for -l, -r changes.
 *		15 Aug 2005, apply PLURAL() to num_files (Jean Delvare).
 *			     add -l option (request by Michael Burian).
 *			     Use fgetc_locked() if available.
 *		14 Aug 2005, add -r2 option (rounding with adjustment to ensure
 *			     that nonzero values always display a histogram
 *			     bar), adapted from patch by Jean Delvare.  Extend
 *			     the -f option (2=filled, 4=verbose).
 *		12 Aug 2005, modify to use tsearch() for sorted lists.
 *		11 Aug 2005, minor fixes to scaling of modified lines.  Add
 *			     -r (round) option.
 *		05 Aug 2005, add -t (table) option.
 *		10 Apr 2005, change order of merging and prefix-stripping so
 *			     stripping all prefixes, e.g., with -p9, will be
 *			     sorted as expected (Patch by Jean Delvare
 *			     <khali@linux-fr.org>).
 *		10 Jan 2005, add support for '--help' and '--version' (Patch
 *			     by Eric Blake <ebb9@byu.net>.)
 *		16 Dec 2004, fix a different case for data beginning with "--"
 *			     which was treated as a header line.
 *		14 Dec 2004, Fix allocation problems.  Open files in binary
 *			     mode for reading.  Getopt returns -1, not
 *			     necessarily EOF.  Add const where useful.  Use
 *			     NO_IDENT where necessary.  malloc() comes from
 *			     <stdlib.h> in standard systems (Patch by Eric
 *			     Blake <ebb9@byu.net>.)
 *		08 Nov 2004, minor fix for resync of unified diffs checks for
 *			     range (line beginning with '@' without header
 *			     lines (successive lines beginning with "---" and
 *			     "+++").  Fix a few problems reported by valgrind.
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
#endif

#if defined(HAVE_SEARCH_H) && defined(HAVE_TSEARCH)
#include <search.h>
#else
#undef HAVE_TSEARCH
#endif

#ifdef HAVE_FGETC_LOCKED
#define MY_FGETC fgetc_locked
#else
#define MY_FGETC fgetc
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

#define FMT_CONCISE  0
#define FMT_NORMAL   1
#define FMT_FILLED   2
#define FMT_VERBOSE  4

typedef enum comment {
    Normal, Only, Binary
} Comment;

#define MARKS 3			/* each of +, - and ! */

#define InsOf(p) (p)->count[0]	/* "+" count inserted lines */
#define DelOf(p) (p)->count[1]	/* "-" count deleted lines */
#define ModOf(p) (p)->count[2]	/* "!" count modified lines */

#define TotalOf(p) (InsOf(p) + DelOf(p) + ModOf(p))

typedef struct _data {
    struct _data *link;
    char *name;			/* the filename */
    int base;			/* beginning of name if -p option used */
    Comment cmt;
    long count[3];
} DATA;

static const char marks[MARKS + 1] = "+-!";

static DATA *all_data;
static char *comment_opt = "";
static int format_opt = FMT_NORMAL;
static int max_width;		/* the specified width-limit */
static int merge_names = 1;	/* true if we merge similar filenames */
static int name_wide;		/* the amount reserved for filenames */
static int names_only;		/* true if we list filenames only */
static int show_progress;	/* if not writing to tty, show progress */
static int plot_width;		/* the amount left over for histogram */
static int prefix_opt = -1;	/* if positive, controls stripping of PATHSEP */
static int round_opt = 0;	/* if nonzero, round data for histogram */
static int table_opt = 0;	/* if nonzero, write table rather than plot */
static int sort_names = 1;	/* true if we sort filenames */
static int verbose = 0;		/* -q/-v options */
static long plot_scale;		/* the effective scale (1:maximum) */

#ifdef HAVE_TSEARCH
static int use_tsearch;
static void *sorted_data;
#endif

static int prefix_len = -1;

/******************************************************************************/

static void
failed(const char *s)
{
    perror(s);
    exit(EXIT_FAILURE);
}

/* malloc wrapper that never returns NULL */
static void *
xmalloc(size_t s)
{
    void *p;
    if ((p = malloc(s)) == NULL)
	failed("malloc");
    return p;
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
new_string(const char *s)
{
    return strcpy((char *) xmalloc((size_t) (strlen(s) + 1)), s);
}

static int
compare_data(const void *a, const void *b)
{
    const DATA *p = (const DATA *) a;
    const DATA *q = (const DATA *) b;
    return strcmp(p->name + p->base, q->name + q->base);
}

static DATA *
new_data(char *name, int base)
{
    DATA *r = (DATA *) xmalloc(sizeof(DATA));

    memset(r, 0, sizeof(*r));
    r->name = new_string(name);
    r->base = base;
    r->cmt = Normal;

    return r;
}

static DATA *
find_data(char *name)
{
    DATA *p, *q, *r;
    DATA find;
    int base = 0;

    TRACE(("find_data(%s)\n", name));

    /* Compute the base offset if the prefix option is used */
    if (prefix_opt >= 0) {
	int n;

	for (n = prefix_opt; n > 0; n--) {
	    char *s = strchr(name + base, PATHSEP);
	    if (s == 0 || *++s == EOS)
		break;
	    base = s - name;
	}
	TRACE(("base set to %d\n", base));
    }

    /*
     * Setup parameter for compare_data().
     */
    memset(&find, 0, sizeof(find));
    find.name = name;
    find.base = base;
    find.cmt = Normal;

    /* Insert into sorted list (usually sorted).  If we are not sorting or
     * merging names, we fall off the end and link the new entry to the end of
     * the list.  If the prefix option is used, the prefix is ignored by the
     * merge and sort operations.
     *
     * If we have tsearch(), we will maintain the sorted list using it and
     * tfind().
     */
#ifdef HAVE_TSEARCH
    if (use_tsearch) {
	void *pp;
	if ((pp = tfind(&find, &sorted_data, compare_data)) != 0) {
	    p = *(DATA **) pp;
	    return p;
	}
	r = new_data(name, base);
	(void) tsearch(r, &sorted_data, compare_data);
	r->link = all_data;
	all_data = r;
    } else
#endif
    {
	for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
	    int cmp = compare_data(p, &find);
	    if (merge_names && (cmp == 0))
		return p;
	    if (sort_names && (cmp > 0))
		break;
	}
	r = new_data(name, base);
	if (q != 0)
	    q->link = r;
	else
	    all_data = r;

	r->link = p;
    }

    return r;
}

/*
 * Remove a unneeded data item from the linked list.  Free the name as well.
 */
static int
delink(DATA * data)
{
    DATA *p, *q;

    TRACE(("delink '%s'\n", data->name));

#ifdef HAVE_TSEARCH
    if (use_tsearch)
	if (tdelete(data, &sorted_data, compare_data) == 0)
	    return 0;
#endif
    for (p = all_data, q = 0; p != 0; q = p, p = p->link) {
	if (p == data) {
	    if (q != 0)
		q->link = p->link;
	    else
		all_data = p->link;
	    free(p->name);
	    free(p);
	    return 1;
	}
    }
    return 0;
}

/* like strncmp, but without the 3rd argument */
static int
match(const char *s, const char *p)
{
    int ok = 0;

    while (*s != EOS) {
	if (*p == EOS) {
	    ok = 1;
	    break;
	}
	if (*s++ != *p++)
	    break;
	if (*s == EOS && *p == EOS) {
	    ok = 1;
	    break;
	}
    }
    return ok;
}

static int
version_num(const char *s)
{
    int main_ver, sub_ver;
    char temp[2];
    return (sscanf(s, "%d.%d%c", &main_ver, &sub_ver, temp) == 2);
}

/*
 * Check for a range of line-numbers, used in editing scripts.
 */
static int
edit_range(const char *s)
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
decode_range(const char *s, int *first, int *second)
{
    char check;
    if (sscanf(s, "%d,%d%c", first, second, &check) == 2) {
	TRACE(("decode_range #1 first=%d, second=%d\n", *first, *second));
	return 1;
    } else if (sscanf(s, "%d%c", first, &check) == 1) {
	*second = *first;	/* diffutils 2.7 does this */
	TRACE(("decode_range #2 first=%d, second=%d\n", *first, *second));
	return 1;
    }
    return 0;
}

static int
HadDiffs(const DATA * data)
{
    return InsOf(data) != 0
	|| DelOf(data) != 0
	|| ModOf(data) != 0
	|| data->cmt != Normal;
}

/*
 * If the given path is not one of the "ignore" paths, then return true.
 */
static int
can_be_merged(const char *path)
{
    if (strcmp(path, "")
	&& strcmp(path, "/dev/null")
	&& strncmp(path, "/tmp/", 5))
	return 1;
    return 0;
}

static int
is_leaf(const char *theLeaf, const char *path)
{
    char *s;

    if (strchr(theLeaf, PATHSEP) == 0
	&& (s = strrchr(path, PATHSEP)) != 0
	&& !strcmp(++s, theLeaf))
	return 1;
    return 0;
}

static char *
do_merging(DATA * data, char *path, int *freed)
{
    TRACE(("do_merging(%s,%s) diffs:%d\n", data->name, path, HadDiffs(data)));

    *freed = 0;
    if (!HadDiffs(data)) {	/* the data was the first of 2 markers */
	if (is_leaf(data->name, path)) {
	    TRACE(("is_leaf: %s vs %s\n", data->name, path));
	    *freed = delink(data);
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

	    *freed = delink(data);
	    TRACE(("merge @%d, prefix_opt=%d matched=%d diff=%d\n",
		   __LINE__, prefix_opt, matched, diff));
	} else if (!can_be_merged(path)) {
	    TRACE(("do not merge, retain @%d\n", __LINE__));
	    /* must not merge, retain existing name */
	    path = data->name;
	} else {
	    TRACE(("merge @%d\n", __LINE__));
	    *freed = delink(data);
	}
    } else if (!can_be_merged(path)) {
	path = data->name;
    }
    TRACE(("...do_merging %s\n", path));
    return path;
}

static int
begin_data(const DATA * p)
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
    int n;

    if (*s == SQUOTE && len > 2 && s[len - 1] == SQUOTE) {
	for (n = 0; (s[n] = s[n + 1]) != EOS; ++n) {
	    ;
	}
	s[len - 2] = EOS;
    }
}

/*
 * Allocate a fixed-buffer
 */
static void
fixed_buffer(char **buffer, size_t want)
{
    *buffer = (char *) xmalloc(want);
}

/*
 * Reallocate a fixed-buffer
 */
static void
adjust_buffer(char **buffer, size_t want)
{
    if ((*buffer = (char *) realloc(*buffer, want)) == 0)
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

    while ((ch = MY_FGETC(fp)) != EOF) {
	if (used + 2 > *have) {
	    adjust_buffer(buffer, *have *= 2);
	}
	(*buffer)[used++] = (char) ch;
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
    DATA dummy;
    DATA *that = &dummy;
    DATA *prev = 0;
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
    int freed = 0;
    int old_unify = 0;
    int new_unify = 0;
    int context = 1;
    char *s;
#ifdef DEBUG
    int line_no = 0;
#endif

    memset(&dummy, 0, sizeof(dummy));
    dummy.name = "";

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
	TRACE(("[%05d] %s\n", ++line_no, buffer));

	/*
	 * The lines identifying files in a context diff depend on how it was
	 * invoked.  But after the header, each chunk begins with a line
	 * containing 15 *'s.  Each chunk may contain a line-range with '***'
	 * for the "before", and a line-range with '---' for the "after".  The
	 * part of the chunk depicting the deletion may be absent, though the
	 * edit line is present.
	 *
	 * The markers for unified diff are a little different from the normal
	 * context-diff.  Also, the edit-lines in a unified diff won't have a
	 * space in column 2.  Because of the missing space, we have to count
	 * lines to ensure we do not confuse the marker lines.
	 */
	marker = -1;
	if (that != &dummy && !strcmp(buffer, "***************")) {
	    TRACE(("begin context chunk\n"));
	    context = 2;
	} else if (context == 2 && match(buffer, "*** ")) {
	    context = 1;
	} else if (context == 1 && match(buffer, "--- ")) {
	    marker = 1;
	    context = 0;
	} else if (match(buffer, "*** ")) {
	    marker = 0;
	} else if ((old_unify + new_unify) == 0 && match(buffer, "--- ")) {
	    marker = unified = 1;
	} else if ((old_unify + new_unify) == 0 && match(buffer, "+++ ")) {
	    marker = unified = 2;
	} else if (unified == 2
		   || ((old_unify + new_unify) == 0 && (*buffer == '@'))) {
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
	} else if (unified == 1 && !context) {
	    /*
	     * If unified==1, we guessed we would find a "+++" line, but since
	     * we are here, we did not find that.  The context check ensures
	     * we do not mistake the "---" for a unified diff with that for
	     * a context diff's "after" line-range.
	     *
	     * If we guessed wrong, then we probably found a data line with
	     * "--" in the first two columns of the diff'd file.
	     */
	    unified = 0;
	    TRACE(("Expected \"+++\" @%d:%s\n", __LINE__, buffer));
	    if (prev != 0
		&& prev != that
		&& InsOf(that) == 0
		&& DelOf(that) == 0
		&& strcmp(prev->name, that->name)) {
		TRACE(("giveup on %ld/%ld %s\n", InsOf(that), DelOf(that), that->name));
		TRACE(("revert to %ld/%ld %s\n", InsOf(prev), DelOf(prev), prev->name));
		(void) delink(that);
		that = prev;
		DelOf(that) += 1;
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

	/*
	 * Override the beginning of the line to simplify the case statement
	 * below.
	 */
	if (marker > 0) {
	    TRACE(("@%d, marker=%d, override %s\n", __LINE__, marker, buffer));
	    (void) strncpy(buffer, "***", 3);
	}

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
		    that = find_data(path);
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
		s = do_merging(that, s, &freed);
		that = find_data(s);
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
		s = do_merging(that, s, &freed);
		that = find_data(s);
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
		    prev = that;
		    s = do_merging(that, b_fname, &freed);
		    if (freed)
			prev = 0;
		    that = find_data(s);
		    ok = begin_data(that);
		    TRACE(("after merge:%d:%s\n", ok, s));
		}
	    }
	    break;

	case '+':
	    /* FALL-THRU */
	case '>':
	    if (ok)
		InsOf(that) += 1;
	    break;

	case '-':
	    if (!ok)
		break;
	    if (!unified && !strcmp(buffer, "---"))
		break;
	    /* fall-thru */
	case '<':
	    if (ok)
		DelOf(that) += 1;
	    break;

	case '!':
	    if (ok)
		ModOf(that) += 1;
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
		    that = find_data(skip_blanks(s));
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

static long
plot_bar(long count, int c)
{
    long result = count;

    while (--count >= 0)
	(void) putchar(c);

    return result;
}

/*
 * Each call to 'plot_num()' prints a scaled bar of 'c' characters.  The
 * 'extra' parameter is used to keep the accumulated error in the bar's total
 * length from getting large.
 */
static long
plot_num(long num_value, int c, long *extra)
{
    long product;
    long result = 0;

    /* the value to plot */
    /* character to display in the bar */
    /* accumulated error in the bar */
    if (num_value) {
	product = (plot_width * num_value);
	result = ((product + *extra) / plot_scale);
	*extra = product - (result * plot_scale) - *extra;
	plot_bar(result, c);
    }
    return result;
}

static long
plot_round1(const long num[MARKS])
{
    long result = 0;
    long scaled[MARKS];
    long remain[MARKS];
    long want = 0;
    long have = 0;
    long half = (plot_scale / 2);
    int i, j;

    for (i = 0; i < MARKS; ++i) {
	long product = (plot_width * num[i]);
	scaled[i] = (product / plot_scale);
	remain[i] = (product % plot_scale);
	want += product;
	have += product - remain[i];
    }
    while (want > have) {
	for (i = 0, j = -1; i < MARKS; ++i) {
	    if (remain[i] != 0
		&& (remain[i] > (j >= 0 ? remain[j] : half))) {
		j = i;
	    }
	}
	if (j >= 0) {
	    have += remain[j];
	    remain[j] = 0;
	    scaled[j] += 1;
	} else {
	    break;
	}
    }
    for (i = 0; i < MARKS; ++i) {
	plot_bar(scaled[i], marks[i]);
	result += scaled[i];
    }
    return result;
}

/*
 * Print a scaled bar of characters, where c[0] is for insertions, c[1]
 * for deletions and c[2] for modifications. The num array contains the
 * count for each type of change, in the same order.
 */
static long
plot_round2(const long num[MARKS])
{
    long result = 0;
    long scaled[MARKS];
    long remain[MARKS];
    long total;
    int i;

    for (total = 0, i = 0; i < MARKS; i++)
	total += num[i];

    if (total == 0)
	return result;

    total = (total * plot_width + (plot_scale / 2)) / plot_scale;
    /* display at least one character */
    if (total == 0)
	total++;

    for (i = 0; i < MARKS; i++) {
	scaled[i] = num[i] * plot_width / plot_scale;
	remain[i] = num[i] * plot_width - scaled[i] * plot_scale;
	total -= scaled[i];
    }

    /* assign the missing chars using the largest remainder algo */
    while (total) {
	int largest, largest_count;	/* largest is a bit field */
	long max_remain;

	/* search for the largest remainder */
	largest = largest_count = 0;
	max_remain = 0;
	for (i = 0; i < MARKS; i++) {
	    if (remain[i] > max_remain) {
		largest = 1 << i;
		largest_count = 1;
		max_remain = remain[i];
	    } else if (remain[i] == max_remain) {	/* ex aequo */
		largest |= 1 << i;
		largest_count++;
	    }
	}

	/* if there are more greatest remainders than characters
	   missing, don't assign them at all */
	if (total < largest_count)
	    break;

	/* allocate the extra characters */
	for (i = 0; i < MARKS; i++) {
	    if (largest & (1 << i)) {
		scaled[i]++;
		total--;
		remain[i] -= plot_width;
	    }
	}
    }

    for (i = 0; i < MARKS; i++)
	result += plot_bar(scaled[i], marks[i]);

    return result;
}

static void
plot_numbers(const DATA * p)
{
    long temp = 0;
    long used = 0;
    int i;

    printf("%5ld ", TotalOf(p));

    if (format_opt & FMT_VERBOSE) {
	printf("%5ld ", InsOf(p));
	printf("%5ld ", DelOf(p));
	printf("%5ld ", ModOf(p));
    }

    if (format_opt == FMT_CONCISE) {
	for (i = 0; i < MARKS; i++) {
	    printf("\t%ld %c", p->count[i], marks[i]);
	}
    } else {
	switch (round_opt) {
	default:
	    for (i = 0; i < MARKS; ++i)
		used += plot_num(p->count[i], marks[i], &temp);
	    break;
	case 1:
	    used = plot_round1(p->count);
	    break;

	case 2:
	    used = plot_round2(p->count);
	    break;
	}

	if ((format_opt & FMT_FILLED) != 0) {
	    if (used > plot_width)
		printf("%ld", used - plot_width);	/* oops */
	    else
		plot_bar(plot_width - used, '.');
	}
    }
}

static void
show_data(const DATA * p)
{
    char *name = p->name + (prefix_opt >= 0 ? p->base : prefix_len);

    if (table_opt) {
	if (names_only) {
	    printf("%s\n", name);
	} else {
	    printf("%ld,%ld,%ld,%s\n",
		   InsOf(p),
		   DelOf(p),
		   ModOf(p),
		   name);
	}
    } else if (names_only) {
	printf("%s\n", name);
    } else {
	printf("%s %-*.*s|",
	       comment_opt,
	       name_wide, name_wide,
	       name);
	switch (p->cmt) {
	default:
	case Normal:
	    plot_numbers(p);
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
}

#ifdef HAVE_TSEARCH
static void
show_tsearch(const void *nodep, const VISIT which, const int depth)
{
    const DATA *p = *(DATA * const *) nodep;
    (void) depth;
    if (which == postorder || which == leaf)
	show_data(p);
}
#endif

static void
summarize(void)
{
    DATA *p;
    long total_ins = 0, total_del = 0, total_mod = 0, temp;
    int num_files = 0, shortest_name = -1, longest_name = -1;

    plot_scale = 0;
    for (p = all_data; p; p = p->link) {
	int len = strlen(p->name);

	/*
	 * "-p0" gives the whole pathname unmodified.  "-p1" strips
	 * through the first path-separator, etc.
	 */
	if (prefix_opt >= 0) {
	    /* p->base has been computed at node creation */
	    if (name_wide < (len - p->base))
		name_wide = (len - p->base);
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
	total_ins += InsOf(p);
	total_del += DelOf(p);
	total_mod += ModOf(p);
	temp = TotalOf(p);
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

    if (table_opt)
	printf("%sFILENAME\n",
	       (names_only ? "" : "INSERTED,DELETED,MODIFIED,"));

#ifdef HAVE_TSEARCH
    if (use_tsearch) {
	twalk(sorted_data, show_tsearch);
    } else
#endif
	for (p = all_data; p; p = p->link) {
	    show_data(p);
	}

    if (!table_opt && !names_only) {
#define PLURAL(n) n, n != 1 ? "s" : ""
	printf("%s %d file%s changed", comment_opt, PLURAL(num_files));
	if (total_ins)
	    printf(", %ld insertion%s(+)", PLURAL(total_ins));
	if (total_del)
	    printf(", %ld deletion%s(-)", PLURAL(total_del));
	if (total_mod)
	    printf(", %ld modification%s(!)", PLURAL(total_mod));
	(void) putchar('\n');
    }
}

#ifdef HAVE_POPEN
static char *
is_compressed(const char *name)
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
	result = (char *) xmalloc(strlen(verb) + len);
	sprintf(result, verb, name);
    }
    return result;

}
#endif

static void
usage(FILE *fp)
{
    static const char *msg[] =
    {
	"Usage: diffstat [options] [files]",
	"",
	"Reads from one or more input files which contain output from 'diff',",
	"producing a histogram of total lines changed for each file referenced.",
	"If no filename is given on the command line, reads from standard input.",
	"",
	"Options:",
	"  -c      prefix each line with comment (#)",
	"  -e FILE redirect standard error to FILE",
	"  -f NUM  format (0=concise, 1=normal, 2=filled, 4=values)",
	"  -h      print this message",
	"  -k      do not merge filenames",
	"  -l      list filenames only",
	"  -n NUM  specify minimum width for the filenames (default: auto)",
	"  -o FILE redirect standard output to FILE",
	"  -p NUM  specify number of pathname-separators to strip (default: common)",
	"  -r NUM  specify rounding for histogram (0=none, 1=simple, 2=adjusted)",
	"  -t      print a table (comma-separated-values) rather than histogram",
	"  -u      do not sort the input list",
	"  -v      makes output more verbose",
	"  -V      prints the version number",
	"  -w NUM  specify maximum width of the output (default: 80)",
    };
    unsigned j;
    for (j = 0; j < sizeof(msg) / sizeof(msg[0]); j++)
	fprintf(fp, "%s\n", msg[j]);
}

/* Wrapper around getopt that also parses "--help" and "--version".  
 * argc, argv, opts, return value, and globals optarg, optind,
 * opterr, and optopt are as in getopt().  help and version designate
 * what should be returned if --help or --version are encountered. */
static int
getopt_helper(int argc, char *const argv[], const char *opts,
	      int help, int version)
{
    if (optind < argc && argv[optind] != NULL) {
	if (strcmp(argv[optind], "--help") == 0) {
	    optind++;
	    return help;
	} else if (strcmp(argv[optind], "--version") == 0) {
	    optind++;
	    return version;
	}
    }
    return getopt(argc, argv, opts);
}

int
main(int argc, char *argv[])
{
    int j;
    char version[80];

    max_width = 80;

    while ((j = getopt_helper(argc, argv, "ce:f:hkln:o:p:r:tuvVw:", 'h', 'V'))
	   != -1) {
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
	case 'l':
	    names_only = 1;
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
	case 'r':
	    round_opt = atoi(optarg);
	    break;
	case 't':
	    table_opt = 1;
	    break;
	case 'u':
	    sort_names = 0;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'V':
#ifndef	NO_IDENT
	    if (!sscanf(Id, "%*s %*s %s", version))
#endif
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

#ifdef HAVE_TSEARCH
    use_tsearch = (sort_names && merge_names);
#endif

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
		}
		free(command);
	    } else
#endif
	    if ((fp = fopen(name, "rb")) != 0) {
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
	delink(all_data);
    }
#endif
    return (EXIT_SUCCESS);
}
