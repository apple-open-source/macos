/*
 * Copyright (c) 1999 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/hook.c,v 1.10 2003/01/09 03:36:29 paulo Exp $ */

/*
 * This file is intended to be used to add all the necessary hooks to xedit
 * emulate certain features of emacs (and other text editors) that are better
 * kept only in xedit, to avoid unnecessary code in the Text Widget.
 *
 * The code here is not finished, and will probably be changed frequently.
 */

#include "xedit.h"
#include "re.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Types
 */
typedef struct _ReplaceList {
    char *word;
    char *replace;
    struct _ReplaceList *next;
} ReplaceList;

typedef enum {
    SubstituteDisabled,
    SubstituteAsk,
    SubstituteNo,
    SubstituteYes
} SubstitutionState;

typedef struct _EditInfo {
    /* Xedit regex data */
    re_cod regex;
    re_mat mats[10];

    /* Last command entered */
    char command[128];

    /* String and flags used to compile regex */
    char pattern[64];
    int flags;

    /* Substitution buffer */
    char subst[64];
    int soff, slen, sref;

    /* For interactive substitution */
    int callback;
    Widget widget;
    char *text_line;
    SubstitutionState state;
    XawTextPosition from, to, start, end, first, last;

    /* Use if need to allocate a buffer to pass the entire line to reexec */
    char *line;
    long lsize;

    /* Buffer to prepare replacement, if needs to expand backreferences */
    char *buffer;
    long bsize;
} EditInfo;

/*
 * Prototypes
 */
static void ActionHook(Widget, XtPointer, String, XEvent*, String*, Cardinal*);
static void AutoReplaceHook(Widget, String, XEvent*);
static Bool StartAutoReplace(void);
static char *ReplacedWord(char*, char*);
static void AutoReplace(Widget, XEvent*);
static void AutoReplaceCallback(Widget, XtPointer, XtPointer);

static void SubstituteHook(Widget w, String action, XEvent *event);
static void SubstituteCallback(Widget, XtPointer, XtPointer);

/*
 * Initialization
 */
#define STRTBLSZ	11
static ReplaceList *replace_list[STRTBLSZ];
static EditInfo einfo;
extern Widget scratch;

/*
 * Implementation
 */
Bool
StartHooks(XtAppContext app)
{
    static Bool first_time = True;

    if (first_time) {
	StartAutoReplace();
	(void)XtAppAddActionHook(app, ActionHook, NULL);
	first_time = False;

	return (True);
    }
    return (False);
}

/*ARGSUSED*/
static void
ActionHook(Widget w, XtPointer client_data, String action, XEvent *event,
	   String *params, Cardinal *num_params)
{
    AutoReplaceHook(w, action, event);
    SubstituteHook(w, action, event);
}

/*** auto replace ***/
struct {
    Widget widget;
    String text;
    Cardinal length;
    XawTextPosition left, right;
    Bool replace;
    Bool enabled;
} auto_replace;

static void
AutoReplaceHook(Widget w, String action, XEvent *event)
{
    static Bool multiply;

    if (w != textwindow || !auto_replace.enabled)
	return;

    if (auto_replace.widget != textwindow) {
	if (auto_replace.replace) {
	    auto_replace.replace = False;
	    XtRemoveCallback(auto_replace.widget, XtNpositionCallback,
			     AutoReplaceCallback, NULL);
	}
    }
    else if (strcmp(action, "multiply") == 0) {
	multiply = True;
	return;
    }
    else if (strcmp(action, "numeric") == 0) {
	if (multiply)
	    return;
    }
    else if (strcmp(action, "insert-char") && strcmp(action, "newline") &&
	strcmp(action, "newline-and-indent")) {
	return;
    }
    multiply = False;

    AutoReplace(w, event);
}

static Bool
StartAutoReplace(void)
{
    Bool esc;
    int len, llen, rlen, count = 0;
    char ch, *tmp, *left, *right, *replace = app_resources.auto_replace;

    if (!replace || !*replace)
	return (False);

    left = XtMalloc(llen = 256);
    right = XtMalloc(rlen = 256);
    while (*replace) {
	/* skip white spaces */
	while (*replace && isspace(*replace))
	    ++replace;
	if (!*replace)
	    break;

	/* read left */
	tmp = replace;
	while (*replace && !isspace(*replace))
	    ++replace;
	len = replace - tmp;
	if (len >= llen)
	    left = XtRealloc(left, llen = len + 1);
	strncpy(left, tmp, len);
	left[len] = '\0';

	/* skip white spaces */
	while (*replace && isspace(*replace))
	    ++replace;

	/* read right */
	len = 0;
	esc = False;
	while ((ch = *replace) != '\0') {
	    ++replace;
	    if (len + 2 >= rlen)
		right = XtRealloc(right, rlen += 256);
	    if (ch == '\\') {
		if (esc)
		    right[len++] = '\\';
		esc = !esc;
		continue;
	    }
	    else if (ch == '\n' && !esc)
		break;
	    else
		right[len++] = ch;
	    esc = False;
	}
	right[len] = '\0';

	(void)ReplacedWord(left, right);
	++count;
    }
    XtFree(left);
    XtFree(right);

    return (auto_replace.enabled = count > 0);
}

static char *
ReplacedWord(char *word, char *replace)
{
    ReplaceList *list;
    int ii = 0;
    char *pp = word;

    while (*pp)
	ii = (ii << 1) ^ *pp++;
    if (ii < 0)
	ii = -ii;
    ii %= STRTBLSZ;
    for (list = replace_list[ii]; list; list = list->next)
	if (strcmp(list->word, word) == 0) {
	    if (replace) {
		XtFree(list->replace);
		list->replace = XtNewString(replace);
	    }
	    return (list->replace);
	}

    if (!replace)
	return (NULL);

    list = XtNew(ReplaceList);
    list->word = XtNewString(word);
    list->replace = XtNewString(replace);
    list->next = replace_list[ii];
    replace_list[ii] = list;

    return (list->replace);
}

static void
AutoReplace(Widget w, XEvent *event)
{
    static XComposeStatus compose = {NULL, 0};
    KeySym keysym;
    XawTextBlock block;
    XawTextPosition left, right, pos;
    Widget source;
    int i, len, size;
    char *str, buf[32], mb[sizeof(wchar_t)];

    size = XLookupString((XKeyEvent*)event, mb, sizeof(mb), &keysym, &compose);

    if (size != 1 || isalnum(*mb))
	return;

    source = XawTextGetSource(w);
    right = XawTextGetInsertionPoint(w);
    left = XawTextSourceScan(source, right, XawstWhiteSpace,
			     XawsdLeft, 1, False);

    if (left < 0 || left == right)
	return;

    len = 0;
    str = buf;
    size = sizeof(buf);
    pos = left;
    while (pos < right) {
	pos = XawTextSourceRead(source, pos, &block, right - pos);
	for (i = 0; i < block.length; i++) {
	    if (block.format == FMT8BIT)
		*mb = block.ptr[i];
	    else
		wctomb(mb, ((wchar_t*)block.ptr)[i]);
	    str[len++] = *mb;
	    if (len + 2 >= size) {
		if (str == buf)
		    str = XtMalloc(size += sizeof(buf));
		else
		    str = XtRealloc(str, size += sizeof(buf));
	    }
	}
    }
    str[len] = '\0';
    if ((auto_replace.text = ReplacedWord(str, NULL)) != NULL) {
	auto_replace.length = strlen(auto_replace.text);
	auto_replace.left = left;
	auto_replace.right = right;
	auto_replace.replace = True;
	XtAddCallback(auto_replace.widget = w, XtNpositionCallback,
		      AutoReplaceCallback, NULL);
    }
    if (str != buf)
	XtFree(str);
}

/*ARGSUSED*/
static void
AutoReplaceCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    int i, inc;
    XawTextBlock block, text;
    char buffer[1024], mb[sizeof(wchar_t)];
    XawTextPosition left, right, pos;

    if (!auto_replace.replace || w != auto_replace.widget)
	return;

    XtRemoveCallback(auto_replace.widget, XtNpositionCallback,
		     AutoReplaceCallback, NULL);
    auto_replace.replace = False;

    inc = XawTextGetInsertionPoint(w) - auto_replace.right;
    if (auto_replace.length + inc > sizeof(buffer))
	block.ptr = XtMalloc(auto_replace.length + inc);
    else
	block.ptr = buffer;
    memcpy(block.ptr, auto_replace.text, auto_replace.length);

    block.length = auto_replace.length;
    pos = left = auto_replace.right;
    right = left + inc;
    while (pos < right) {
	pos = XawTextSourceRead(XawTextGetSource(w), pos, &text, inc);
	for (i = 0; i < text.length; i++) {
	    if (text.format == FMT8BIT)
		*mb = text.ptr[i];
	    else
		wctomb(mb, ((wchar_t*)text.ptr)[i]);
	    block.ptr[block.length++] = *mb;
	}
    }

    block.firstPos = 0;
    block.format = FMT8BIT;

    if (XawTextReplace(w, auto_replace.left, auto_replace.right + inc,
		       &block) == XawEditDone)
	XawTextSetInsertionPoint(w, auto_replace.left + block.length);

    if (block.ptr != buffer)
	XtFree(block.ptr);
}

/*ARGUSED*/
void
LineEditAction(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    XawTextBlock block;

    if (international)
	/* XXX FIXME */
	return;

    block.firstPos = 0;
    block.format = FMT8BIT;
    block.ptr = einfo.command;
    block.length = strlen(einfo.command);

    XawTextReplace(filenamewindow, 0,
		   XawTextLastPosition(filenamewindow), &block);
    XtSetKeyboardFocus(topwindow, filenamewindow);
    line_edit = True;
}

#define LSCAN(from, count, include)	\
	XawTextSourceScan(source, from, XawstEOL, XawsdLeft, count, include)
#define RSCAN(from, count, include)	\
	XawTextSourceScan(source, from, XawstEOL, XawsdRight, count, include)
void
LineEdit(Widget w)
{
    /* Global usage variables */
    XawTextPosition from, to, first, last, position, length, redisplay;
    int replace, compile, ecode, nth, flags, count, etype;
    char *command, *line, buffer[128];
    XawTextBlock block;
    Widget source;
    XawTextScanDirection direction;
    xedit_flist_item *item;

    /* Variables used while parsing command */
    int state, action, offset, icase, confirm;
    long lfrom, lto, lfinc, ltinc, number;
    char *ptr, *pstart, *pend, *rstart, *rend, *tmp;

    /* Variables used in the search/replace loop */
    int len;
    XawTextPosition adjust = 0;

    command = GetString(filenamewindow);
    length = strlen(command);
    if (length >= sizeof(einfo.command)) {
	Feep();
	return;
    }

    item = FindTextSource(XawTextGetSource(w), NULL);
    source = item->source;
    position = XawTextGetInsertionPoint(w);
    first = XawTextSourceScan(source, 0, XawstAll, XawsdLeft, 1, True);
    last = XawTextSourceScan(source, 0, XawstAll, XawsdRight, 1, True);
    compile = redisplay = nth = count = confirm = 0;
    direction = XawsdRight;
    flags = RE_STARTEND;

	/* Error types */
#define T_NONE		0
#define T_OPTION	1
#define T_ICASE		2
#define T_COMMAND	3
#define T_REPLACE	4
#define T_SEARCH	5
#define T_BACKSLASH	6
#define T_DIRECTION	7
#define T_COMMA		8
#define T_OFFSET	9
#define T_INCREMENT	10
#define T_NUMBER	11
#define T_UNFINISHED	12
#define T_RANGE		13
#define T_BACKREF	14
#define T_EDIT		15
    etype = T_NONE;

#define FAIL(code)	{ etype = code; goto fail; }

	/* Value for the line value, anything else is the line number */
#define L_FIRST		-1
#define L_CURRENT	-2
#define L_LAST		-3
    lfrom = L_FIRST;
    lto = L_LAST;

	/* Parsing states */
#define E_FINC		0
#define E_FROM		1
#define E_COMMA		2
#define E_TINC		3
#define E_TO		4
#define E_COMMAND	5
#define E_REGEX		6
#define E_SUBST		7
#define E_OPTIONS	8
    state = E_FROM;	    /* Beginning interpretation */

	/* Known commands */
#define	A_SEARCH	0
#define	A_REPLACE	1
    action = A_SEARCH;

	/* Flag to replace all occurrences */
#define	O_ALL		-1

    number = 1;
    lfinc = ltinc = 0;
    icase = offset = 0;
    pstart = pend = rstart = rend = NULL;

    if (einfo.state != SubstituteDisabled) {
	if (einfo.widget != w || strcmp(einfo.command, command)) {
	    einfo.widget = w;
	    einfo.state = SubstituteAsk;
	}
	else {
	    XawTextPosition s_start, s_end;

	    XawTextGetSelectionPos(w, &s_start, &s_end);
	    if (s_start != einfo.start || s_end != einfo.end)
		einfo.state = SubstituteAsk;
	    confirm = replace = 1;
	    from = einfo.from;
	    to = einfo.to;
	    first = einfo.first;
	    last = einfo.last;
	    goto confirm_label;
	}
    }

    /* Remember last command */
    strcpy(einfo.command, command); 

    /* Loop parsing command */
    for (ptr = einfo.command; *ptr;) {
	switch (*ptr++) {
	    case 'c':
		if (state != E_OPTIONS &&
		    state != E_COMMAND &&
		    state != E_REGEX)
		    FAIL(T_OPTION)
		confirm = 1;
		break;
	    case 'g':
		if (state != E_OPTIONS &&
		    state != E_COMMAND &&
		    state != E_REGEX)
		    FAIL(T_OPTION)
		offset = O_ALL;
		break;
	    case 'i':
		if (state != E_OPTIONS &&
		    state != E_COMMAND &&
		    state != E_REGEX &&
		    state != E_FROM)
		    FAIL(T_ICASE)
		icase = 1;
		break;
	    case 's':
		if (state == E_FROM)
		    lfrom = lto = L_CURRENT;
		else if (state == E_COMMA) {
		    lto = L_CURRENT;
		    ltinc = lfinc;
		}
		else if (state == E_TO)
		    lto = L_LAST;
		else if (state == E_FINC) {
		    ltinc = lfinc;
		    lto = L_CURRENT;
		}
		else if (state != E_COMMAND && state != E_TINC)
		    FAIL(T_COMMAND)
		action = A_REPLACE;
		state = E_REGEX;
		break;
	    case '?':
		if (action == A_REPLACE)
		    FAIL(T_REPLACE)
	    case '/':
		if (state == E_TINC)
		    state = action == A_REPLACE ? E_REGEX : E_FROM;
		else if (state == E_COMMA || state == E_FINC) {
		    lto = L_LAST;
		    state = E_FROM;
		}
		else if (state == E_TO) {
		    if (ltinc == 0)
			lto = L_LAST;
		    state = E_FROM;
		}
		else if (state == E_COMMAND)
		    state = E_FROM;
		else if (state != E_REGEX &&
			 state != E_SUBST &&
			 state != E_FROM)
		    FAIL(T_SEARCH)
		if (state != E_SUBST)
		    direction = ptr[-1] == '/' ? XawsdRight : XawsdLeft;
		for (tmp = ptr; *tmp; tmp++) {
		    if (*tmp == '\\') {
			if (*++tmp == '\0')
			    FAIL(T_BACKSLASH)
		    }
		    else if (*tmp == ptr[-1])
			break;
		}
		if (state == E_REGEX) {
		    if (*tmp != ptr[-1])
			FAIL(T_DIRECTION)
		    pstart = ptr;
		    pend = ptr = tmp;
		    state = E_SUBST;
		}
		else if (state == E_FROM) {
		    pstart = ptr;
		    pend = ptr = tmp;
		    state = E_OPTIONS;
		    if (*ptr)
			++ptr;
		}
		else { /* E_SUBST */
		    rstart = ptr;
		    rend = tmp;
		    state = E_OPTIONS;
		    ptr = tmp;
		    if (*ptr)
			++ptr;
		}
		break;
	    case ',':
		if (state == E_FROM)
		    lfrom = L_FIRST;
		else if (state == E_FINC)
		    lfrom = L_CURRENT;
		else if (state != E_COMMA)
		    FAIL(T_COMMA)
		state = E_TO;
		break;
	    case '%':
		if (state == E_FROM) {
		    lfrom = L_FIRST;
		    lto = L_LAST;
		    state = E_COMMAND;
		}
		else
		    FAIL(T_OFFSET)
		break;
	    case '$':
		if (state != E_TO)
		    FAIL(T_OFFSET)
		lto = L_LAST;
		state = E_COMMAND;
		break;
	    case '.':
		if (state == E_FROM) {
		    lfrom = L_CURRENT;
		    state = E_COMMA;
		}
		else if (state == E_TO) {
		    lto = L_CURRENT;
		    state = E_COMMAND;
		}
		else
		    FAIL(T_OFFSET)
		break;
	    case '+':
		if (state == E_FROM) {
		    lfinc = 1;
		    lfrom = L_CURRENT;
		    state = E_FINC;
		}
		else if (state == E_TO) {
		    ltinc = 1;
		    lto = L_CURRENT;
		    state = E_TINC;
		}
		else
		    FAIL(T_INCREMENT)
		break;
	    case '-':	    case '^':
		if (state == E_FROM) {
		    lfinc = -1;
		    lfrom = L_CURRENT;
		    state = E_FINC;
		}
		else if (state == E_TO) {
		    ltinc = -1;
		    lto = L_CURRENT;
		    state = E_TINC;
		}
		else
		    FAIL(T_INCREMENT)
		number = -1;
		break;
	    case ';':
		if (state != E_FROM)
		    FAIL(T_OFFSET)
		lfrom = L_CURRENT;
		lto = L_LAST;
		state = E_COMMAND;
		break;
	    case '1':	    case '2':	    case '3':
	    case '4':	    case '5':	    case '6':
	    case '7':	    case '8':	    case '9':
		number = number * (ptr[-1] - '0');
		while (isdigit(*ptr))
		    number = number * 10 + (*ptr++ - '0');
		if (state == E_FROM) {
		    lfrom = number;
		    state = E_COMMA;
		}
		else if (state == E_FINC) {
		    lfinc = number;
		    state = E_COMMA;
		}
		else if (state == E_TO) {
		    lto = number;
		    state = E_COMMAND;
		}
		else if (state == E_TINC) {
		    ltinc = number;
		    state = E_COMMAND;
		}
		else if (state == E_OPTIONS && action == A_REPLACE)
		    offset = number - 1;
		else
		    FAIL(T_NUMBER)
		number = 1;
		break;
	    case '\0':
		if (state == E_OPTIONS)
		    break;
	    default:
		FAIL(T_UNFINISHED)
	}
    }

    replace = action == A_REPLACE;

    switch (lfrom) {
	case L_FIRST:
	    from = first;
	    break;
	case L_LAST:
	    from = LSCAN(last, 1, False);
	    break;
	case L_CURRENT:
	    if (lfinc <= 0)
		from = LSCAN(position, -lfinc + 1, False);
	    else {
		from = RSCAN(position, lfinc + 1, False);
		from = LSCAN(from, 1, False);
	    }
	    break;
	default:
	    from = RSCAN(first, lfrom, False);
	    from = LSCAN(from, 1, False);
	    break;
    }
    /* Just requesting to go to the numbered line */
    if (state == E_COMMA || state == E_FINC) {
	XawTextSetInsertionPoint(w, from);
	return;
    }

    length = pend - pstart;
    if (pstart == NULL || (replace && rstart == NULL) ||
	length >= sizeof(einfo.pattern) - 1)
	FAIL(T_UNFINISHED)

    /* Need to (re)compile regular expression pattern? */
    if ((!!(einfo.flags & RE_ICASE) ^ icase) ||
	strlen(einfo.pattern) < length ||
	strncmp(pstart, einfo.pattern, length)) {
	compile = 1;
	memcpy(einfo.pattern, pstart, length);
	einfo.pattern[length] = '\0';
	einfo.flags = icase ? RE_ICASE : 0;
    }

    /* Check range of lines to operate on */
    switch (lto) {
	case L_FIRST:
	    to = RSCAN(first, 1, True);
	    break;
	case L_LAST:
	    to = last;
	    break;
	case L_CURRENT:
	    if (ltinc < 0) {
		to = LSCAN(position, -ltinc + 1, True);
		to = RSCAN(to, 2, True);
	    }
	    else
		to = RSCAN(position, ltinc + 1, True);
	    break;
	default:
	    to = RSCAN(first, lto, True);
	    break;
    }
    if (from >= to)
	FAIL(T_RANGE)

    /* Set first and last position allowed to search/replace */
    first = from;
    last = to;

    /* Check bounds to work on */
    if (replace) {
	int i, csubst;

	/* Check number of required match results and remove/parse backslashes */
	memcpy(einfo.subst, rstart, einfo.slen = rend - rstart);
	einfo.sref = 0;
	einfo.soff = offset;
	for (i = 0; i < einfo.slen - 1; i++) {
	    if (einfo.subst[i] == '\\') {
		csubst = -1;
		switch (einfo.subst[i + 1]) {
 		    case '0':	    csubst = '\0';  break;
		    case 'a':	    csubst = '\b';  break;
		    case 'b':	    csubst = '\b';  break;
		    case 'f':	    csubst = '\f';  break;
		    case 'n':	    csubst = '\n';  break;
		    case 'r':	    csubst = '\r';  break;
		    case 't':	    csubst = '\t';  break;
		    case 'v':	    csubst = '\v';  break;
		    case '1':	    case '2':	    case '3':
		    case '4':	    case '5':	    case '6':
		    case '7':	    case '8':	    case '9':
			++i;
			if (einfo.subst[i] - '0' > einfo.sref)
			    einfo.sref = einfo.subst[i] - '0';
			break;
		    default:
			csubst = einfo.subst[i + 1];
			break;
		}
		if (csubst >= 0) {
		    memmove(einfo.subst + i, einfo.subst + i + 1,
			    einfo.slen - i);
		    einfo.subst[i] = csubst;
		    --einfo.slen;
		    ++i;
		    csubst = -1;
		}
	    }
	}
    }
    else if (einfo.widget != w) {
	/* Just a flag for backward search */
	einfo.from = last;
	einfo.widget = w;
    }

    /* Compile pattern if required */
    if (compile) {
	refree(&einfo.regex);
	if ((ecode = recomp(&einfo.regex, einfo.pattern, einfo.flags)) != 0)
	    goto print;
    }

    if (!replace && position >= first && position <= last) {
	from = position;
	/* The backwards repetition currently is only backwards when
	 * changing lines, so remember from where started, to also
	 * search in the first line. */
	if (LSCAN(from, 1, False) == from) {
	    if (direction == XawsdLeft)
		einfo.from = from;
	}
	else
	    flags |= RE_NOTBOL;
    }
    to = RSCAN(from, 1, True);

    if (confirm) {
	if (!replace)
	    FAIL(T_UNFINISHED)
	einfo.widget = w;
	einfo.state = SubstituteAsk;
	einfo.from = from;
	einfo.to = to;
	einfo.first = first;
	einfo.last = last;
    }
    else
	einfo.state = SubstituteDisabled;

confirm_label:
    if (replace) {
	redisplay = 1;
	XawTextDisableRedisplay(w);
    }

    for (;;) {
	if (confirm && einfo.state != SubstituteAsk) {
	    /* Restore state from previous call */
	    ecode = 0;
	    nth = einfo.soff;
	    /* einfo.mats should not have changed */
	    if (einfo.state == SubstituteYes) {
		einfo.state = SubstituteAsk;
		line = einfo.text_line;
		goto substitute_label;
	    }
	    else {
		++nth;
		einfo.state = SubstituteAsk;
		from = einfo.from = einfo.end;
		goto no_substitute_label;
	    }
	}

	/* Read or use a line of text inplace */
	position = from;
	length = to - from;
	XawTextSourceRead(source, position, &block, to - position);
	if (block.length >= length)
	    line = block.ptr;
	else {
	    if (length > einfo.lsize) {
		einfo.line = XtRealloc(einfo.line, to - from);
		einfo.lsize = to - from;
	    }
	    memcpy(einfo.line, block.ptr, block.length);
	    length = block.length;
	    for (position += length; position < to; position += length) {
		XawTextSourceRead(source, position, &block, to - position);
		memcpy(einfo.line + length, block.ptr, block.length);
		length += block.length;
	    }
	    line = einfo.line;
	}

	/* Execute expression */
	einfo.mats[0].rm_so = 0;
	einfo.mats[0].rm_eo = to - from - !(from == to || to == last);
	ecode = reexec(&einfo.regex, line,
		       einfo.sref + 1, &einfo.mats[0], flags);

	if (replace && einfo.mats[0].rm_so == einfo.mats[0].rm_eo)
	    /* Ignore empty matches */
	    ecode = RE_NOMATCH;

	if (ecode == 0 && confirm &&
	    (einfo.soff == O_ALL || nth == einfo.soff)) {
	    einfo.end = from + einfo.mats[0].rm_eo;
	    einfo.start = from + einfo.mats[0].rm_so;
	    XawTextSetInsertionPoint(w, einfo.end);
	    XawTextSetSelection(w, einfo.start, einfo.end);

	    einfo.state = SubstituteAsk;
	    einfo.from = from;
	    einfo.to = to;
	    einfo.first = first;
	    einfo.last = last;
	    einfo.text_line = line;
	    break;
	}

substitute_label:
	if (ecode == 0) {
	    from += einfo.mats[0].rm_so;
	    len = einfo.mats[0].rm_eo - einfo.mats[0].rm_so;

	    /* Found match */
	    if (replace) {
		/* If not replacing all ocurrences, or if not
		 * at the correct offset */
		if (einfo.soff != O_ALL && nth < einfo.soff) {
		    from += len;
		    ++nth;
		    continue;
		}

		/* Do the substitution */
		block.firstPos = 0;
		block.format = FMT8BIT;
		if (einfo.sref) {
		    /* Hard way */
		    int i, ref, xlen;

		    for (i = length = 0; i < einfo.slen; i++) {
			if (length + 2 >= einfo.bsize) {
			    einfo.bsize = einfo.bsize + 1024;
			    einfo.buffer = XtRealloc(einfo.buffer, einfo.bsize);
			}
			if (einfo.subst[i] == '\\') {
			    ++i;
			    if (einfo.subst[i] >= '1' && einfo.subst[i] <= '9') {
				ref = einfo.subst[i] - '0';
				xlen = einfo.mats[ref].rm_eo -
				       einfo.mats[ref].rm_so;
				if (xlen < 0)
				    /* Oops, something went wrong... */
				    FAIL(T_BACKREF)
				if (length + xlen >= einfo.bsize) {
				    einfo.bsize += xlen + 1024 - (xlen % 1024);
				    einfo.buffer = XtRealloc(einfo.buffer,
							     einfo.bsize);
				}
				memcpy(einfo.buffer + length,
				      line + einfo.mats[ref].rm_so, xlen);
				length += xlen;
			    }
			    else {
				einfo.buffer[length++] = einfo.subst[i - 1];
				einfo.buffer[length++] = einfo.subst[i];
			    }
			}
			else
			    einfo.buffer[length++] = einfo.subst[i];
		    }
		    block.ptr = einfo.buffer;
		    block.length = length;
		}
		else {
		    block.ptr = einfo.subst;
		    block.length = length = einfo.slen;
		}
		adjust = length - len;
		if (XawTextReplace(w, from, from + len, &block) != XawEditDone)
		    FAIL(T_EDIT)
		last += adjust;
		to += adjust;
		from += length;

no_substitute_label:
		if (einfo.soff != O_ALL) {
		    nth = 0;
		    to = RSCAN(from, 1, True);
		    from = LSCAN(to, 1, False);
		    if (to == last) {
			XawTextSetInsertionPoint(w, from);
			break;
		    }
		}
		else
		    flags |= RE_NOTBOL;
	    }
	    else {
		XawTextSetInsertionPoint(w, from + len);
		XawTextSetSelection(w, from, from + len);
		break;
	    }
	}
	else if (ecode == RE_NOMATCH) {
	    nth = 0;

	    /* Try again in the next/previous line */
	    if (direction == XawsdLeft) {
		from = LSCAN(to - 1, 1 + (from != to), False);
		if (einfo.from <= first) {
		    Feep();
		    if (++count > 1) {
			XawTextSetInsertionPoint(w, position);
			XawTextUnsetSelection(w);
			break;
		    }
		    from = LSCAN(last, 1, False);
		}
		to = RSCAN(from, 1, True);
		/* Can use einfo.from because replace is only done forward */
		einfo.from = from;
	    }
	    else {
		if (to >= last) {
		    Feep();
		    if (replace || ++count > 1) {
			XawTextSetInsertionPoint(w, position);
			XawTextUnsetSelection(w);
			einfo.state = SubstituteDisabled;
			confirm = 0;
			break;
		    }
		    to = first;
		}
		from = LSCAN(to + 1, 1, False);
		to = RSCAN(from, 1, True);
	    }

	    /* Reset flags now */
	    flags = RE_STARTEND;
	}
	else
	    goto print;
    }

    if (redisplay)
	XawTextEnableRedisplay(w);
    /* If replacing not interatively return to the edit window after finished */
    if (replace && !confirm) {
	Arg args[1];

	XtSetKeyboardFocus(topwindow, textwindow);
	if (item->source != scratch)
	    XtSetArg(args[0], XtNstring, item->name);
	else
	    XtSetArg(args[0], XtNstring, NULL);
	XtSetValues(filenamewindow, args, 1);
    }
    return;

print:
    if (redisplay)
	XawTextEnableRedisplay(w);

    strcpy(buffer, "Regex error: ");
    length = 13;
    reerror(ecode, &einfo.regex,
	     buffer + length, sizeof(buffer) - length - 2);
    strcat(buffer, "\n");
    XeditPrintf(buffer);
    refree(&einfo.regex);
    einfo.state = SubstituteDisabled;
    Feep();
    return;


fail:
    if (etype != T_NONE) {
	switch (etype) {
	    case T_OPTION:
		ptr = "Option needs a command";
		break;
	    case T_ICASE:
		ptr = "Icase needs an command defined or none for search";
		break;
	    case T_COMMAND:
		ptr = "Command incorrectly specified";
		break;
	    case T_REPLACE:
		ptr = "Can only search backwards";
		break;
	    case T_SEARCH:
		ptr = "Badly placed search/replace specifier";
		break;
	    case T_BACKSLASH:
		ptr = "A single backslash cannot be the last command character";
		break;
	    case T_DIRECTION:
		ptr = "Regular expression must be separeted by / or ? not both";
		break;
	    case T_COMMA:
		ptr = "Badly placed comma";
		break;
	    case T_OFFSET:
		ptr = "Badly placed line offset specifier";
		break;
	    case T_INCREMENT:
		ptr = "Badly placed line offset increment specifier";
		break;
	    case T_NUMBER:
		ptr = "Numeric argument not expected";
		break;
	    case T_UNFINISHED:
		ptr = "Unfinished command";
		break;
	    case T_RANGE:
		ptr = "Bad line range";
		break;
	    case T_BACKREF:
		/* This may be an internal re error, but most likely the
		 * user asked for something like "s/re0(re1)re2/\2/" */
		ptr = "Bad backreference";
		break;
	    case T_EDIT:
		ptr = "Failed to replace text";
		break;
	    default:
		ptr = "Unknown error";
		break;
	}
	XmuSnprintf(buffer, sizeof(buffer), "Error: %s.\n", ptr);
	XeditPrintf(buffer);
    }
    if (redisplay)
	XawTextEnableRedisplay(w);
    einfo.state = SubstituteDisabled;
    Feep();
}

static void
SubstituteHook(Widget w, String action, XEvent *event)
{
    if (w != filenamewindow)
	return;

    if (line_edit && einfo.state == SubstituteAsk) {
	if (strcmp(action, "newline") == 0 ||
	    strcmp(action, "load-file") == 0)
	    einfo.state = SubstituteAsk;
	else if (strcmp(action, "insert-char") == 0) {
	    static XComposeStatus compose = {NULL, 0};
	    KeySym keysym;
	    char mb[sizeof(wchar_t)];

	    if (XLookupString((XKeyEvent*)event, mb, sizeof(mb),
			      &keysym, &compose) == 1) {
		if (*mb == 'y' || *mb == 'Y')
		    einfo.state = SubstituteYes;
		else if (*mb == 'n' || *mb == 'N')
		    einfo.state = SubstituteNo;
		else
		    einfo.state = SubstituteDisabled;

		if (einfo.state != SubstituteDisabled) {
		    einfo.callback = 1;
		    XtAddCallback(filenamewindow, XtNpositionCallback,
				  SubstituteCallback, NULL);
		}
	    }
	}
	else if (strcmp(action, "cancel-find-file") == 0)
	    einfo.state = SubstituteDisabled;
    }
    if (einfo.state == SubstituteDisabled && einfo.callback) {
	einfo.callback = 0;
	XtRemoveCallback(filenamewindow, XtNpositionCallback,
			 SubstituteCallback, NULL);
    }
}

/*ARGSUSED*/
static void
SubstituteCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
    XawTextBlock block;

    einfo.callback = 0;
    XtRemoveCallback(filenamewindow, XtNpositionCallback,
		     SubstituteCallback, NULL);

    block.firstPos = 0;
    block.format = FMT8BIT;
    block.ptr = einfo.command;
    block.length = strlen(einfo.command);

    XawTextReplace(filenamewindow, 0,
		   XawTextLastPosition(filenamewindow), &block);

    LineEdit(einfo.widget);
}
