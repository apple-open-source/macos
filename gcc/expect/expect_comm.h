/* expectcomm.h - public symbols common to both expect.h and expect_tcl.h

Written by: Don Libes, libes@cme.nist.gov, NIST, 12/3/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.
*/

#ifndef _EXPECT_COMM_H
#define _EXPECT_COMM_H

#if 0
#include "expect_cf.h"
#endif

#include <stdio.h>
#include <setjmp.h>

/* since it's possible that the caller may include tcl.h before including
   this file, we cannot include varargs/stdargs ourselves */

/* Much of the following stdarg/prototype support is taken from tcl.h
 * (7.5) with modifications.  What's going on here is that don't want
 * to simply include tcl.h everywhere, because one of the files is the
 * Tcl-less Expect library.)
 */


/* Definitions that allow Tcl functions with variable numbers of
 * arguments to be used with either varargs.h or stdarg.h.
 * TCL_VARARGS is used in procedure prototypes.  TCL_VARARGS_DEF is
 * used to declare the arguments in a function definiton: it takes the
 * type and name of the first argument and supplies the appropriate
 * argument declaration string for use in the function definition.
 * TCL_VARARGS_START initializes the va_list data structure and
 * returns the first argument.  */

/* in Tcl 7.5, Tcl now supplies these definitions */
#if !defined(TCL_VARARGS)
#  if defined(__STDC__) || defined(HAVE_STDARG_H)
#   include <stdarg.h>
#   define TCL_VARARGS(type, name) (type name, ...)
#   define TCL_VARARGS_DEF(type, name) (type name, ...)
#   define TCL_VARARGS_START(type, name, list) (va_start(list, name), name)
#  else
#   include <varargs.h>
#   ifdef __cplusplus
#	define TCL_VARARGS(type, name) (type name, ...)
#	define TCL_VARARGS_DEF(type, name) (type va_alist, ...)
#   else
#	define TCL_VARARGS(type, name) ()
#	define TCL_VARARGS_DEF(type, name) (va_alist)
#   endif
#   define TCL_VARARGS_START(type, name, list) \
	(va_start(list), va_arg(list, type))
#  endif /* use stdarg.h */

/*
 * Definitions that allow this header file to be used either with or
 * without ANSI C features like function prototypes.
 */

#  undef _ANSI_ARGS_
#  undef CONST

#  if ((defined(__STDC__) || defined(SABER)) && !defined(NO_PROTOTYPE)) || defined(__cplusplus) || defined(USE_PROTOTYPE)
#    define _USING_PROTOTYPES_ 1
#    define _ANSI_ARGS_(x)	x
#    define CONST const
#  else
#    define _ANSI_ARGS_(x)	()
#    define CONST
#  endif

#  ifdef __cplusplus
#    define EXTERN extern "C"
#  else
#    define EXTERN extern
#  endif

#endif /* defined(TCL_VARARGS) */

/* Arghhh!  Tcl pulls in all of tcl.h in order to get the regexp funcs */
/* Tcl offers us a way to avoid this: temporarily define _TCL.  Here goes: */

#ifdef EXP_AVOID_INCLUDING_TCL_H
# ifdef _TCL
#  define EXP__TCL_WAS_DEFINED
# else
#  define _TCL
# endif
#endif

#include "tclRegexp.h"

/* clean up the mess */
#ifdef EXP_AVOID_INCLUDING_TCL_H
# ifdef EXP__TCL_WAS_DEFINED
#  undef EXP__TCL_WAS_DEFINED
# else
#  undef _TCL
# endif
#endif

#if 0
/* moved to exp_int.h so expect_cf.h no longer needs to be installed */
#ifdef NO_STDLIB_H
#  include "../compat/stdlib.h"
#else
#  include <stdlib.h>		/* for malloc */
#endif /*NO_STDLIB_H*/
#endif

/* common return codes for Expect functions */
/* The library actually only uses TIMEOUT and EOF */
#define EXP_ABEOF	-1	/* abnormal eof in Expect */
				/* when in library, this define is not used. */
				/* Instead "-1" is used literally in the */
				/* usual sense to check errors in system */
				/* calls */
#define EXP_TIMEOUT	-2
#define EXP_TCLERROR	-3
#define EXP_FULLBUFFER	-5
#define EXP_MATCH	-6
#define EXP_NOMATCH	-7
#define EXP_CANTMATCH	EXP_NOMATCH
#define EXP_CANMATCH	-8
#define EXP_DATA_NEW	-9	/* if select says there is new data */
#define EXP_DATA_OLD	-10	/* if we already read data in another cmd */
#define EXP_EOF		-11
#define EXP_RECONFIGURE	-12	/* changes to indirect spawn id lists */
				/* require us to reconfigure things */

/* in the unlikely event that a signal handler forces us to return this */
/* through expect's read() routine, we temporarily convert it to this. */
#define EXP_TCLRET	-20
#define EXP_TCLCNT	-21
#define EXP_TCLCNTTIMER	-22
#define EXP_TCLBRK	-23
#define EXP_TCLCNTEXP	-24
#define EXP_TCLRETTCL	-25

/* yet more TCL return codes */
/* Tcl does not safely provide a way to define the values of these, so */
/* use ridiculously numbers for safety */
#define EXP_CONTINUE		-101	/* continue expect command */
					/* and restart timer */
#define EXP_CONTINUE_TIMER	-102	/* continue expect command */
					/* and continue timer */
#define EXP_TCL_RETURN		-103	/* converted by interact */
					/* and interpeter from */
					/* inter_return into */
					/* TCL_RETURN*/

#define EXP_TIME_INFINITY	-1
#define EXP_SPAWN_ID_BAD	-1

EXTERN int exp_is_debugging;
EXTERN int exp_loguser;
EXTERN int exp_disconnected;		/* proc. disc'd from controlling tty */

EXTERN void (*exp_close_in_child)();	/* procedure to close files in child */
EXTERN void exp_close_tcl_files();	/* deflt proc: close all Tcl's files */

EXTERN void exp_slave_control _ANSI_ARGS_((int,int));

EXTERN char *exp_pty_error;		/* place to pass a string generated */
					/* deep in the innards of the pty */
					/* code but needed by anyone */

#endif /* _EXPECT_COMM_H */
