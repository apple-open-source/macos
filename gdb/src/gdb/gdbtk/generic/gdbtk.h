/* Tcl/Tk interface routines header file.
   Copyright 1994, 1995, 1996, 1997, 1998, 2000, 2001, 2003
   Free Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.

   This file is part of GDB.  It contains the public data that is shared between
   the gdbtk startup code and the gdbtk commands.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef _GDBTK_H
#define _GDBTK_H

#ifdef _WIN32
#define GDBTK_PATH_SEP ";"
#else
#define GDBTK_PATH_SEP ":"
#endif

/* Some versions (1.3.79, 1.3.81) of Linux don't support SIOCSPGRP the way
   gdbtk wants to use it... */
#ifdef __linux__
#undef SIOCSPGRP
#endif

/*
 *  These are the version numbers for GDBTK.  There is a package require
 *  statement in main.tcl that checks the version.  If you make an incompatible
 *  change to the gdb commands, or add any new commands, be sure to bump the
 *  version number both here and in main.tcl.  This will save us the trouble of
 *  having a version of gdb find the wrong versions of the Tcl libraries.
 */

#define GDBTK_MAJOR_VERSION "1"
#define GDBTK_MINOR_VERSION "0"
#define GDBTK_VERSION       "1.0"

/*
 * These are variables that are needed in gdbtk commands. 
 */

/* This variable determines where memory used for disassembly is read from.
   If > 0, then disassembly comes from the exec file rather than the
   target (which might be at the other end of a slow serial link).  If
   == 0 then disassembly comes from target.  If < 0 disassembly is
   automatically switched to the target if it's an inferior process,
   otherwise the exec file is used.  It is defined in gdbtk.c */


extern int disassemble_from_exec;

/* This variable is true when the inferior is running.  Although it's
   possible to disable most input from widgets and thus prevent
   attempts to do anything while the inferior is running, any commands
   that get through - even a simple memory read - are Very Bad, and
   may cause GDB to crash or behave strangely.  So, this variable
   provides an extra layer of defense.  It is defined in gdbtk.c */

extern int running_now;

/* These two control how the GUI behaves when tracing or loading
   They are defined in gdbtk-cmds.c */

extern int No_Update;
extern int load_in_progress;

/* This is the main gdbtk interpreter.  It is defined and initialized
   in gdbtk.c */

extern Tcl_Interp *gdbtk_interp;

/*
 * This structure controls how the gdb output is fed into gdbtk_call_wrapper invoked
 * commands.  See the explanation of gdbtk_fputs in gdbtk_hooks.c for more details.
 */

typedef struct gdbtk_result
  {
    Tcl_Obj *obj_ptr;		/* This will eventually be copied over to the 
				   Tcl result */
    int flags;			/* Flag vector to control how the result is
				   used. */
  }
gdbtk_result;

struct target_ops;

/* These defines give the allowed values for the gdbtk_result.flags field. */

#define GDBTK_TO_RESULT     1	/* This controls whether output from
				   gdbtk_fputs goes to the command result, or 
				   to gdbtk_tcl_fputs. */
#define GDBTK_MAKES_LIST    2	/* whether gdbtk_fputs adds the 
				   element it is outputting as a string, or
				   as a separate list element. */
#define GDBTK_IN_TCL_RESULT 4	/* Indicates that the result is already in the
				   Tcl result.  You can use this to preserve
				   error messages from functions like
				   Tcl_GetIntFromObj.  You can also store the
				   output of a call wrapped command directly in 
				   the Tcl result if you want, but beware, it will
				   not then be preserved across recursive
				   gdbtk_call_wrapper invocations. */
#define GDBTK_ERROR_STARTED 8	/* This one is just used in gdbtk_fputs.  If we 
				   see some output on stderr, we need to clear
				   the result we have been accumulating, or the 
				   error and the previous successful output
				   will get mixed, which would be confusing. */
#define GDBTK_ERROR_ONLY    16	/* Indicates that all incoming I/O is
				   to be treated as if it had arrived for gdb_stderr. This is
				   used to help error_begin in utils.c. */

/* This is a pointer to the gdbtk_result struct that
   we are currently filling.  We use the C stack to make a stack of these
   structures for nested calls to gdbtk commands that are invoked through
   the gdbtk_call_wrapper mechanism.  See that function for more details. */

extern gdbtk_result *result_ptr;

/* If you want to restore an old value of result_ptr whenever cleanups
   are run, pass this function to make_cleanup, along with the value
   of result_ptr you'd like to reinstate.  */
extern void gdbtk_restore_result_ptr (void *);

/* GDB context identifier */
extern int gdb_context;

/* Internal flag used to tell callers of deprecated_ui_loop_hook
   whether they should detach from the target. See explanations before
   x_event and gdb_stop. */
extern int gdbtk_force_detach;

/*
 * These functions are used in all the modules of Gdbtk.
 * 
 */

extern int Gdbtk_Init (Tcl_Interp * interp);
extern void gdbtk_stop_timer (void);
extern void gdbtk_start_timer (void);
extern void gdbtk_ignorable_warning (const char *, const char *);
extern void gdbtk_interactive (void);
extern int x_event (int);
extern int gdbtk_two_elem_cmd (char *, char *);
extern int target_is_native (struct target_ops *t);
extern void gdbtk_fputs (const char *, struct ui_file *);
extern struct ui_file *gdbtk_fileopen (void);
extern struct ui_file *gdbtk_fileopenin (void);
extern int gdbtk_disable_fputs;

#ifdef _WIN32
extern void close_bfds ();
#endif /* _WIN32 */

extern void
  TclDebug (char level, const char *fmt,...);

/* A convenience macro for getting the demangled source names,
   regardless of the user's mangling style. */
#define GDBTK_SYMBOL_SOURCE_NAME(symbol) \
      (SYMBOL_DEMANGLED_NAME (symbol) != NULL \
       ? SYMBOL_DEMANGLED_NAME (symbol)       \
       : DEPRECATED_SYMBOL_NAME (symbol))


/* gdbtk_add_hooks - add all the hooks to gdb.  This will get called
   by the startup code to fill in the hooks needed by core gdb. */
extern void gdbtk_add_hooks (void);

/* Initialize Insight */
extern void gdbtk_init (void);

/* Start Insight. Insight must have already been initialized with a call
   to gdbtk_init. */
extern void gdbtk_source_start_file (void);
#endif /* !_GDBTK_H */
