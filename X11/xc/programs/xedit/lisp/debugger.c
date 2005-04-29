/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
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

/* $XFree86: xc/programs/xedit/lisp/debugger.c,v 1.25 2003/04/27 18:17:32 tsi Exp $ */

#include <ctype.h>
#include "lisp/io.h"
#include "lisp/debugger.h"
#include "lisp/write.h"

#ifdef DEBUGGER
#define DebuggerHelp		0
#define DebuggerAbort		1
#define DebuggerBacktrace	2
#define DebuggerContinue	3
#define DebuggerFinish		4
#define DebuggerFrame		5
#define DebuggerNext		6
#define DebuggerPrint		7
#define DebuggerStep		8
#define DebuggerBreak		9
#define DebuggerDelete		10
#define DebuggerDown		11
#define DebuggerUp		12
#define DebuggerInfo		13
#define DebuggerWatch		14

#define DebuggerInfoBreakpoints	0
#define DebuggerInfoBacktrace	1

/*
 * Prototypes
 */
static char *format_integer(int);
static void LispDebuggerCommand(LispObj *obj);

/*
 * Initialization
 */
static struct {
    char *name;
    int action;
} commands[] = {
    {"help",		DebuggerHelp},
    {"abort",		DebuggerAbort},
    {"backtrace",	DebuggerBacktrace},
    {"b",		DebuggerBreak},
    {"break",		DebuggerBreak},
    {"bt",		DebuggerBacktrace},
    {"continue",	DebuggerContinue},
    {"d",		DebuggerDelete},
    {"delete",		DebuggerDelete},
    {"down",		DebuggerDown},
    {"finish",		DebuggerFinish},
    {"frame",		DebuggerFrame},
    {"info",		DebuggerInfo},
    {"n",		DebuggerNext},
    {"next",		DebuggerNext},
    {"print",		DebuggerPrint},
    {"run",		DebuggerContinue},
    {"s",		DebuggerStep},
    {"step",		DebuggerStep},
    {"up",		DebuggerUp},
    {"watch",		DebuggerWatch},
};

static struct {
    char *name;
    int subaction;
} info_commands[] = {
    {"breakpoints",	DebuggerInfoBreakpoints},
    {"stack",		DebuggerInfoBacktrace},
    {"watchpoints",	DebuggerInfoBreakpoints},
};

static char debugger_help[] =
"Available commands are:\n\
\n\
help		- This message.\n\
abort		- Abort the current execution, and return to toplevel.\n\
backtrace, bt	- Print backtrace.\n\
b, break	- Set breakpoint at function name argument.\n\
continue	- Continue execution.\n\
d, delete	- Delete breakpoint(s), all breakpoint if no arguments given.\n\
down		- Set environment to frame called by the current one.\n\
finish		- Executes until current form is finished.\n\
frame		- Set environment to selected frame.\n\
info		- Prints information about the debugger state.\n\
n, next		- Evaluate next form.\n\
print		- Print value of variable name argument.\n\
run		- Continue execution.\n\
s, step		- Evaluate next form, stopping on any subforms.\n\
up		- Set environment to frame that called the current one.\n\
\n\
Commands may be abbreviated.\n";

static char debugger_info_help[] =
"Available subcommands are:\n\
\n\
breakpoints	- List and prints status of breakpoints, and watchpoints.\n\
stack		- Backtrace of stack.\n\
watchpoints	- List and prints status of watchpoints, and breakpoints.\n\
\n\
Subcommands may be abbreviated.\n";

/* Debugger variables layout (if you change it, update description):
 *
 * DBG
 *	is a macro for lisp__data.dbglist
 *	is a NIL terminated list
 *	every element is a list in the format (NOT NIL terminated):
 *	(list* NAM ARG ENV HED LEX)
 *	where
 *		NAM is an ATOM for the function/macro name
 *		    or NIL for lambda expressions
 *		ARG is NAM arguments (a LIST)
 *		ENV is the value of lisp__data.stack.base (a FIXNUM)
 *		LEN is the value of lisp__data.env.length (a FIXNUM)
 *		LEX is the value of lisp__data.env.lex (a FIXNUM)
 *	new elements are added to the beggining of the DBG list
 *
 * BRK
 *	is macro for lisp__data.brklist
 *	is a NIL terminated list
 *	every element is a list in the format (NIL terminated):
 *	(list NAM IDX TYP HIT VAR VAL FRM)
 *	where
 *		NAM is an ATOM for the name of the object at
 *		    wich the breakpoint was added
 *		IDX is a FIXNUM, the breakpoint number
 *		    must be stored, as breakpoints may be deleted
 *		TYP is a FIXNUM that must be an integer of enum LispBreakType
 *		HIT is a FIXNUM, with the number of times this breakpoint was
 *		    hitted.
 *		VAR variable to watch a SYMBOL	(not needed for breakpoints)
 *		VAL value of watched variable	(not needed for breakpoints)
 *		FRM frame where variable started being watched
 *						(not needed for breakpoints)
 *	new elements are added to the end of the list
 */

/*
 * Implementation
 */
void
LispDebugger(LispDebugCall call, LispObj *name, LispObj *arg)
{
    int force = 0;
    LispObj *obj, *prev;

    switch (call) {
	case LispDebugCallBegin:
	    ++lisp__data.debug_level;
	    GCDisable();
	    DBG = CONS(CONS(name, CONS(arg, CONS(FIXNUM(lisp__data.stack.base),
		       CONS(FIXNUM(lisp__data.env.length),
			    FIXNUM(lisp__data.env.lex))))), DBG);
	    GCEnable();
	    for (obj = BRK; obj != NIL; obj = CDR(obj))
		if (ATOMID(CAR(CAR(obj))) == ATOMID(name) &&
		    FIXNUM_VALUE(CAR(CDR(CDR(CAR(obj))))) ==
		    LispDebugBreakFunction)
		    break;
	    if (obj != NIL) {
		long counter;

		/* if not at a fresh line */
		if (LispGetColumn(NIL))
		    LispFputc(Stdout, '\n');
		LispFputs(Stdout, "BREAK #");
		LispWriteObject(NIL, CAR(CDR(CAR(obj))));
		LispFputs(Stdout, "> (");
		LispWriteObject(NIL, CAR(CAR(DBG)));
		LispFputc(Stdout, ' ');
		LispWriteObject(NIL, CAR(CDR(CAR(DBG))));
		LispFputs(Stdout, ")\n");
		force = 1;
		/* update hits counter */
		counter = FIXNUM_VALUE(CAR(CDR(CDR(CDR(CAR(obj))))));
		CAR(CDR(CDR(CDR(CAR(obj))))) FIXNUM(counter + 1);
	    }
	    break;
	case LispDebugCallEnd:
	    DBG = CDR(DBG);
	    if (lisp__data.debug_level < lisp__data.debug_step)
		lisp__data.debug_step = lisp__data.debug_level;
	    --lisp__data.debug_level;
	    break;
	case LispDebugCallFatal:
	    LispDebuggerCommand(NIL);
	    return;
	case LispDebugCallWatch:
	    break;
    }

    /* didn't return, check watchpoints */
    if (call == LispDebugCallEnd || call == LispDebugCallWatch) {
watch_again:
	for (prev = obj = BRK; obj != NIL; prev = obj, obj = CDR(obj)) {
	    if (FIXNUM_VALUE(CAR(CDR(CDR(CAR(obj))))) ==
		LispDebugBreakVariable) {
		/* the variable */
		LispObj *wat = CAR(CDR(CDR(CDR(CDR(CAR(obj))))));
		void *sym = LispGetVarAddr(CAAR(obj));
		LispObj *frm = CAR(CDR(CDR(CDR(CDR(CDR(CDR(CAR(obj))))))));

		if ((sym == NULL && lisp__data.debug_level <= 0) ||
		    (sym != wat->data.opaque.data &&
		     FIXNUM_VALUE(frm) > lisp__data.debug_level)) {
		    LispFputs(Stdout, "WATCH #");
		    LispFputs(Stdout, format_integer(FIXNUM_VALUE(CAR(CDR(CAR(obj))))));
		    LispFputs(Stdout, "> ");
		    LispFputs(Stdout, STRPTR(CAR(CAR(obj))));
		    LispFputs(Stdout, " deleted. Variable does not exist anymore.\n");
		    /* force debugger to stop */
		    force = 1;
		    if (obj == prev) {
			BRK = CDR(BRK);
			goto watch_again;
		    }
		    else
			RPLACD(prev, CDR(obj));
		    obj = prev;
		}
		else {
		    /* current value */
		    LispObj *cur = *(LispObj**)wat->data.opaque.data;
		    /* last value */
		    LispObj *val = CAR(CDR(CDR(CDR(CDR(CDR(CAR(obj)))))));
		    if (XEQUAL(val, cur) == NIL) {
			long counter;

			LispFputs(Stdout, "WATCH #");
			LispFputs(Stdout, format_integer(FIXNUM_VALUE(CAR(CDR(CAR(obj))))));
			LispFputs(Stdout, "> ");
			LispFputs(Stdout, STRPTR(CAR(CAR(obj))));
			LispFputc(Stdout, '\n');

			LispFputs(Stdout, "OLD: ");
			LispWriteObject(NIL, val);

			LispFputs(Stdout, "\nNEW: ");
			LispWriteObject(NIL, cur);
			LispFputc(Stdout, '\n');

			/* update current value */
			CAR(CDR(CDR(CDR(CDR(CDR(CAR(obj))))))) = cur;
			/* update hits counter */
			counter = FIXNUM_VALUE(CAR(CDR(CDR(CDR(CAR(obj))))));
			CAR(CDR(CDR(CDR(CAR(obj))))) = FIXNUM(counter + 1);
			/* force debugger to stop */
			force = 1;
		    }
		}
	    }
	}

	if (call == LispDebugCallWatch)
	    /* special call, just don't keep gc protected variables that may be
	     * using a lot of memory... */
	    return;
    }

    switch (lisp__data.debug) {
	case LispDebugUnspec:
	    LispDebuggerCommand(NIL);
	    goto debugger_done;
	case LispDebugRun:
	    if (force)
		LispDebuggerCommand(NIL);
	    goto debugger_done;
	case LispDebugFinish:
	    if (!force &&
		(call != LispDebugCallEnd ||
		 lisp__data.debug_level != lisp__data.debug_step))
		goto debugger_done;
	    break;
	case LispDebugNext:
	    if (call == LispDebugCallBegin) {
		if (!force && lisp__data.debug_level != lisp__data.debug_step)
		    goto debugger_done;
	    }
	    else if (call == LispDebugCallEnd) {
		if (!force && lisp__data.debug_level >= lisp__data.debug_step)
		    goto debugger_done;
	    }
	    break;
	case LispDebugStep:
	    break;
    }

    if (call == LispDebugCallBegin) {
	LispFputc(Stdout, '#');
	LispFputs(Stdout, format_integer(lisp__data.debug_level));
	LispFputs(Stdout, "> (");
	LispWriteObject(NIL, CAR(CAR(DBG)));
	LispFputc(Stdout, ' ');
	LispWriteObject(NIL, CAR(CDR(CAR(DBG))));
	LispFputs(Stdout, ")\n");
	LispDebuggerCommand(NIL);
    }
    else if (call == LispDebugCallEnd) {
	LispFputc(Stdout, '#');
	LispFputs(Stdout, format_integer(lisp__data.debug_level + 1));
	LispFputs(Stdout, "= ");
	LispWriteObject(NIL, arg);
	LispFputc(Stdout, '\n');
	LispDebuggerCommand(NIL);
    }
    else if (force)
	LispDebuggerCommand(arg);

debugger_done:
    return;
}

static void
LispDebuggerCommand(LispObj *args)
{
    LispObj *obj, *frm, *curframe;
    int i = 0, frame, matches, action = -1, subaction = 0;
    char *cmd, *arg, *ptr, line[256];

    int envbase = lisp__data.stack.base,
	envlen = lisp__data.env.length,
	envlex = lisp__data.env.lex;

    frame = lisp__data.debug_level;
    curframe = CAR(DBG);

    line[0] = '\0';
    arg = line;
    for (;;) {
	LispFputs(Stdout, DBGPROMPT);
	LispFflush(Stdout);
	if (LispFgets(Stdin, line, sizeof(line)) == NULL) {
	    LispFputc(Stdout, '\n');
	    return;
	}
	/* get command */
	ptr = line;
	while (*ptr && isspace(*ptr))
	    ++ptr;
	cmd = ptr;
	while (*ptr && !isspace(*ptr))
	    ++ptr;
	if (*ptr)
	    *ptr++ = '\0';

	if (*cmd) {	/* if *cmd is nul, then arg may be still set */
	    /* get argument(s) */
	    while (*ptr && isspace(*ptr))
		++ptr;
	    arg = ptr;
	    /* goto end of line */
	    if (*ptr) {
		while (*ptr)
		    ++ptr;
		--ptr;
		while (*ptr && isspace(*ptr))
		    --ptr;
		if (*ptr)
		    *++ptr = '\0';
	    }
	}

	if (*cmd == '\0') {
	    if (action < 0) {
		if (lisp__data.debug == LispDebugFinish)
		    action = DebuggerFinish;
		else if (lisp__data.debug == LispDebugNext)
		    action = DebuggerNext;
		else if (lisp__data.debug == LispDebugStep)
		    action = DebuggerStep;
		else if (lisp__data.debug == LispDebugRun)
		    action = DebuggerContinue;
		else
		    continue;
	    }
	}
	else {
	    for (i = matches = 0; i < sizeof(commands) / sizeof(commands[0]);
		 i++) {
		char *str = commands[i].name;

		ptr = cmd;
		while (*ptr && *ptr == *str) {
		    ++ptr;
		    ++str;
		}
		if (*ptr == '\0') {
		    action = commands[i].action;
		    if (*str == '\0') {
			matches = 1;
			break;
		    }
		    ++matches;
		}
	    }
	    if (matches == 0) {
		LispFputs(Stdout, "* Command unknown: ");
		LispFputs(Stdout, cmd);
		LispFputs(Stdout, ". Type help for help.\n");
		continue;
	    }
	    else if (matches > 1) {
		LispFputs(Stdout, "* Command is ambiguous: ");
		LispFputs(Stdout, cmd);
		LispFputs(Stdout, ". Type help for help.\n");
		continue;
	    }
	}

	switch (action) {
	    case DebuggerHelp:
		LispFputs(Stdout, debugger_help);
		break;
	    case DebuggerInfo:
		if (*arg == '\0') {
		    LispFputs(Stdout, debugger_info_help);
		    break;
		}

		for (i = matches = 0;
		     i < sizeof(info_commands) / sizeof(info_commands[0]);
		     i++) {
		    char *str = info_commands[i].name;

		    ptr = arg;
		    while (*ptr && *ptr == *str) {
			++ptr;
			++str;
		    }
		    if (*ptr == '\0') {
			subaction = info_commands[i].subaction;
			if (*str == '\0') {
			    matches = 1;
			    break;
			}
			++matches;
		    }
		}
		if (matches == 0) {
		    LispFputs(Stdout, "* Command unknown: ");
		    LispFputs(Stdout, arg);
		    LispFputs(Stdout, ". Type info for help.\n");
		    continue;
		}
		else if (matches > 1) {
		    LispFputs(Stdout, "* Command is ambiguous: ");
		    LispFputs(Stdout, arg);
		    LispFputs(Stdout, ". Type info for help.\n");
		    continue;
		}

		switch (subaction) {
		    case DebuggerInfoBreakpoints:
			LispFputs(Stdout, "Num\tHits\tType\t\tWhat\n");
			for (obj = BRK; obj != NIL; obj = CDR(obj)) {
			    /* breakpoint number */
			    LispFputc(Stdout, '#');
			    LispWriteObject(NIL, CAR(CDR(CAR(obj))));

			    /* number of hits */
			    LispFputc(Stdout, '\t');
			    LispWriteObject(NIL, CAR(CDR(CDR(CDR(CAR(obj))))));

			    /* breakpoint type */
			    LispFputc(Stdout, '\t');
			    switch ((int)FIXNUM_VALUE(CAR(CDR(CDR(CAR(obj)))))) {
				case LispDebugBreakFunction:
				    LispFputs(Stdout, "Function");
				    break;
				case LispDebugBreakVariable:
				    LispFputs(Stdout, "Variable");
				    break;
			    }

			    /* breakpoint object */
			    LispFputc(Stdout, '\t');
			    LispWriteObject(NIL, CAR(CAR(obj)));
			    LispFputc(Stdout, '\n');
			}
			break;
		    case DebuggerInfoBacktrace:
			goto debugger_print_backtrace;
		}
		break;
	    case DebuggerAbort:
		while (lisp__data.mem.level) {
		    --lisp__data.mem.level;
		    if (lisp__data.mem.mem[lisp__data.mem.level])
			free(lisp__data.mem.mem[lisp__data.mem.level]);
		}
		lisp__data.mem.index = 0;
		LispTopLevel();
		if (!lisp__data.running) {
		    LispMessage("*** Fatal: nowhere to longjmp.");
		    abort();
		}
		/* don't need to restore environment */
		siglongjmp(lisp__data.jmp, 1);
		/*NOTREACHED*/
		break;
	    case DebuggerBreak:
		for (ptr = arg; *ptr; ptr++) {
		    if (isspace(*ptr))
			break;
		    else
			*ptr = toupper(*ptr);
		}

		if (!*arg || *ptr || strchr(arg, '(') || strchr(arg, '(') ||
		    strchr(arg, ';')) {
		    LispFputs(Stdout, "* Bad function name '");
		    LispFputs(Stdout, arg);
		    LispFputs(Stdout, "' specified.\n");
		}
		else {
		    for (obj = frm = BRK; obj != NIL; frm = obj, obj = CDR(obj))
			;
		    i = lisp__data.debug_break;
		    ++lisp__data.debug_break;
		    GCDisable();
		    obj = CONS(ATOM(arg),
			       CONS(FIXNUM(i),
				    CONS(FIXNUM(LispDebugBreakFunction),
					 CONS(FIXNUM(0), NIL))));
		    if (BRK == NIL)
			BRK = CONS(obj, NIL);
		    else
			RPLACD(frm, CONS(obj, NIL));
		    GCEnable();
		}
		break;
	    case DebuggerWatch: {
		void *sym;
		int vframe;
		LispObj *val, *atom;

		/* make variable name uppercase, an ATOM */
		ptr = arg;
		while (*ptr) {
		    *ptr = toupper(*ptr);
		    ++ptr;
		}
		atom = ATOM(arg);
		val = LispGetVar(atom);
		if (val == NULL) {
		    LispFputs(Stdout, "* No variable named '");
		    LispFputs(Stdout, arg);
		    LispFputs(Stdout, "' in the selected frame.\n");
		    break;
		}

		/* variable is available at the current frame */
		sym = LispGetVarAddr(atom);

		/* find the lowest frame where the variable is visible */
		vframe = 0;
		if (frame > 0) {
		    for (; vframe < frame; vframe++) {
			for (frm = DBG, i = lisp__data.debug_level; i > vframe;
			     frm = CDR(frm), i--)
			    ;
			obj = CAR(frm);
			lisp__data.stack.base = FIXNUM_VALUE(CAR(CDR(CDR(obj))));
			lisp__data.env.length = FIXNUM_VALUE(CAR(CDR(CDR(CDR(obj)))));
			lisp__data.env.lex = FIXNUM_VALUE(CDR(CDR(CDR(CDR(obj)))));

			if (LispGetVarAddr(atom) == sym)
			    /* got variable initial frame */
			    break;
		    }
		    vframe = i;
		    if (vframe != frame) {
			/* restore environment */
			for (frm = DBG, i = lisp__data.debug_level; i > frame;
			     frm = CDR(frm), i--)
			    ;
			obj = CAR(frm);
			lisp__data.stack.base = FIXNUM_VALUE(CAR(CDR(CDR(obj))));
			lisp__data.env.length = FIXNUM_VALUE(CAR(CDR(CDR(CDR(obj)))));
			lisp__data.env.lex = FIXNUM_VALUE(CDR(CDR(CDR(CDR(obj)))));
		    }
		}

		i = lisp__data.debug_break;
		++lisp__data.debug_break;
		for (obj = frm = BRK; obj != NIL; frm = obj, obj = CDR(obj))
		    ;

		GCDisable();
		obj = CONS(atom,					/* NAM */
			   CONS(FIXNUM(i),				/* IDX */
				CONS(FIXNUM(LispDebugBreakVariable),	/* TYP */
				     CONS(FIXNUM(0),			/* HIT */
					  CONS(OPAQUE(sym, 0),		/* VAR */
					       CONS(val,		/* VAL */
						    CONS(FIXNUM(vframe),/* FRM */
							      NIL)))))));

		/* add watchpoint */
		if (BRK == NIL)
		    BRK = CONS(obj, NIL);
		else
		    RPLACD(frm, CONS(obj, NIL));
		GCEnable();
	    }	break;
	    case DebuggerDelete:
		if (*arg == 0) {
		    int confirm = 0;

		    for (;;) {
			int ch;

			LispFputs(Stdout, "* Delete all breakpoints? (y or n) ");
			LispFflush(Stdout);
			if ((ch = LispFgetc(Stdin)) == '\n')
			    continue;
			while ((i = LispFgetc(Stdin)) != '\n' && i != EOF)
			    ;
			if (tolower(ch) == 'n')
			    break;
			else if (tolower(ch) == 'y') {
			    confirm = 1;
			    break;
			}
		    }
		    if (confirm)
			BRK = NIL;
		}
		else {
		    for (ptr = arg; *ptr;) {
			while (*ptr && isdigit(*ptr))
			    ++ptr;
			if (*ptr && !isspace(*ptr)) {
			    *ptr = '\0';
			    LispFputs(Stdout, "* Bad breakpoint number '");
			    LispFputs(Stdout, arg);
			    LispFputs(Stdout, "' specified.\n");
			    break;
			}
			i = atoi(arg);
			for (obj = frm = BRK; frm != NIL;
			     obj = frm, frm = CDR(frm))
			    if (FIXNUM_VALUE(CAR(CDR(CAR(frm)))) == i)
				break;
			if (frm == NIL) {
			    LispFputs(Stdout, "* No breakpoint number ");
			    LispFputs(Stdout, arg);
			    LispFputs(Stdout, " available.\n");
			    break;
			}
			if (obj == frm)
			    BRK = CDR(BRK);
			else
			    RPLACD(obj, CDR(frm));
			while (*ptr && isspace(*ptr))
			    ++ptr;
			arg = ptr;
		    }
		}
		break;
	    case DebuggerFrame:
		i = -1;
		ptr = arg;
		if (*ptr) {
		    i = 0;
		    while (*ptr && isdigit(*ptr)) {
			i *= 10;
			i += *ptr - '0';
			++ptr;
		    }
		    if (*ptr) {
			LispFputs(Stdout, "* Frame identifier must "
				"be a positive number.\n");
			break;
		    }
		}
		else
		    goto debugger_print_frame;
		if (i >= 0 && i <= lisp__data.debug_level)
		    goto debugger_new_frame;
		LispFputs(Stdout, "* No such frame ");
		LispFputs(Stdout, format_integer(i));
		LispFputs(Stdout, ".\n");
		break;
	    case DebuggerDown:
		if (frame + 1 > lisp__data.debug_level) {
		    LispFputs(Stdout, "* Cannot go down.\n");
		    break;
		}
		i = frame + 1;
		goto debugger_new_frame;
		break;
	    case DebuggerUp:
		if (frame == 0) {
		    LispFputs(Stdout, "* Cannot go up.\n");
		    break;
		}
		i = frame - 1;
		goto debugger_new_frame;
		break;
	    case DebuggerPrint:
		ptr = arg;
		while (*ptr) {
		    *ptr = toupper(*ptr);
		    ++ptr;
		}
		obj = LispGetVar(ATOM(arg));
		if (obj != NULL) {
		    LispWriteObject(NIL, obj);
		    LispFputc(Stdout, '\n');
		}
		else {
		    LispFputs(Stdout, "* No variable named '");
		    LispFputs(Stdout, arg);
		    LispFputs(Stdout, "' in the selected frame.\n");
		}
		break;
	    case DebuggerBacktrace:
debugger_print_backtrace:
		if (DBG == NIL) {
		    LispFputs(Stdout, "* No stack.\n");
		    break;
		}
		DBG = LispReverse(DBG);
		for (obj = DBG, i = 0; obj != NIL; obj = CDR(obj), i++) {
		    frm = CAR(obj);
		    LispFputc(Stdout, '#');
		    LispFputs(Stdout, format_integer(i));
		    LispFputs(Stdout, "> (");
		    LispWriteObject(NIL, CAR(frm));
		    LispFputc(Stdout, ' ');
		    LispWriteObject(NIL, CAR(CDR(frm)));
		    LispFputs(Stdout, ")\n");
		}
		DBG = LispReverse(DBG);
		break;
	    case DebuggerContinue:
		lisp__data.debug = LispDebugRun;
		goto debugger_command_done;
	    case DebuggerFinish:
		if (lisp__data.debug != LispDebugFinish) {
		    lisp__data.debug_step = lisp__data.debug_level - 2;
		    lisp__data.debug = LispDebugFinish;
		}
		else
		    lisp__data.debug_step = lisp__data.debug_level - 1;
		goto debugger_command_done;
	    case DebuggerNext:
		if (lisp__data.debug != LispDebugNext) {
		    lisp__data.debug = LispDebugNext;
		    lisp__data.debug_step = lisp__data.debug_level + 1;
		}
		goto debugger_command_done;
	    case DebuggerStep:
		lisp__data.debug = LispDebugStep;
		goto debugger_command_done;
	}
	continue;

debugger_new_frame:
	/* goto here with i as the new frame value, after error checking */
	if (i != frame) {
	    frame = i;
	    for (frm = DBG, i = lisp__data.debug_level;
		 i > frame; frm = CDR(frm), i--)
		;
	    curframe = CAR(frm);
	    lisp__data.stack.base = FIXNUM_VALUE(CAR(CDR(CDR(curframe))));
	    lisp__data.env.length = FIXNUM_VALUE(CAR(CDR(CDR(CDR(curframe)))));
	    lisp__data.env.lex = FIXNUM_VALUE(CDR(CDR(CDR(CDR(curframe)))));
	}
debugger_print_frame:
	LispFputc(Stdout, '#');
	LispFputs(Stdout, format_integer(frame));
	LispFputs(Stdout, "> (");
	LispWriteObject(NIL, CAR(curframe));
	LispFputc(Stdout, ' ');
	LispWriteObject(NIL, CAR(CDR(curframe)));
	LispFputs(Stdout, ")\n");
    }

debugger_command_done:
    lisp__data.stack.base = envbase;
    lisp__data.env.length = envlen;
    lisp__data.env.lex = envlex;
}

static char *
format_integer(int integer)
{
    static char buffer[16];

    sprintf(buffer, "%d", integer);

    return (buffer);
}

#endif /* DEBUGGER */
