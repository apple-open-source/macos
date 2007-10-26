/* Startup code for Insight
   Copyright 1994, 1995, 1996, 1997, 1998, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

   Written by Stu Grossman <grossman@cygnus.com> of Cygnus Support.

   This file is part of GDB.

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

#include "defs.h"
#include "inferior.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbcore.h"
#include "tracepoint.h"
#include "demangle.h"
#include "version.h"
#include "top.h"
#include "annotate.h"
#include "exceptions.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* tcl header files includes varargs.h unless HAS_STDARG is defined,
   but gdb uses stdarg.h, so make sure HAS_STDARG is defined.  */
#define HAS_STDARG 1

#include <itcl.h>
#include <itk.h>
#include "guitcl.h"
#include "gdbtk.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>

#include "gdb_string.h"
#include "dis-asm.h"
#include "gdbcmd.h"

#ifdef __CYGWIN32__
#include <sys/cygwin.h>		/* for cygwin32_attach_handle_to_fd */
#endif

extern void _initialize_gdbtk (void);

/* For unix natives, we use a timer to periodically keep the gui alive.
   See comments before x_event. */
static sigset_t nullsigmask;
static struct sigaction act1, act2;
static struct itimerval it_on, it_off;

static void
x_event_wrapper (int signo)
{
  x_event (signo);
}

/*
 * This variable controls the interaction with an external editor.
 */

char *external_editor_command = NULL;

extern int Tktable_Init (Tcl_Interp * interp);

void gdbtk_init (void);

static void gdbtk_init_1 (char *argv0);

void gdbtk_interactive (void);

static void cleanup_init (void *ignore);

static void tk_command (char *, int);

static int target_should_use_timer (struct target_ops *t);

int target_is_native (struct target_ops *t);

int gdbtk_test (char *);

static void view_command (char *, int);

/* Handle for TCL interpreter */
Tcl_Interp *gdbtk_interp = NULL;

static int gdbtk_timer_going = 0;

/* linked variable used to tell tcl what the current thread is */
int gdb_context = 0;

/* This variable is true when the inferior is running.  See note in
 * gdbtk.h for details.
 */
int running_now;

/* This variable holds the name of a Tcl file which should be sourced by the
   interpreter when it goes idle at startup. Used with the testsuite. */
static char *gdbtk_source_filename = NULL;

int gdbtk_disable_fputs = 1;

static const char *argv0; 

#ifndef _WIN32

/* Supply malloc calls for tcl/tk.  We do not want to do this on
   Windows, because Tcl_Alloc is probably in a DLL which will not call
   the mmalloc routines.
   We also don't need to do it for Tcl/Tk8.1, since we locally changed the
   allocator to use malloc & free. */

#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0
char *
TclpAlloc (unsigned int size)
{
  return xmalloc (size);
}

char *
TclpRealloc (char *ptr, unsigned int size)
{
  return xrealloc (ptr, size);
}

void
TclpFree (char *ptr)
{
  free (ptr);
}
#endif /* TCL_VERSION == 8.0 */

#endif /* ! _WIN32 */

#ifdef _WIN32

/* On Windows, if we hold a file open, other programs can't write to
 * it.  In particular, we don't want to hold the executable open,
 * because it will mean that people have to get out of the debugging
 * session in order to remake their program.  So we close it, although
 * this will cost us if and when we need to reopen it.
 */

void
close_bfds ()
{
  struct objfile *o;
  
  ALL_OBJFILES (o)
    {
      if (o->obfd != NULL)
	bfd_cache_close (o->obfd);
    }
  
  if (exec_bfd != NULL)
    bfd_cache_close (exec_bfd);
}

#endif /* _WIN32 */


/* TclDebug (const char *fmt, ...) works just like printf() but 
 * sends the output to the GDB TK debug window. 
 * Not for normal use; just a convenient tool for debugging
 */

void
TclDebug (char level, const char *fmt,...)
{
  va_list args;
  char *buf;
  const char *v[3];
  char *merge;
  char *priority;

  switch (level)
    {
    case 'W':
      priority = "W";
      break;
    case 'E':
      priority = "E";
      break;
    case 'X':
      priority = "X";
      break;
    default:
      priority = "I";
    }

  va_start (args, fmt);


  xvasprintf (&buf, fmt, args);
  va_end (args);

  v[0] = "dbug";
  v[1] = priority;
  v[2] = buf;

  merge = Tcl_Merge (3, v);
  if (Tcl_Eval (gdbtk_interp, merge) != TCL_OK)
    Tcl_BackgroundError (gdbtk_interp);
  Tcl_Free (merge);
  free(buf);
}


/*
 * The rest of this file contains the start-up, and event handling code for gdbtk.
 */

/*
 * This cleanup function is added to the cleanup list that surrounds the Tk
 * main in gdbtk_init.  It deletes the Tcl interpreter.
 */

static void
cleanup_init (void *ignore)
{
  if (gdbtk_interp != NULL)
    Tcl_DeleteInterp (gdbtk_interp);
  gdbtk_interp = NULL;
}

/* Come here during long calculations to check for GUI events.  Usually invoked
   via the QUIT macro.  */

void
gdbtk_interactive ()
{
  /* Tk_DoOneEvent (TK_DONT_WAIT|TK_IDLE_EVENTS); */
}

/* Start a timer which will keep the GUI alive while in target_wait. */
void
gdbtk_start_timer ()
{
  static int first = 1;

  if (first)
    {
      /* first time called, set up all the structs */
      first = 0;
      sigemptyset (&nullsigmask);

      act1.sa_handler = x_event_wrapper;
      act1.sa_mask = nullsigmask;
      act1.sa_flags = 0;

      act2.sa_handler = SIG_IGN;
      act2.sa_mask = nullsigmask;
      act2.sa_flags = 0;

      it_on.it_interval.tv_sec = 0;
      it_on.it_interval.tv_usec = 250000;	/* .25 sec */
      it_on.it_value.tv_sec = 0;
      it_on.it_value.tv_usec = 250000;

      it_off.it_interval.tv_sec = 0;
      it_off.it_interval.tv_usec = 0;
      it_off.it_value.tv_sec = 0;
      it_off.it_value.tv_usec = 0;
    }

  if (target_should_use_timer (&current_target))
    {
      if (!gdbtk_timer_going)
	{
	  sigaction (SIGALRM, &act1, NULL);
	  setitimer (ITIMER_REAL, &it_on, NULL);
	  gdbtk_timer_going = 1;
	}
    }
  return;
}

/* Stop the timer if it is running. */
void
gdbtk_stop_timer ()
{
  if (gdbtk_timer_going)
    {
      gdbtk_timer_going = 0;
      setitimer (ITIMER_REAL, &it_off, NULL);
      sigaction (SIGALRM, &act2, NULL);
    }
  return;
}

/* Should this target use the timer? See comments before
   x_event for the logic behind all this. */
static int
target_should_use_timer (struct target_ops *t)
{
  return target_is_native (t);
}

/* Is T a native target? */
int
target_is_native (struct target_ops *t)
{
  char *name = t->to_shortname;

  if (strcmp (name, "exec") == 0 || strcmp (name, "hpux-threads") == 0
      || strcmp (name, "child") == 0 || strcmp (name, "procfs") == 0
      || strcmp (name, "solaris-threads") == 0
      || strcmp (name, "linuxthreads") == 0
      || strcmp (name, "multi-thread") == 0)
    return 1;

  return 0;
}

/* gdbtk_init installs this function as a final cleanup.  */

static void
gdbtk_cleanup (PTR dummy)
{
  Tcl_Eval (gdbtk_interp, "gdbtk_cleanup");
  Tcl_Finalize ();
}


/* Initialize gdbtk.  This involves creating a Tcl interpreter,
 * defining all the Tcl commands that the GUI will use, pointing
 * all the gdb "hooks" to the correct functions,
 * and setting the Tcl auto loading environment so that we can find all
 * the Tcl based library files.
 */

void
gdbtk_init (void)
{
  struct cleanup *old_chain;
  char *s;
  int element_count;
  const char **exec_path;
  CONST char *internal_exec_name;

  /* If there is no DISPLAY environment variable, Tk_Init below will fail,
     causing gdb to abort.  If instead we simply return here, gdb will
     gracefully degrade to using the command line interface. */

#ifndef _WIN32
  if (getenv ("DISPLAY") == NULL)
    {
      return;
    }
#endif

  old_chain = make_cleanup (cleanup_init, 0);

  /* First init tcl and tk. */
  Tcl_FindExecutable (argv0);
  gdbtk_interp = Tcl_CreateInterp ();

#ifdef TCL_MEM_DEBUG
  Tcl_InitMemory (gdbtk_interp);
#endif

  if (!gdbtk_interp)
    error ("Tcl_CreateInterp failed");

  /* We need to check if we are being run from
     a bin directory, if not then we may have to
     set some environment variables. */

  internal_exec_name = Tcl_GetNameOfExecutable ();

  Tcl_SplitPath ((char *) internal_exec_name, &element_count, &exec_path);

  if (strcmp (exec_path[element_count - 2], "bin") != 0)
    {
      /* We check to see if TCL_LIBRARY, TK_LIBRARY,
	 ITCL_LIBRARY, ITK_LIBRARY, TIX_LIBRARY and maybe
	 a couple other environment variables have been
	 set (we don't want to override the User's settings).
	 If the *_LIBRARY variable is not set, point it at
	 the source directory. */

      static char set_libs_path_script[] = "\
	  set srcDir [file dirname [file dirname $env(TCL_LIBRARY)]];\n\
\
	  if {![info exists env(TK_LIBRARY)]} {\n\
	      set env(TK_LIBRARY) [file join $srcDir tk library]\n\
	  }\n\
\
	  if {![info exists env(ITCL_LIBRARY)]} {\n\
	      set env(ITCL_LIBRARY) [file join $srcDir itcl itcl library]\n\
	  }\n\
\
	  if {![info exists env(ITK_LIBRARY)]} {\n\
	      set env(ITK_LIBRARY) [file join $srcDir itcl itk library]\n\
	  }\n\
\
	  if {![info exists env(IWIDGETS_LIBRARY)]} {\n\
	      set env(IWIDGETS_LIBRARY)\
                     [file join $srcDir itcl iwidgets generic]\n\
	  }\n\
\
	  if {![info exists env(TIX_LIBRARY)]} {\n\
	      set env(TIX_LIBRARY) [file join $srcDir tix library]\n\
	  }\n\
\
	  if {![info exists env(GDBTK_LIBRARY)]} {\n\
	      set env(GDBTK_LIBRARY) [file join $srcDir gdb gdbtk library]\n\
	  }\n\
\
          # Append the directory with the itcl pkg index\n\
          if {[info exists env(TCLLIBPATH)]} {\n\
            append env(TCLLIBPATH) :[file joing $srcDir itcl]\n\
          } else {\n\
            set env(TCLLIBPATH) [file join $srcDir itcl]\n\
          }\n\
\
          # We also need to append the iwidgets library path.\n\
          # Unfortunately, there is no IWIDGETS_LIBRARY.\n\
          set IWIDGETS_LIBRARY [file join $srcDir itcl iwidgets generic]\n";

      Tcl_Obj *commandObj;

      /* Before we can run our script we must set TCL_LIBRARY. */
      if (Tcl_GetVar2 (gdbtk_interp, "env", "TCL_LIBRARY", TCL_GLOBAL_ONLY) == NULL)
	{
	  int i, count;
	  char *src_dir = SRC_DIR;
	  const char **src_path;
	  const char **lib_path;
	  Tcl_DString lib_dstring;

	  Tcl_DStringInit (&lib_dstring);

#ifdef __CYGWIN__
	  /* SRC_DIR from configure is a posix path. Tcl really needs a
	     windows path. */
	  src_dir = (char *) alloca (cygwin_posix_to_win32_path_list_buf_size (SRC_DIR));
	  cygwin_posix_to_win32_path_list (SRC_DIR, src_dir);
#endif
	  Tcl_SplitPath (src_dir, &count, &src_path);

	  /* Append tcl/library to src_dir (src_dir=/foo/bar/gdb) */
	  lib_path = (const char **) alloca ((count + 2) * sizeof (char *));
	  for (i = 0; i < count - 1; i++)
	    lib_path[i] = src_path[i];
	  lib_path[i++] = "tcl";
	  lib_path[i++] = "library";
	  Tcl_JoinPath (i, lib_path, &lib_dstring);

	  /* Set TCL_LIBRARY */
	  Tcl_SetVar2 (gdbtk_interp, "env", "TCL_LIBRARY",
		       Tcl_DStringValue (&lib_dstring) , TCL_GLOBAL_ONLY);
	  Tcl_DStringFree (&lib_dstring);
	  Tcl_Free ((char *) src_path);
	}

      commandObj = Tcl_NewStringObj (set_libs_path_script, -1);
      Tcl_IncrRefCount (commandObj);
      Tcl_EvalObj (gdbtk_interp, commandObj);
      Tcl_DecrRefCount (commandObj);
    }

  Tcl_Free ((char *) exec_path);

  if (Tcl_Init (gdbtk_interp) != TCL_OK)
    error ("Tcl_Init failed: %s", gdbtk_interp->result);

  /* Set up some globals used by gdb to pass info to gdbtk
     for start up options and the like */
  xasprintf (&s, "%d", inhibit_gdbinit);
  Tcl_SetVar2 (gdbtk_interp, "GDBStartup", "inhibit_prefs", s, TCL_GLOBAL_ONLY);
  free(s);
   
  /* Note: Tcl_SetVar2() treats the value as read-only (making a
     copy).  Unfortunately it does not mark the parameter as
     ``const''. */
  Tcl_SetVar2 (gdbtk_interp, "GDBStartup", "host_name", (char*) host_name, TCL_GLOBAL_ONLY);
  Tcl_SetVar2 (gdbtk_interp, "GDBStartup", "target_name", (char*) target_name, TCL_GLOBAL_ONLY);

  make_final_cleanup (gdbtk_cleanup, NULL);

  /* Initialize the Paths variable.  */
  if (ide_initialize_paths (gdbtk_interp, "") != TCL_OK)
    error ("ide_initialize_paths failed: %s", gdbtk_interp->result);

  if (Tk_Init (gdbtk_interp) != TCL_OK)
    error ("Tk_Init failed: %s", gdbtk_interp->result);

  if (Itcl_Init (gdbtk_interp) == TCL_ERROR)
    error ("Itcl_Init failed: %s", gdbtk_interp->result);
  Tcl_StaticPackage (gdbtk_interp, "Itcl", Itcl_Init,
		     (Tcl_PackageInitProc *) NULL);

  if (Itk_Init (gdbtk_interp) == TCL_ERROR)
    error ("Itk_Init failed: %s", gdbtk_interp->result);
  Tcl_StaticPackage (gdbtk_interp, "Itk", Itk_Init,
		     (Tcl_PackageInitProc *) NULL);

  if (Tktable_Init (gdbtk_interp) != TCL_OK)
    error ("Tktable_Init failed: %s", gdbtk_interp->result);

  Tcl_StaticPackage (gdbtk_interp, "Tktable", Tktable_Init,
		     (Tcl_PackageInitProc *) NULL);
  /*
   * These are the commands to do some Windows Specific stuff...
   */

#ifdef __CYGWIN32__
  if (ide_create_messagebox_command (gdbtk_interp) != TCL_OK)
    error ("messagebox command initialization failed");
  /* On Windows, create a sizebox widget command */
#if 0
  if (ide_create_sizebox_command (gdbtk_interp) != TCL_OK)
    error ("sizebox creation failed");
#endif
  if (ide_create_winprint_command (gdbtk_interp) != TCL_OK)
    error ("windows print code initialization failed");
  if (ide_create_win_grab_command (gdbtk_interp) != TCL_OK)
    error ("grab support command initialization failed");
  /* Path conversion functions.  */
  if (ide_create_cygwin_path_command (gdbtk_interp) != TCL_OK)
    error ("cygwin path command initialization failed");
  if (ide_create_shell_execute_command (gdbtk_interp) != TCL_OK)
    error ("cygwin shell execute command initialization failed");
#endif

  /* Only for testing -- and only when it can't be done any
     other way. */
  if (cyg_create_warp_pointer_command (gdbtk_interp) != TCL_OK)
    error ("warp_pointer command initialization failed");

  /*
   * This adds all the Gdbtk commands.
   */

  if (Gdbtk_Init (gdbtk_interp) != TCL_OK)
    {
      error ("Gdbtk_Init failed: %s", gdbtk_interp->result);
    }

  Tcl_StaticPackage (gdbtk_interp, "Insight", Gdbtk_Init, NULL);

  /* Add a back door to Tk from the gdb console... */

  add_com ("tk", class_obscure, tk_command,
	   "Send a command directly into tk.");

  add_com ("view", class_obscure, view_command,
	   "View a location in the source window.");

  /*
   * Set the variable for external editor:
   */

  if (external_editor_command != NULL)
    {
      Tcl_SetVar (gdbtk_interp, "external_editor_command",
		  external_editor_command, 0);
      xfree (external_editor_command);
      external_editor_command = NULL;
    }

#ifdef __CYGWIN32__
  (void) FreeConsole ();
#endif

  discard_cleanups (old_chain);
}

void
gdbtk_source_start_file (void)
{
  /* find the gdb tcl library and source main.tcl */
#ifdef NO_TCLPRO_DEBUGGER
  static char script[] = "\
proc gdbtk_find_main {} {\n\
    global Paths GDBTK_LIBRARY\n\
    rename gdbtk_find_main {}\n\
    tcl_findLibrary insight 1.0 {} main.tcl GDBTK_LIBRARY GDBTKLIBRARY\n\
    set Paths(appdir) $GDBTK_LIBRARY\n\
}\n\
gdbtk_find_main";
#else
    static char script[] = "\
proc gdbtk_find_main {} {\n\
    global Paths GDBTK_LIBRARY env\n\
    rename gdbtk_find_main {}\n\
    if {[info exists env(DEBUG_STUB)]} {\n\
        source $env(DEBUG_STUB)\n\
        debugger_init\n\
        set debug_startup 1\n\
    } else {\n\
        set debug_startup 0\n\
    }\n\
    tcl_findLibrary insight 1.0 {} main.tcl GDBTK_LIBRARY GDBTK_LIBRARY\n\
    set Paths(appdir) $GDBTK_LIBRARY\n\
}\n\
gdbtk_find_main";
#endif /* NO_TCLPRO_DEBUGGER */

  /* now enable gdbtk to parse the output from gdb */
  gdbtk_disable_fputs = 0;
    
  if (Tcl_GlobalEval (gdbtk_interp, (char *) script) != TCL_OK)
    {
      const char *msg;

      /* Force errorInfo to be set up propertly.  */
      Tcl_AddErrorInfo (gdbtk_interp, "");
      msg = Tcl_GetVar (gdbtk_interp, "errorInfo", TCL_GLOBAL_ONLY);

#ifdef _WIN32
      /* On windows, display the error using a pop-up message box.
	 If GDB wasn't started from the DOS prompt, the user won't
	 get to see the failure reason.  */
      MessageBox (NULL, msg, NULL, MB_OK | MB_ICONERROR | MB_TASKMODAL);
      {
        struct gdb_exception e;
        e.reason  = RETURN_ERROR;
        e.error   = GENERIC_ERROR;
        e.message = msg;
        throw_exception (e);
      }
#else
      /* FIXME: cagney/2002-04-17: Wonder what the lifetime of
	 ``msg'' is - does it need a cleanup?  */
      error ("%s", msg);
#endif
    }

  /* Now source in the filename provided by the --tclcommand option.
     This is mostly used for the gdbtk testsuite... */

  if (gdbtk_source_filename != NULL)
    {
      char *s = "after idle source ";
      char *script = concat (s, gdbtk_source_filename, (char *) NULL);
      Tcl_Eval (gdbtk_interp, script);
      free (gdbtk_source_filename);
      free (script);
    }
}

static void
gdbtk_init_1 (char *arg0)
{
  argv0 = arg0;
  deprecated_init_ui_hook = NULL;
}

/* gdbtk_test is used in main.c to validate the -tclcommand option to
   gdb, which sources in a file of tcl code after idle during the
   startup procedure. */

int
gdbtk_test (char *filename)
{
  if (access (filename, R_OK) != 0)
    return 0;
  else
    gdbtk_source_filename = xstrdup (filename);
  return 1;
}

/* Come here during initialize_all_files () */

void
_initialize_gdbtk ()
{
  /* Current_interpreter not set yet, so we must check
     if "interpreter_p" is set to "insight" to know if
     insight is GOING to run. */
  if (strcmp (interpreter_p, "insight") == 0)
    deprecated_init_ui_hook = gdbtk_init_1;
#ifdef __CYGWIN__
  else
    {
      DWORD ft = GetFileType (GetStdHandle (STD_INPUT_HANDLE));

      switch (ft)
	{
	case FILE_TYPE_DISK:
	case FILE_TYPE_CHAR:
	case FILE_TYPE_PIPE:
	  break;
	default:
	  AllocConsole ();
	  cygwin32_attach_handle_to_fd ("/dev/conin", 0,
					GetStdHandle (STD_INPUT_HANDLE),
					1, GENERIC_READ);
	  cygwin32_attach_handle_to_fd ("/dev/conout", 1,
					GetStdHandle (STD_OUTPUT_HANDLE),
					0, GENERIC_WRITE);
	  cygwin32_attach_handle_to_fd ("/dev/conout", 2,
					GetStdHandle (STD_ERROR_HANDLE),
					0, GENERIC_WRITE);
	  break;
	}
    }
#endif
}

static void
tk_command (char *cmd, int from_tty)
{
  int retval;
  char *result;
  struct cleanup *old_chain;

  /* Catch case of no argument, since this will make the tcl interpreter 
     dump core. */
  if (cmd == NULL)
    error_no_arg ("tcl command to interpret");

  retval = Tcl_Eval (gdbtk_interp, cmd);

  result = xstrdup (gdbtk_interp->result);

  old_chain = make_cleanup (free, result);

  if (retval != TCL_OK)
    error ("%s", result);

  printf_unfiltered ("%s\n", result);

  do_cleanups (old_chain);
}

static void
view_command (char *args, int from_tty)
{
  char *script;
  struct cleanup *old_chain;

  if (args != NULL)
    {
      xasprintf (&script,
		 "[lindex [ManagedWin::find SrcWin] 0] location BROWSE_TAG [gdb_loc %s]",
		 args);
      old_chain = make_cleanup (xfree, script);
      if (Tcl_Eval (gdbtk_interp, script) != TCL_OK)
	{
	  Tcl_Obj *obj = Tcl_GetObjResult (gdbtk_interp);
	  error ("%s", Tcl_GetStringFromObj (obj, NULL));
	}

      do_cleanups (old_chain);
    }
  else
    error ("Argument required (location to view)");
}
