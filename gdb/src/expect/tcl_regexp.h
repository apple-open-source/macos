/*
 * Definitions etc. for regexp(3) routines.
 *
 * Caveat:  this is V8 regexp(3) [actually, a reimplementation thereof],
 * not the System V one.
 *
 * RCS: @(#) tcl_regexp.h,v 1.1 2001/09/08 06:26:30 irox Exp
 */

#ifndef _TCL_REGEXP
#define _TCL_REGEXP 1

/*
 * NSUBEXP must be at least 10, and no greater than 117 or the parser
 * will not work properly.
 */

#define NSUBEXP  20

typedef struct Expect_regexp {
	char *startp[NSUBEXP];
	char *endp[NSUBEXP];
	char regstart;		/* Internal use only. */
	char reganch;		/* Internal use only. */
	char *regmust;		/* Internal use only. */
	int regmlen;		/* Internal use only. */
	char program[1];	/* Unwarranted chumminess with compiler. */
} Expect_regexp;

EXTERN Expect_regexp *Expect_TclRegComp _ANSI_ARGS_((char *exp));
EXTERN int Expect_TclRegExec _ANSI_ARGS_((Expect_regexp *prog, char *string, char *start));
EXTERN void Expect_TclRegSub _ANSI_ARGS_((Expect_regexp *prog, char *source, char *dest));
EXTERN void Expect_TclRegError _ANSI_ARGS_((char *msg));
EXTERN char *Expect_TclGetRegError _ANSI_ARGS_((void));

#endif /* TCL_REGEXP */

