/*
 * Copyright (c) 2002 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/regex.c,v 1.10tsi Exp $ */

#include "lisp/regex.h"
#include "lisp/private.h"
#include "lisp/helper.h"

/*
 * Prototypes
 */
static re_cod *LispRecomp(LispBuiltin*, char*, int);

/*
 * Initialization
 */
LispObj *Knomatch;

/*
 * Implementation
 */
static re_cod *
LispRecomp(LispBuiltin *builtin, char *pattern, int cflags)
{
    int code;
    re_cod *regex = LispMalloc(sizeof(re_cod));

    if ((code = recomp(regex, pattern, cflags)) != 0) {
	char buffer[256];

	reerror(code, regex, buffer, sizeof(buffer));
	refree(regex);
	LispFree(regex);
	LispDestroy("%s: recomp(\"%s\"): %s", STRFUN(builtin), pattern, buffer);
    }

    return (regex);
}

void
LispRegexInit(void)
{
    Knomatch = KEYWORD("NOMATCH");
}

LispObj *
Lisp_Recomp(LispBuiltin *builtin)
/*
 re-comp pattern &key nospec icase nosub newline
 */
{
    re_cod *regex;
    int cflags = 0;

    LispObj *result;

    LispObj *pattern, *nospec, *icase, *nosub, *newline;

    newline = ARGUMENT(4);
    nosub = ARGUMENT(3);
    icase = ARGUMENT(2);
    nospec = ARGUMENT(1);
    pattern = ARGUMENT(0);

    /* Don't generate an error if it is already a compiled regex. */
    if (REGEXP(pattern))
	return (pattern);

    CHECK_STRING(pattern);

    if (nospec != UNSPEC && nospec != NIL)
	cflags |= RE_NOSPEC;
    if (icase != UNSPEC && icase != NIL)
	cflags |= RE_ICASE;
    if (nosub != UNSPEC && nosub != NIL)
	cflags |= RE_NOSUB;
    if (newline != UNSPEC && newline != NIL)
	cflags |= RE_NEWLINE;

    regex = LispRecomp(builtin, THESTR(pattern), cflags);
    result = LispNew(pattern, NIL);
    result->type = LispRegex_t;
    result->data.regex.regex = regex;
    result->data.regex.pattern = pattern;
    result->data.regex.options = cflags;
    LispMused(regex);

    return (result);
}

LispObj *
Lisp_Reexec(LispBuiltin *builtin)
/*
 re-exec regex string &key count start end notbol noteol
 */
{
    size_t nmatch;
    re_mat match[10];
    long start, end, length;
    int code, cflags, eflags;
    char *string;
    LispObj *result;
    re_cod *regexp;

    LispObj *regex, *ostring, *count, *ostart, *oend, *notbol, *noteol;

    noteol = ARGUMENT(6);
    notbol = ARGUMENT(5);
    oend = ARGUMENT(4);
    ostart = ARGUMENT(3);
    count = ARGUMENT(2);
    ostring = ARGUMENT(1);
    regex = ARGUMENT(0);

    if (STRINGP(regex))
	regexp = LispRecomp(builtin, THESTR(regex), cflags = 0);
    else {
	CHECK_REGEX(regex);
	regexp = regex->data.regex.regex;
	cflags = regex->data.regex.options;
    }

    CHECK_STRING(ostring);

    if (count == UNSPEC)
	nmatch = 1;
    else {
	CHECK_INDEX(count);
	nmatch = FIXNUM_VALUE(count);
	if (nmatch > 10)
	    LispDestroy("%s: COUNT cannot be larger than 10", STRFUN(builtin));
    }
    if (nmatch && (cflags & RE_NOSUB))
	nmatch = 1;

    eflags = RE_STARTEND;
    if (notbol != UNSPEC && notbol != NIL)
	eflags |= RE_NOTBOL;
    if (noteol != UNSPEC && noteol != NIL)
	eflags |= RE_NOTEOL;

    string = THESTR(ostring);
    LispCheckSequenceStartEnd(builtin, ostring, ostart, oend,
			      &start, &end, &length);

    match[0].rm_so = start;
    match[0].rm_eo = end;
    code = reexec(regexp, string, nmatch, &match[0], eflags);

    if (code == 0) {
	if (nmatch && match[0].rm_eo >= match[0].rm_so) {
	    result = CONS(CONS(FIXNUM(match[0].rm_so),
			       FIXNUM(match[0].rm_eo)), NIL);
	    if (nmatch > 1 && match[1].rm_eo >= match[1].rm_so) {
		int i;
		GC_ENTER();
		LispObj *cons = result;

		GC_PROTECT(result);
		for (i = 1;
		     i < nmatch && match[i].rm_eo >= match[i].rm_so;
		     i++) {
		    RPLACD(cons, CONS(CONS(FIXNUM(match[i].rm_so),
					   FIXNUM(match[i].rm_eo)), NIL));
		    cons = CDR(cons);
		}
		GC_LEAVE();
	    }
	}
	else
	    result = NIL;
    }
    else
	result = Knomatch;

    /* Maybe shoud cache compiled regex, but better the caller do it */
    if (!XREGEXP(regex)) {
	refree(regexp);
	LispFree(regexp);
    }

    return (result);
}

LispObj *
Lisp_Rep(LispBuiltin *builtin)
/*
 re-p object
 */
{
    LispObj *object;

    object = ARGUMENT(0);

    return (REGEXP(object) ? T : NIL);
}
