/* Target-vector operations for controlling win32 child processes, for GDB.
   Copyright 1995, 1996
   Free Software Foundation, Inc.

   Contributed by Cygnus Support.
   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without eve nthe implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* by Steve Chamberlain, sac@cygnus.com */

#include "defs.h"
#include "frame.h"		/* required by inferior.h */
#include "inferior.h"
#include "target.h"
#include "wait.h"
#include "gdbcore.h"
#include "command.h"
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <windows.h>
#include "buildsym.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdb_string.h"
#include "gdbthread.h"
#include "gdbcmd.h"

#if !defined NeXT_PDO
#include <sys/param.h>
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

enum dirty_bits { READ_FROM_CHILD = 0, WRITE_TO_CHILD = -1};

#define CONTEXT_DEBUGGER /* Hey, we're a debugger - we want it all! */ \
       (CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS | \
		CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS)

#define CHECK(x) 	check (x, __FILE__,__LINE__)
#define DEBUG_EXEC(x)	if (debug_exec)		printf_filtered x
#define DEBUG_EVENTS(x)	if (debug_events)	printf_filtered x
#define DEBUG_MEM(x)	if (debug_memory)	printf_filtered x
#define DEBUG_EXCEPT(x)	if (debug_exceptions)	printf_filtered x

typedef struct DBGTHREAD
{
  HANDLE	handle;
  CONTEXT	context;
  int		id;
  void *	startAddr;
  struct minimal_symbol *startSym;
  int		suspended;
  int		runnable;
  int		priority;
  struct DBGTHREAD *next;
} DbgThread;

typedef struct DBGPROCESS
{
  HANDLE	handle;
  int		id;
  int		eventThreadId;
  int		priority;
  int		haltedByDebugEvent;
  int		haltedByCtrlC;
  int		continue_code;
  DbgThread *	threads;
} DbgProcess;

/* Forward declaration */
extern struct target_ops child_ops;

/* The process and thread handles for the current context. */

static DbgProcess current_process = {0, 0, 0, 0, 0, 0, 0};
static DbgThread *current_thread;
       int ctrlc_received_by_debugger = 0;

/* Counts of things. */
static int exception_count = 0;
static int event_count = 0;

/* User options. */
static int new_console      = 0;
static int new_group        = 0;
static int pass_ctrlc       = 0;
static int dos_path_style   = 0;
static int debug_exec       = 0;	/* show execution */
static int debug_events     = 0;	/* show events from kernel */
static int debug_memory     = 0;	/* show target memory accesses */
static int debug_exceptions = 0;	/* show target exceptions */

/* Call dummy support */
typedef struct {
   DbgThread dummythread;
   HANDLE handle;
   int 	  id;
   DbgThread *oldthread;
   int haltedByCtrlC;
   } SavedThreadInfo;

static SavedThreadInfo call_dummy_saved_thread = {{0},0, 0, 0, 0};


/* This vector maps GDB's idea of a register's number into an address
   in the win32 exception context vector. 

   It also contains the bit mask needed to load the register in question.  

   One day we could read a reg, we could inspect the context we
   already have loaded, if it doesn't have the bit set that we need,
   we read that set of registers in using GetThreadContext.  If the
   context already contains what we need, we just unpack it. Then to
   write a register, first we have to ensure that the context contains
   the other regs of the group, and then we copy the info in and set
   out bit. */

#if 0	/* replaced below: MVS */

struct regmappings
  {
    char *incontext;
    int mask;
  };


static const struct regmappings  mappings[] =
{
#ifdef __PPC__
  {(char *) &context.Gpr0, CONTEXT_INTEGER},
  {(char *) &context.Gpr1, CONTEXT_INTEGER},
  {(char *) &context.Gpr2, CONTEXT_INTEGER},
  {(char *) &context.Gpr3, CONTEXT_INTEGER},
  {(char *) &context.Gpr4, CONTEXT_INTEGER},
  {(char *) &context.Gpr5, CONTEXT_INTEGER},
  {(char *) &context.Gpr6, CONTEXT_INTEGER},
  {(char *) &context.Gpr7, CONTEXT_INTEGER},

  {(char *) &context.Gpr8, CONTEXT_INTEGER},
  {(char *) &context.Gpr9, CONTEXT_INTEGER},
  {(char *) &context.Gpr10, CONTEXT_INTEGER},
  {(char *) &context.Gpr11, CONTEXT_INTEGER},
  {(char *) &context.Gpr12, CONTEXT_INTEGER},
  {(char *) &context.Gpr13, CONTEXT_INTEGER},
  {(char *) &context.Gpr14, CONTEXT_INTEGER},
  {(char *) &context.Gpr15, CONTEXT_INTEGER},

  {(char *) &context.Gpr16, CONTEXT_INTEGER},
  {(char *) &context.Gpr17, CONTEXT_INTEGER},
  {(char *) &context.Gpr18, CONTEXT_INTEGER},
  {(char *) &context.Gpr19, CONTEXT_INTEGER},
  {(char *) &context.Gpr20, CONTEXT_INTEGER},
  {(char *) &context.Gpr21, CONTEXT_INTEGER},
  {(char *) &context.Gpr22, CONTEXT_INTEGER},
  {(char *) &context.Gpr23, CONTEXT_INTEGER},

  {(char *) &context.Gpr24, CONTEXT_INTEGER},
  {(char *) &context.Gpr25, CONTEXT_INTEGER},
  {(char *) &context.Gpr26, CONTEXT_INTEGER},
  {(char *) &context.Gpr27, CONTEXT_INTEGER},
  {(char *) &context.Gpr28, CONTEXT_INTEGER},
  {(char *) &context.Gpr29, CONTEXT_INTEGER},
  {(char *) &context.Gpr30, CONTEXT_INTEGER},
  {(char *) &context.Gpr31, CONTEXT_INTEGER},

  {(char *) &context.Fpr0, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr1, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr2, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr3, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr4, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr5, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr6, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr7, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr8, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr9, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr10, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr11, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr12, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr13, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr14, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr15, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr16, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr17, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr18, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr19, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr20, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr21, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr22, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr23, CONTEXT_FLOATING_POINT},

  {(char *) &context.Fpr24, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr25, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr26, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr27, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr28, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr29, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr30, CONTEXT_FLOATING_POINT},
  {(char *) &context.Fpr31, CONTEXT_FLOATING_POINT},


  {(char *) &context.Iar, CONTEXT_CONTROL},
  {(char *) &context.Msr, CONTEXT_CONTROL},
  {(char *) &context.Cr,  CONTEXT_INTEGER},
  {(char *) &context.Lr,  CONTEXT_CONTROL},
  {(char *) &context.Ctr, CONTEXT_CONTROL},

  {(char *) &context.Xer, CONTEXT_INTEGER},
  {0,0}, /* MQ, but there isn't one */
#else
  {(char *) &context.Eax, CONTEXT_INTEGER},
  {(char *) &context.Ecx, CONTEXT_INTEGER},
  {(char *) &context.Edx, CONTEXT_INTEGER},
  {(char *) &context.Ebx, CONTEXT_INTEGER},
  {(char *) &context.Esp, CONTEXT_CONTROL},
  {(char *) &context.Ebp, CONTEXT_CONTROL},
  {(char *) &context.Esi, CONTEXT_INTEGER},
  {(char *) &context.Edi, CONTEXT_INTEGER},
  {(char *) &context.Eip, CONTEXT_CONTROL},
  {(char *) &context.EFlags, CONTEXT_CONTROL},
  {(char *) &context.SegCs, CONTEXT_SEGMENTS},
  {(char *) &context.SegSs, CONTEXT_SEGMENTS},
  {(char *) &context.SegDs, CONTEXT_SEGMENTS},
  {(char *) &context.SegEs, CONTEXT_SEGMENTS},
  {(char *) &context.SegFs, CONTEXT_SEGMENTS},
  {(char *) &context.SegGs, CONTEXT_SEGMENTS},
  {&context.FloatSave.RegisterArea[0 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[1 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[2 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[3 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[4 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[5 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[6 * 10], CONTEXT_FLOATING_POINT},
  {&context.FloatSave.RegisterArea[7 * 10], CONTEXT_FLOATING_POINT},
#endif
};

#else	/* new version: MVS */

static const int mappings[] =
{
#ifdef __PPC__
  (int) &((CONTEXT *) 0)->Gpr0, 
  (int) &((CONTEXT *) 0)->Gpr1, 
  (int) &((CONTEXT *) 0)->Gpr2, 
  (int) &((CONTEXT *) 0)->Gpr3, 
  (int) &((CONTEXT *) 0)->Gpr4, 
  (int) &((CONTEXT *) 0)->Gpr5, 
  (int) &((CONTEXT *) 0)->Gpr6, 
  (int) &((CONTEXT *) 0)->Gpr7, 

  (int) &((CONTEXT *) 0)->Gpr8, 
  (int) &((CONTEXT *) 0)->Gpr9, 
  (int) &((CONTEXT *) 0)->Gpr10, 
  (int) &((CONTEXT *) 0)->Gpr11, 
  (int) &((CONTEXT *) 0)->Gpr12, 
  (int) &((CONTEXT *) 0)->Gpr13, 
  (int) &((CONTEXT *) 0)->Gpr14, 
  (int) &((CONTEXT *) 0)->Gpr15, 

  (int) &((CONTEXT *) 0)->Gpr16, 
  (int) &((CONTEXT *) 0)->Gpr17, 
  (int) &((CONTEXT *) 0)->Gpr18, 
  (int) &((CONTEXT *) 0)->Gpr19, 
  (int) &((CONTEXT *) 0)->Gpr20, 
  (int) &((CONTEXT *) 0)->Gpr21, 
  (int) &((CONTEXT *) 0)->Gpr22, 
  (int) &((CONTEXT *) 0)->Gpr23, 

  (int) &((CONTEXT *) 0)->Gpr24, 
  (int) &((CONTEXT *) 0)->Gpr25, 
  (int) &((CONTEXT *) 0)->Gpr26, 
  (int) &((CONTEXT *) 0)->Gpr27, 
  (int) &((CONTEXT *) 0)->Gpr28, 
  (int) &((CONTEXT *) 0)->Gpr29, 
  (int) &((CONTEXT *) 0)->Gpr30, 
  (int) &((CONTEXT *) 0)->Gpr31, 

  (int) &((CONTEXT *) 0)->Fpr0, 
  (int) &((CONTEXT *) 0)->Fpr1, 
  (int) &((CONTEXT *) 0)->Fpr2, 
  (int) &((CONTEXT *) 0)->Fpr3, 
  (int) &((CONTEXT *) 0)->Fpr4, 
  (int) &((CONTEXT *) 0)->Fpr5, 
  (int) &((CONTEXT *) 0)->Fpr6, 
  (int) &((CONTEXT *) 0)->Fpr7, 

  (int) &((CONTEXT *) 0)->Fpr8, 
  (int) &((CONTEXT *) 0)->Fpr9, 
  (int) &((CONTEXT *) 0)->Fpr10, 
  (int) &((CONTEXT *) 0)->Fpr11, 
  (int) &((CONTEXT *) 0)->Fpr12, 
  (int) &((CONTEXT *) 0)->Fpr13, 
  (int) &((CONTEXT *) 0)->Fpr14, 
  (int) &((CONTEXT *) 0)->Fpr15, 

  (int) &((CONTEXT *) 0)->Fpr16, 
  (int) &((CONTEXT *) 0)->Fpr17, 
  (int) &((CONTEXT *) 0)->Fpr18, 
  (int) &((CONTEXT *) 0)->Fpr19, 
  (int) &((CONTEXT *) 0)->Fpr20, 
  (int) &((CONTEXT *) 0)->Fpr21, 
  (int) &((CONTEXT *) 0)->Fpr22, 
  (int) &((CONTEXT *) 0)->Fpr23, 

  (int) &((CONTEXT *) 0)->Fpr24, 
  (int) &((CONTEXT *) 0)->Fpr25, 
  (int) &((CONTEXT *) 0)->Fpr26, 
  (int) &((CONTEXT *) 0)->Fpr27, 
  (int) &((CONTEXT *) 0)->Fpr28, 
  (int) &((CONTEXT *) 0)->Fpr29, 
  (int) &((CONTEXT *) 0)->Fpr30, 
  (int) &((CONTEXT *) 0)->Fpr31, 


  (int) &((CONTEXT *) 0)->Iar, 
  (int) &((CONTEXT *) 0)->Msr, 
  (int) &((CONTEXT *) 0)->Cr, 
  (int) &((CONTEXT *) 0)->Lr, 
  (int) &((CONTEXT *) 0)->Ctr, 

  (int) &((CONTEXT *) 0)->Xer, 
  0,0}, /* MQ, but there isn't one */
#else
  (int) &((CONTEXT *) 0)->Eax, 
  (int) &((CONTEXT *) 0)->Ecx, 
  (int) &((CONTEXT *) 0)->Edx, 
  (int) &((CONTEXT *) 0)->Ebx, 
  (int) &((CONTEXT *) 0)->Esp, 
  (int) &((CONTEXT *) 0)->Ebp, 
  (int) &((CONTEXT *) 0)->Esi, 
  (int) &((CONTEXT *) 0)->Edi, 
  (int) &((CONTEXT *) 0)->Eip, 
  (int) &((CONTEXT *) 0)->EFlags, 
  (int) &((CONTEXT *) 0)->SegCs, 
  (int) &((CONTEXT *) 0)->SegSs, 
  (int) &((CONTEXT *) 0)->SegDs, 
  (int) &((CONTEXT *) 0)->SegEs, 
  (int) &((CONTEXT *) 0)->SegFs, 
  (int) &((CONTEXT *) 0)->SegGs, 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[0 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[1 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[2 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[3 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[4 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[5 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[6 * 10], 
  (int) &((CONTEXT *) 0)->FloatSave.RegisterArea[7 * 10], 
#endif
};

#endif /* 0 */

/* This vector maps the target's idea of an exception (extracted
   from the DEBUG_EVENT structure) to GDB's idea. */

struct xlate_exception
  {
    int them;
    enum target_signal us;
  };


static const struct xlate_exception
  xlate[] =
{
  {EXCEPTION_ACCESS_VIOLATION, TARGET_SIGNAL_SEGV},
  {STATUS_STACK_OVERFLOW, TARGET_SIGNAL_SEGV},
  {EXCEPTION_BREAKPOINT, TARGET_SIGNAL_TRAP},
  {DBG_CONTROL_C, TARGET_SIGNAL_INT},
  {EXCEPTION_SINGLE_STEP, TARGET_SIGNAL_TRAP},
  {-1, -1}};


/*
 * Error checking and reporting routines:
 */

static void 
PrintErrorFormatted (err)
     int err;
{
  char *lpMessageBuffer;

  FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		 NULL,
		 err,
		 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		 (LPTSTR) &lpMessageBuffer, 
		 0, 
		 NULL);
  fprintf (stderr, lpMessageBuffer);
  if (lpMessageBuffer[strlen (lpMessageBuffer) - 1] != '\n')
    fprintf (stderr, "\n");
  LocalFree ((LPVOID) lpMessageBuffer);
}

static int
check (ok, file, line)
     int ok;
     const char *file;
     int line;
{
  if (!ok)
    {
      fprintf (stderr, "Win32 error at %s line %d:\n\t", file, line);
      PrintErrorFormatted (GetLastError ());
    }
  return ok;
}

int isWin95()
{
    static int is95 = -1;
    if (is95 < 0) {
        OSVERSIONINFO osvi;
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!CHECK(GetVersionEx(&osvi))) {
            fprintf(stderr, "Can't determine operating system version.\n");
            }
        else
            is95 = (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
        }
    return is95;
}

static void
switch_context (tid)
     int tid;
{
  DbgThread *thread;
  static int child_thread_alive ();

  if (tid == current_thread->id)
    return;	/* new thread is current thread -- nothing to do */

  if (!(thread = (DbgThread *) child_thread_alive (tid)))
    error ("attempt to switch to invalid thread");

  current_thread = thread;
  registers_changed ();
}


static void
child_fetch_inferior_registers (int r)
{
  if (current_thread && current_thread->id != inferior_pid)
    switch_context (inferior_pid);

  if (current_thread->context.ContextFlags == READ_FROM_CHILD)
    { /* need to read registers for this thread from the child */
      current_thread->context.ContextFlags = CONTEXT_DEBUGGER;
      CHECK (GetThreadContext (current_thread->handle, 
			       &current_thread->context));
    }

  if (r < 0)
    {
      for (r = 0; r < NUM_REGS; r++)
	child_fetch_inferior_registers (r);
    }
  else
    {
      supply_register (r, ((char *) &current_thread->context) + mappings[r]);
    }
}

static void
child_store_inferior_registers (int r)
{
  if (current_thread && current_thread->id != inferior_pid)
    switch_context (inferior_pid);

  if (current_thread->context.ContextFlags == READ_FROM_CHILD)
    { /* need to read registers for this thread from the child */
      current_thread->context.ContextFlags = CONTEXT_DEBUGGER;
      CHECK (GetThreadContext (current_thread->handle, 
			       &current_thread->context));
    }

  if (r < 0)
    {
      for (r = 0; r < NUM_REGS; r++)
	child_store_inferior_registers (r);
    }
  else
    { /* dirty, needs to be saved to the child process */
      current_thread->context.ContextFlags = WRITE_TO_CHILD;
      read_register_gen (r, ((char *) &current_thread->context) + mappings[r]);
    }
}

/* Load symbols from a newly-loaded DLL */

static int
handle_load_dll (char *eventp)
{
  DEBUG_EVENT * event = (DEBUG_EVENT *) eventp;
  DWORD dll_name_ptr;
  DWORD done;

  ReadProcessMemory (current_process.handle,
		     (void *) event->u.LoadDll.lpImageName,
		     (char *) &dll_name_ptr,
		     sizeof (dll_name_ptr), &done);

  /* See if we could read the address of a string, and that the 
     address isn't null. */

  if (done == sizeof (dll_name_ptr) && dll_name_ptr)
    {
      char *dll_name, *dll_basename;
      struct objfile *objfile;
      char unix_dll_name[MAX_PATH];
      int size = event->u.LoadDll.fUnicode ? sizeof (WCHAR) : sizeof (char);
      int len = 0;
      char b[2];
      do
	{
	  ReadProcessMemory (current_process.handle,
			     (void *) (dll_name_ptr + len * size),
			     &b,
			     size,
			     &done);
	  len++;
	}
      while ((b[0] != 0 || b[size - 1] != 0) && done == size);

      dll_name = alloca (len);

      if (event->u.LoadDll.fUnicode)
	{
	  WCHAR *unicode_dll_name = (WCHAR *) alloca (len * sizeof (WCHAR));
	  ReadProcessMemory (current_process.handle,
			     (void *) dll_name_ptr,
			     unicode_dll_name,
			     len * sizeof (WCHAR),
			     &done);

	  WideCharToMultiByte (CP_ACP, 0,
			       unicode_dll_name, len,
			       dll_name, len, 0, 0);
	}
      else
	{
	  ReadProcessMemory (current_process.handle,
			     (void *) dll_name_ptr,
			     dll_name,
			     len,
			     &done);
	}


      dos_path_to_unix_path (dll_name, unix_dll_name);

      /* FIXME!! It would be nice to define one symbol which pointed to the 
         front of the dll if we can't find any symbols. */

      if (!(dll_basename = strrchr (dll_name, '\\')))
	dll_basename = strrchr (dll_name, '/');

      ALL_OBJFILES(objfile)
	{
	  char *objfile_basename;
	  if (!(objfile_basename = strrchr (objfile->name, '\\')))
	    objfile_basename = strrchr (objfile->name, '/');

	  if (dll_basename && objfile_basename && 
	      strcmp (dll_basename + 1, objfile_basename + 1) == 0)
	    {
	      printf_filtered ("%s (symbols previously loaded)\n", 
			       dll_basename + 1);
	      return 1;
	    }
	}

      /* The symbols in a dll are offset by 0x1000, which is the
	 the offset from 0 of the first byte in an image - because
	 of the file header and the section alignment. 
	 
	 FIXME: Is this the real reason that we need the 0x1000 ? */

      objfile = symbol_file_add (unix_dll_name, 0, 
				 (int) event->u.LoadDll.lpBaseOfDll + 0x1000, 0,
				 0, 0, 0, 0, 0);

      if (objfile->minimal_symbol_count == 0)
	{
	  prim_record_minimal_symbol
	    (obsavestring (dll_basename + 1, strlen (dll_basename), 
			  &objfile->symbol_obstack),
	     (int) event->u.LoadDll.lpBaseOfDll + 0x1000, 
	     mst_text, objfile);
	  install_minimal_symbols (objfile);
	}

      registers_changed ();
      current_thread->context.ContextFlags = READ_FROM_CHILD;
      printf_filtered ("%x:%s\n", event->u.LoadDll.lpBaseOfDll, 
		       unix_dll_name);
    }
  return 1;
}

/* Function: handle_exception
 *
 * Returns: True if debugger should ignore exception (ie. continue), 
 *          False if debugger should stop for exception.
 */

typedef struct _ex_names {
  unsigned long      eno;
  char              *ename;
  enum target_signal sig;
} ex_names;

ex_names win32_exceptions[] = {
  {EXCEPTION_ACCESS_VIOLATION,          "ACCESS_VIOLATION", 
     TARGET_SIGNAL_SEGV}, 
  {EXCEPTION_ARRAY_BOUNDS_EXCEEDED,     "ARRAY_BOUNDS_EXCEEDED", 
     TARGET_SIGNAL_SEGV}, 
  {EXCEPTION_BREAKPOINT,                "BREAKPOINT ", 
     TARGET_SIGNAL_TRAP}, 
  {EXCEPTION_DATATYPE_MISALIGNMENT,     "DATATYPE_MISALIGNMENT", 
     TARGET_SIGNAL_SEGV}, 
  {EXCEPTION_FLT_DENORMAL_OPERAND,      "FLT_DENORMAL_OPERAND", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_DIVIDE_BY_ZERO,        "FLT_DIVIDE_BY_ZERO", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_INEXACT_RESULT,        "FLT_INEXACT", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_INVALID_OPERATION,     "FLT_INVALID_OPERATION", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_OVERFLOW,              "FLT_OVERFLOW", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_STACK_CHECK,           "FLT_STACK_CHECK", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_FLT_UNDERFLOW,             "FLT_UNDERFLOW", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_GUARD_PAGE,                "GUARD_PAGE", 
     TARGET_SIGNAL_SEGV}, 
  {EXCEPTION_ILLEGAL_INSTRUCTION,       "ILLEGAL_INSTRUCTION", 
     TARGET_SIGNAL_ILL}, 
  {EXCEPTION_IN_PAGE_ERROR,             "IN_PAGE_ERROR", 
     TARGET_SIGNAL_SEGV}, 
  {EXCEPTION_INT_DIVIDE_BY_ZERO,        "INT_DIVIDE_BY_ZERO", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_INT_OVERFLOW,              "INT_OVERFLOW", 
     TARGET_SIGNAL_FPE}, 
  {EXCEPTION_INVALID_DISPOSITION,       "INVALID_DISPOSITION", 
     TARGET_SIGNAL_UNKNOWN}, 
  {EXCEPTION_NONCONTINUABLE_EXCEPTION , "NONCONTINUABLE_EXCEPTION", 
     TARGET_SIGNAL_UNKNOWN}, 
  {EXCEPTION_PRIV_INSTRUCTION,          "PRIV_INSTRUCTION", 
     TARGET_SIGNAL_ILL}, 
  {EXCEPTION_SINGLE_STEP,               "SINGLE_STEP", 
     TARGET_SIGNAL_TRAP}, 
  {EXCEPTION_STACK_OVERFLOW,            "STACK_OVERFLOW", 
     TARGET_SIGNAL_SEGV}, 
  {0x0000DEAD,                          "Mach", 
     TARGET_SIGNAL_EMT}, 
  {DBG_CONTROL_C,                       "CONTROL_C", 
     TARGET_SIGNAL_INT}, 
  {DBG_CONTROL_BREAK,                   "CONTROL_BREAK", 
     TARGET_SIGNAL_INT}, 
  {0, 					"Unknown", 
     TARGET_SIGNAL_UNKNOWN}
};


static int exception_signal_severity = 3;
static int exception_report_severity = 4;

static int
handle_exception (ex, ourstatus)
     EXCEPTION_DEBUG_INFO *ex;
     struct target_waitstatus *ourstatus;
{
  enum target_signal sig = TARGET_SIGNAL_0;
  int severity = ex->ExceptionRecord.ExceptionCode >> 30;
  int i;

  current_process.continue_code = DBG_EXCEPTION_NOT_HANDLED;	/* default */

  for (i = 0; win32_exceptions[i].eno != 0; i++)
    if (ex->ExceptionRecord.ExceptionCode == win32_exceptions[i].eno)
      break;		/* found match: known exception */

  sig = win32_exceptions[i].sig;
  if (sig == TARGET_SIGNAL_TRAP)	/* traps must be handled by gdb */
    {
      current_process.continue_code = DBG_CONTINUE;
      ourstatus->value.sig = sig;
      return 0;
    }
  if (sig == TARGET_SIGNAL_INT)		/* ^C interrupt handled specially */
    {
      if (!pass_ctrlc)
	current_process.continue_code = DBG_CONTINUE;
      return 1;				/* don't signal */
    }

  /* OK: decide whether to report the exception */
  if (debug_exceptions || severity >= exception_report_severity ||
      !ex->dwFirstChance)		/* always report if not first chance */
    {					/* (see comment below) */
      target_terminal_ours ();
      printf_filtered ("Program received %s Win32 Exception ", 
		       win32_exceptions[i].ename);
      if (win32_exceptions[i].eno == 0)	/* unknown (to us) exception */
	printf_filtered ("(0x%08x) ",  ex->ExceptionRecord.ExceptionCode);
      printf_filtered ("at 0x%08x.\n", ex->ExceptionRecord.ExceptionAddress);
      if (ex->ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
	printf_filtered ("\t(attempting to %s memory at 0x%08x)\n", 
			 ex->ExceptionRecord.ExceptionInformation[0] ?
			 "write" : "read ",
			 ex->ExceptionRecord.ExceptionInformation[1]);
      if (!ex->dwFirstChance)
	{
	  /* not first chance --
	   * This means that all potential exception handlers (including
	   * the debugger) have already had a chance at this exception, 
	   * and no one has handled it.  The debugger is being given a
	   * last chance to handle it.  Otherwise this will become a 
	   * fatal exception to the child.  That's why we want to report
	   * it unconditionally.
	   */
	  printf_filtered ("\tWarning! Exception not handled by child.  ");
	  printf_filtered ("To continue will be fatal.\n");
	}
      if (severity < exception_signal_severity)
	target_terminal_inferior ();
    }

  /* And now decide whether to convert it to a signal and stop for it */
  if (severity >= exception_signal_severity ||
      !ex->dwFirstChance)
    {
      /*
       * See comment above about first chance.  If we don't take this
       * as a signal now, the child will unconditionally die.  Gdb's
       * built-in signal handling facilities may still choose to ignore it.
       */
      ourstatus->value.sig = sig;
      return 0;
    }
  else
    return 1;
}


static void
AddThread (process, tHandle, tId, startAddr, priority)
     DbgProcess *process;
     HANDLE	 tHandle;
     int	 tId;
     void *	 startAddr;
     int	 priority;
{
  DbgThread *new = malloc (sizeof (DbgThread));

  memset (new, 0, sizeof (DbgThread));
  new->handle    = tHandle;
  new->id        = tId;
  new->startAddr = startAddr;
  new->startSym = lookup_minimal_symbol_by_pc((CORE_ADDR)startAddr);
  new->priority  = priority;
  new->runnable  = TRUE;
  new->next      = process->threads;
  new->context.ContextFlags = READ_FROM_CHILD;
  process->threads = new;
  
  add_thread (tId);	/* inform gdb core of this new thread */
}

static void
RemoveThread (process, threadId)
     DbgProcess *process;
     int	 threadId;
{
  DbgThread *freeme, *dontfreeme;
  int removed_current_thread = (current_thread &&
				 current_thread->id == threadId);

  /* Never remove the call dummy thread */
  if (threadId == call_dummy_saved_thread.dummythread.id) return;
  
  if (threadId == process->threads->id)	/* special case -- first thread */
    {
      freeme = process->threads;
      CHECK (CloseHandle (freeme->handle));
      process->threads = process->threads->next;
      free (freeme);
    }
  else for (dontfreeme = process->threads; 
	    dontfreeme != NULL;
	    dontfreeme = dontfreeme->next)
    if (dontfreeme->next && dontfreeme->next->id == threadId)
      {
	freeme = dontfreeme->next;
	CHECK (CloseHandle (freeme->handle));
	dontfreeme->next = dontfreeme->next->next;
	free (freeme);
      }
  if (removed_current_thread)
    { /* have to switch to another "current" thread: how to pick one? */
      current_thread = current_process.threads;	/* should be safe */
    }
}

/*
 * Function: child_thread_alive
 *
 * Used to match a thread id with a currently known DbgThread struct.
 * Returns a pointer to the struct, or NULL, and can therefore also
 * be treated as returning BOOL found/not-found.
 */

static int
child_thread_alive (tId)
     int tId;
{
  DbgThread *thread;

  for (thread = current_process.threads; thread; thread = thread->next)
    if (thread->id == tId)
      return (int) thread;

  return (int) NULL;
}

static void
SuspendThreads (process)
     DbgProcess *process;
{
  DbgThread *thread = process->threads;

  while (thread)
    {
      CHECK (SuspendThread (thread->handle) != 0xffffffff);
      thread->suspended = TRUE;
      thread = thread->next;
    }
  process->haltedByCtrlC = TRUE;
}

static void
ResumeThreads (process)
     DbgProcess *process;
{
  DbgThread *thread = process->threads;

  while (thread)
    {
      if (thread->suspended && thread->runnable)
	CHECK (ResumeThread (thread->handle) != 0xffffffff);
      thread->suspended = FALSE;
      thread = thread->next;
    }
  process->haltedByCtrlC = FALSE;
}

static int
CtrlC_Handler (DWORD opcode)
{
  switch (opcode)
    {
    case CTRL_C_EVENT:
      ctrlc_received_by_debugger = TRUE;
      return TRUE;
    case CTRL_BREAK_EVENT:	/* Foundation sends this to kill us */
    default:
      return FALSE;
    }
}

/* Wait for child to do something.  Return pid of child, or -1 in case
   of error; store status through argument pointer OURSTATUS.  */

static int
child_wait (int pid, struct target_waitstatus *ourstatus)
{
  /* We loop when we get a non-standard exception rather than return
     with a SPURIOUS because resume can try and step or modify things,
     which needs a current_thread->handle.  But some of these exceptions mark
     the birth or death of threads, which mean that the current thread
     isn't necessarily what you think it is. */

  DEBUG_EVENT event;
  int db_ev;
  int temp;

  SetConsoleCtrlHandler ((void *) CtrlC_Handler, TRUE);

  while (TRUE)
    {
      if (db_ev = WaitForDebugEvent (&event, 100))
	{
	  if (event.dwProcessId != current_process.id)
	    continue;	/* some other process? */

	  event_count++;

	  current_process.id = event.dwProcessId;
	  current_process.haltedByDebugEvent = TRUE;
	  current_process.eventThreadId = event.dwThreadId;
	  current_process.continue_code = DBG_CONTINUE;	/* default */

	  switch (event.dwDebugEventCode)
	    {
	    case CREATE_THREAD_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n", 
			     event.dwProcessId, event.dwThreadId,
			     "CREATE_THREAD_DEBUG_EVENT"));
              if (event.dwThreadId != call_dummy_saved_thread.dummythread.id)
                AddThread (&current_process, 
			event.u.CreateThread.hThread, 
			event.dwThreadId, 
			event.u.CreateThread.lpStartAddress, 
			0 /* Priority? */ );
	      break;
	    case EXIT_THREAD_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "EXIT_THREAD_DEBUG_EVENT"));
              RemoveThread (&current_process, event.dwThreadId);
	      break;
	    case CREATE_PROCESS_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "CREATE_PROCESS_DEBUG_EVENT"));
	      current_process.handle   = event.u.CreateProcessInfo.hProcess;
	      current_process.id       = event.dwProcessId;
	      current_process.priority = 0; /* ? */
	      AddThread (&current_process, 
			 event.u.CreateProcessInfo.hThread, 
			 event.dwThreadId, 
			 event.u.CreateProcessInfo.lpStartAddress,
			 0 /* priority? */);
	      current_thread = current_process.threads;
	      CHECK (CloseHandle (event.u.CreateProcessInfo.hFile));
	      registers_changed ();	/* just to be safe */
	      break;
	    case EXIT_PROCESS_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "EXIT_PROCESS_DEBUG_EVENT"));
	      ourstatus->kind          = TARGET_WAITKIND_EXITED;
	      ourstatus->value.integer = event.u.ExitProcess.dwExitCode;

	      RemoveThread (&current_process, event.dwThreadId);
              if (call_dummy_saved_thread.dummythread.handle) {
	          CHECK (CloseHandle (call_dummy_saved_thread.dummythread.handle));
                  memset(&call_dummy_saved_thread, 0, sizeof (call_dummy_saved_thread));
                  }
                  
	      CHECK (CloseHandle (current_process.handle));

	      CHECK (ContinueDebugEvent (current_process.id,
					 current_process.eventThreadId,
					 current_process.continue_code));
	      current_process.handle = 0;
	      current_process.id     = 0;
	      current_thread         = 0;
	      registers_changed ();	/* just to be safe */
              goto child_wait_return;
	      break;
	    case LOAD_DLL_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "LOAD_DLL_DEBUG_EVENT"));
	      catch_errors (handle_load_dll,
			    (char*) &event,
			   "\n[failed reading symbols from DLL]\n", 
			   RETURN_MASK_ALL);
	      CHECK (CloseHandle (event.u.LoadDll.hFile));
	      break;
	    case UNLOAD_DLL_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "UNLOAD_DLL_DEBUG_EVENT"));
	      break;
	    case EXCEPTION_DEBUG_EVENT:
	      DEBUG_EVENTS (("gdb: kernel event for pid=%d tid=%d code=%s)\n",
			     event.dwProcessId, event.dwThreadId,
			     "EXCEPTION_DEBUG_EVENT"));
	      DEBUG_EVENTS (("     exception code = 0x%08x, flags = 0x%08x, pc = 0x%08x\n",
			     event.u.Exception.ExceptionRecord.ExceptionCode,
                             event.u.Exception.ExceptionRecord.ExceptionFlags,
                             event.u.Exception.ExceptionRecord.ExceptionAddress));
	      temp = event.u.Exception.ExceptionRecord.ExceptionCode;
#if 0
	      if (temp == DBG_CONTROL_C ||
		  temp == DBG_CONTROL_BREAK)
		{ /* treated differently than a regular exception */
		  if (pass_ctrlc)
		    current_process.continue_code = DBG_EXCEPTION_NOT_HANDLED;
		  /* else leave it as DBG_CONTINUE */
		  break;
		}
	      else
#endif
		{
		  if (!handle_exception (&event.u.Exception, ourstatus))
		    {
		      ourstatus->kind = TARGET_WAITKIND_STOPPED;
		      if (current_thread->id != event.dwThreadId) 
			switch_context (event.dwThreadId);
		      goto child_wait_return;
		    }
		  break;
		}
	    case OUTPUT_DEBUG_STRING_EVENT: /* message from the kernel */
	      {
		char *p;

		DEBUG_EVENTS (("gdb: output debug event for pid=%d tid=%d)\n",
			       event.dwProcessId, event.dwThreadId));
		if (target_read_string
		    ((CORE_ADDR) event.u.DebugString.lpDebugStringData, 
		     &p, 1024, 0) && p && *p)
		  {
		    char *last = &p[strlen (p)];

		    if (*last == '\n')
		      *last = '\0';	/* warning() provides newline */
		    warning (p);
		    free (p);
		  }		   
		break;
	      }
	    default:
	      printf_filtered ("gdb: kernel event for pid=%d tid=%d\n",
			       event.dwProcessId, event.dwThreadId);
	      printf_filtered ("                 unknown event code %d\n",
			       event.dwDebugEventCode);
	      break;
	    }
	  DEBUG_EVENTS (("ContinueDebugEvent (pid=%d, tid=%d, code=%d);\n",
			 current_process.id, 
			 current_process.eventThreadId,
			 current_process.continue_code));
	  CHECK (ContinueDebugEvent (current_process.id,
				     current_process.eventThreadId, 
				     current_process.continue_code));
	  current_process.haltedByDebugEvent = FALSE;
	}
      if (ctrlc_received_by_debugger)
        {
                SuspendThreads (&current_process);
                printf_filtered ("gdb: interrupted by ^C\n");
                ctrlc_received_by_debugger = FALSE;
                ourstatus->value.sig = TARGET_SIGNAL_INT;
                ourstatus->kind      = TARGET_WAITKIND_STOPPED;
                goto child_wait_return;
        }
    }
 child_wait_return:
  SetConsoleCtrlHandler ((void*) CtrlC_Handler, FALSE);
  registers_changed ();		/* mark all regs invalid */
  if (current_thread)
      return current_thread->id;
  else
    return inferior_pid;
}


/* Attach to process PID, then initialize for debugging it.  */

static void
child_attach (args, from_tty)
     char *args;
     int from_tty;
{
  int ok, pid;
  struct target_waitstatus dummy;

  if (!args)
    error_no_arg ("process-id to attach");

  pid = strtoul (args, 0, 0);

  CHECK (ok = DebugActiveProcess (pid));

  if (!ok)
    error ("Can't attach to process.");

  current_process.id = pid;
  exception_count = 0;
  event_count = 0;

  if (from_tty)
    {
      char *exec_file = (char *) get_exec_file (0);

      if (exec_file)
	printf_filtered ("Attaching to program `%s', %s\n", exec_file,
			 target_pid_to_str (current_process.id));
      else
	printf_filtered ("Attaching to %s\n",
			 target_pid_to_str (current_process.id));

      gdb_flush (gdb_stdout);
    }
  inferior_pid = current_process.id;
  push_target (&child_ops);
}


static void
child_detach (args, from_tty)
     char *args;
     int from_tty;
{
  if (from_tty)
    {
      char *exec_file = get_exec_file (0);
      if (exec_file == 0)
	exec_file = "";
      printf_filtered ("Detaching from program: %s %s\n", exec_file,
		       target_pid_to_str (inferior_pid));
      gdb_flush (gdb_stdout);
    }
  child_resume (current_process.id, 0, 0);

  while (current_process.threads)
    {
      current_thread = current_process.threads;
      RemoveThread (&current_process, current_thread->id);
    }
  current_thread = NULL;
  CHECK (CloseHandle (current_process.handle));
  current_process.id = 0;

  inferior_pid = 0;
  unpush_target (&child_ops);
}


/* Print status information about what we're accessing.  */

static void
child_files_info (ignore)
     struct target_ops *ignore;
{
  printf_filtered ("\tUsing the running image of %s %s.\n",
		   attach_flag ? "attached" : "child", 
		   target_pid_to_str (inferior_pid));
}

/* ARGSUSED */
static void
child_open (arg, from_tty)
     char *arg;
     int from_tty;
{
  error ("Use the \"run\" command to start a Unix child process.");
}

/* Convert a unix-style set-of-paths (a colon-separated list of directory
   paths with forward slashes) into the dos style (semicolon-separated 
   list with backward slashes), simultaneously undoing any translations
   performed by the mount table. */

static char *buf = NULL;
static int blen = 2000;

static char *
unix_paths_to_dos_paths (char *newenv)
{
  int ei;
  char *src;

  if (buf == 0)
    buf = (char *) malloc (blen);

  if (newenv == 0 || *newenv == 0 ||
     (src = strchr (newenv, '=')) == 0)	/* find the equals sign */
    return 0;

  /* Remove any PID supplied by us, so the child initializes
     nicely. */
  if (strncmp (newenv, "PID=", 4) == 0)
    {
      strcpy (buf, "PID=");
      return buf;
    }

  src++;				/* now skip past it */

  if (src[0] == '/' ||			/* is this a unix style path? */
     (src[0] == '.' && src[1] == '/') ||
     (src[0] == '.' && src[1] == '.' && src[2] == '/'))
    { /* we accept that we will fail on a relative path like 'foo/mumble' */
      /* Found an env name, turn from unix style into dos style */
      int len = src - newenv;
      char *dir = buf + len;

      memcpy (buf, newenv, len);
      /* Split out the colons */
      while (1)
	{
	  char *tok = strchr (src, ':');
	  int doff = dir - buf;

	  if (doff + MAX_PATH > blen) 
	    {
	      blen *= 2;
	      buf = (char *) realloc ((void *) buf, blen);
	      dir = buf + doff;
	    }
	  if (tok)
	    {
	      *tok = 0;
	      unix_path_to_dos_path_keep_rel (src, dir);
	      *tok = ':';
	      dir += strlen (dir);
	      src = tok + 1;
	      *dir++ = ';';
	    }
	  else
	    {
	      unix_path_to_dos_path_keep_rel (src, dir);
	      dir += strlen (dir);
	      *dir++ = 0;
	      break;
	    }
	}
      return buf;
    }
  return 0;
}

/* Convert a dos-style set-of-paths (a semicolon-separated list with
   backward slashes) into the dos style (colon-separated list of
   directory paths with forward slashes), simultaneously undoing any
   translations performed by the mount table. */

static char *
dos_paths_to_unix_paths (char *newenv)
{
  int ei;
  char *src;

  if (buf == 0)
    buf = (char *) malloc (blen);

  if (newenv == 0 || *newenv == 0 ||
     (src = strchr (newenv, '=')) == 0)	/* find the equals sign */
    return 0;

  src++;				/* now skip past it */

  if (src[0] == '\\' ||		/* is this a dos style path? */
     (isalpha (src[0]) && src[1] == ':' && src[2] == '\\') ||
     (src[0] == '.' && src[1] == '\\') ||
     (src[0] == '.' && src[1] == '.' && src[2] == '\\'))
    { /* we accept that we will fail on a relative path like 'foo\mumble' */
      /* Found an env name, turn from dos style into unix style */
      int len = src - newenv;
      char *dir = buf + len;

      memcpy (buf, newenv, len);
      /* Split out the colons */
      while (1)
	{
	  char *tok = strchr (src, ';');
	  int doff = dir - buf;
	  
	  if (doff + MAX_PATH > blen) 
	    {
	      blen *= 2;
	      buf = (char *) realloc ((void *) buf, blen);
	      dir = buf + doff;
	    }
	  if (tok)
	    {
	      *tok = 0;
	      dos_path_to_unix_path_keep_rel (src, dir);
	      *tok = ';';
	      dir += strlen (dir);
	      src = tok + 1;
	      *dir++ = ':';
	    }
	  else
	    {
	      dos_path_to_unix_path_keep_rel (src, dir);
	      dir += strlen (dir);
	      *dir++ = 0;
	      break;
	    }
	}
      return buf;
    }
  return 0;
}


/* Start an inferior win32 child process and sets inferior_pid to its pid.
   EXEC_FILE is the file to run.
   ALLARGS is a string containing the arguments to the program.
   ENV is the environment vector to pass.  Errors reported with error().  */


static void
child_create_inferior (exec_file, allargs, env)
     char *exec_file;
     char *allargs;
     char **env;
{
  char real_path[MAXPATHLEN];
  char *winenv;
  char *temp;
  int  envlen;
  int i;

  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  struct target_waitstatus dummy;
  int ret;
  DWORD flags;
  char *args;

  if (!exec_file)
    {
      error ("No executable specified, use `target exec'.\n");
    }

  memset (&si, 0, sizeof (si));
  si.cb      = sizeof (si);
  si.dwFlags = STARTF_FORCEONFEEDBACK;

  unix_path_to_dos_path (exec_file, real_path);

  flags = DEBUG_ONLY_THIS_PROCESS;

  if (new_group)
    flags |= CREATE_NEW_PROCESS_GROUP;

  if (new_console)
    flags |= CREATE_NEW_CONSOLE;

  args = alloca (strlen (real_path) + strlen (allargs) + 2);

  strcpy (args, real_path);

  strcat (args, " ");
  strcat (args, allargs);


  /* get total size for env strings */
  for (envlen = 0, i = 0; env[i] && *env[i]; i++)
    {
      winenv = unix_paths_to_dos_paths (env[i]);
      envlen += winenv ? strlen (winenv) + 1 : strlen (env[i]) + 1;
    }

  winenv = alloca (2 * envlen + 1);	/* allocate new buffer */

  /* copy env strings into new buffer */
  for (temp = winenv, i = 0; env[i] && *env[i];     i++) 
    {
      char *p = unix_paths_to_dos_paths (env[i]);
      strcpy (temp, p ? p : env[i]);
      temp += strlen (temp) + 1;
    }
  *temp = 0;			/* final nil string to terminate new env */

  ret = CreateProcess (0,
		       args, 	/* command line */
		       NULL,	/* Security */
		       NULL,	/* thread */
		       TRUE,	/* inherit handles */
		       flags,	/* start flags */
		       winenv,
		       NULL,	/* current directory */
		       &si,
		       &pi);
  if (!ret)
    error ("Error %d creating process %s\n", GetLastError (), exec_file);

  CHECK (CloseHandle (pi.hProcess));
  CHECK (CloseHandle (pi.hThread));
  current_process.id                 = pi.dwProcessId;
  current_process.priority           = 0;	/* zero priority? */
  current_process.haltedByDebugEvent = FALSE;
  current_process.haltedByCtrlC      = FALSE;
  current_process.continue_code      = DBG_CONTINUE;
  ctrlc_received_by_debugger         = FALSE;

  exception_count = 0;
  event_count = 0;

  inferior_pid = pi.dwThreadId;
  push_target (&child_ops);
  init_thread_list ();
  init_wait_for_inferior ();
  clear_proceed_status ();
  target_terminal_init ();
  target_terminal_inferior ();

  /* Ignore the first trap */
  child_wait (inferior_pid, &dummy);

  proceed ((CORE_ADDR) - 1, TARGET_SIGNAL_0, 0);
}

static void
child_mourn_inferior ()
{
  unpush_target (&child_ops);
  current_process.handle = 0;
  generic_mourn_inferior ();
}


/* Send a SIGINT to the process group.  This acts just like the user typed a
   ^C on the controlling terminal. */

void
child_stop ()
{
  DEBUG_EVENTS (("gdb: GenerateConsoleCtrlEvent (CTRLC_EVENT, %d)\n", 
		 current_process.id));
  SuspendThreads (&current_process);
  registers_changed ();		/* refresh register state */
}




int
child_xfer_memory (CORE_ADDR memaddr, char *our, int len,
		   int write, struct target_ops *target)
{
  DWORD done = -1;
  if (write)
    {
      DEBUG_MEM (("gdb: write target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      if (!WriteProcessMemory (current_process.handle, 
			       (void *) memaddr, our, len, &done))
	return -1;
      FlushInstructionCache (current_process.handle, (void *) memaddr, len);
    }
  else
    {
      DEBUG_MEM (("gdb: read target memory, %d bytes at 0x%08x\n",
		  len, memaddr));
      if (!ReadProcessMemory (current_process.handle, 
			      (void *) memaddr, our, len, &done))
	return -1;
    }
  return done;
}

void
child_kill_inferior (void)
{
  DbgThread *thread;
  struct target_waitstatus dummy;

#if 0
  /* This loop doesn't work in all cases and seems not to be needed. The call to
     TerminateThread() can hang both gdb and the inferior. Happens when gdb is
     interrupted by PB and then a "kill" is issued. Also happens from the commandline
     though not very frequently.
   */
  for (thread = current_process.threads; thread; thread = thread->next)
    {
      CHECK (TerminateThread (thread->handle, 0));
    }
#endif
  CHECK (TerminateProcess (current_process.handle, 0));
  child_resume (current_process.id, 0, 0); /* wake up and die */
  child_wait (current_thread->id, &dummy);
  child_mourn_inferior ();
}

#ifndef FLAG_TRACE_BIT
#define FLAG_TRACE_BIT 0x100
#endif

void
child_resume (int pid, int step, enum target_signal signal)
{
  DbgThread *thread;

  DEBUG_EXEC (("gdb: child_resume (pid=%d, step=%d, signal=%d);\n", 
	       pid, step, signal));

  if (step)
    {
#ifdef __PPC__
      warning ("Single stepping not done.\n");
#endif
#ifdef i386
      /* Single step by setting t bit */
      child_fetch_inferior_registers (PS_REGNUM);
      current_thread->context.EFlags |= FLAG_TRACE_BIT;
      current_thread->context.ContextFlags = WRITE_TO_CHILD; /* will flush */
#endif
    }


  for (thread = current_process.threads; thread; thread = thread->next)
    {
      if (thread->context.ContextFlags == WRITE_TO_CHILD)
	{ /* regs cache has been altered by gdb: pass on to the child */
	  thread->context.ContextFlags = CONTEXT_DEBUGGER;
	  CHECK (SetThreadContext (thread->handle, &thread->context));
	}
      thread->context.ContextFlags = READ_FROM_CHILD;	/* mark regs invalid */
    }

  registers_changed ();

  if (signal)
    {
      fprintf_filtered (gdb_stderr, "Can't send signals to the child.\n");
    }

  if (current_process.haltedByCtrlC)
    ResumeThreads (&current_process);

  if (current_process.haltedByDebugEvent)
    {
      if (call_dummy_saved_thread.oldthread &&
          call_dummy_saved_thread.dummythread.suspended) {
        /* If we're using a call dummy, make sure it  gets started. */
        CHECK(ResumeThread(call_dummy_saved_thread.dummythread.handle)==1);
        call_dummy_saved_thread.dummythread.suspended = 0;
        }
      DEBUG_EVENTS (("gdb: ContinueDebugEvent (pid=%d, tid=%d, code=%d);\n",
		     current_process.id, 
		     current_process.eventThreadId,
		     signal ? current_process.continue_code : DBG_CONTINUE));
      CHECK (ContinueDebugEvent (current_process.id,
				 current_process.eventThreadId,
				 signal ? current_process.continue_code 
					: DBG_CONTINUE));
      current_process.haltedByDebugEvent = FALSE;
      current_process.continue_code = DBG_CONTINUE;
    }
}

static void
child_prepare_to_store ()
{
  /* Do nothing, since we can store individual regs */
}

static int
child_can_run ()
{
  return 1;
}

static void
child_close ()
{
  DEBUG_EVENTS (("gdb: child_close, inferior_pid=%d\n", inferior_pid));
}

struct target_ops child_ops =
{
  "child",			/* to_shortname */
  "Win32 child process",	/* to_longname */
  "Win32 child process (started by the \"run\" command).",	/* to_doc */
  child_open,			/* to_open */
  child_close,			/* to_close */
  child_attach,			/* to_attach */
  child_detach,			/* to_detach */
  child_resume,			/* to_resume */
  child_wait,			/* to_wait */
  child_fetch_inferior_registers,/* to_fetch_registers */
  child_store_inferior_registers,/* to_store_registers */
  child_prepare_to_store,	/* to_child_prepare_to_store */
  child_xfer_memory,		/* to_xfer_memory */
  child_files_info,		/* to_files_info */
  memory_insert_breakpoint,	/* to_insert_breakpoint */
  memory_remove_breakpoint,	/* to_remove_breakpoint */
  terminal_init_inferior,	/* to_terminal_init */
  terminal_inferior,		/* to_terminal_inferior */
  terminal_ours_for_output,	/* to_terminal_ours_for_output */
  terminal_ours,		/* to_terminal_ours */
  child_terminal_info,		/* to_terminal_info */
  child_kill_inferior,		/* to_kill */
  0,				/* to_load */
  0,				/* to_lookup_symbol */
  child_create_inferior,	/* to_create_inferior */
  child_mourn_inferior,		/* to_mourn_inferior */
  child_can_run,		/* to_can_run */
  0,				/* to_notice_signals */
  child_thread_alive,		/* to_thread_alive */
  0,				/* to_pid_to_str */
  child_stop,			/* to_stop */
  process_stratum,		/* to_stratum */
  0,				/* to_next */
  1,				/* to_has_all_memory */
  1,				/* to_has_memory */
  1,				/* to_has_stack */
  1,				/* to_has_registers */
  1,				/* to_has_execution */
  0,				/* to_sections */
  0,				/* to_sections_end */
  OPS_MAGIC			/* to_magic */
};

#include "environ.h"

static void
set_pathstyle_dos (args, from_tty, c)
     char *args;
     int from_tty;
     struct cmd_list_element *c;
{
  char **vector = environ_vector (inferior_environ);
  char *thisvar;
  int dos = *(int *) c->var;

  if (info_verbose)
    printf_filtered ("Change dos_path_style to %s\n", dos ? "true":"false");

  while (vector && *vector) 
    {
      if (dos)
	thisvar = unix_paths_to_dos_paths (*vector);
      else
	thisvar = dos_paths_to_unix_paths (*vector);

      if (thisvar) {
	if (info_verbose)
	  printf_filtered ("Change %s\nto %s\n", *vector, thisvar);
	free (*vector);
	*vector = xmalloc (strlen (thisvar) + 1);
	strcpy (*vector, thisvar);
      }
      vector++;
    }
}

#include <winuser.h>

char *enumlist[] = {
  "all", "info", "warn", "error", "none"
};

#ifdef NeXT_PDO
int RobinHoodHack = 1;
#endif

void
_initialize_inftarg ()
{
  struct cmd_list_element *c;

  bfd_set_cache_max_open (INT_MAX);

  SetDebugErrorLevel (0);

  add_show_from_set
    (add_set_cmd ("report-exception", class_support, var_zinteger, 
		  (char *) &exception_report_severity, 
		  "Set severity of WIN32 exceptions that will be reported:\n\t(0=all, 1=error|warn|info, 2=error|warn, 3=error, 4=none)\n", 
		       &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("signal-exception", class_support, var_zinteger, 
		       (char *) &exception_signal_severity, 
		       "Set severity of WIN32 exceptions that will cause a signal:\n\t(0=all, 1=error|warn|info, 2=error|warn, 3=error, 4=none)\n", 
		       &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("new-console", class_support, var_boolean,
		  (char *) &new_console,
		  "Set creation of new console when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("new-group", class_support, var_boolean,
		  (char *) &new_group,
		  "Set creation of new group when creating child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("pass-ctrlc", class_support, var_boolean,
		  (char *) &pass_ctrlc,
		  "Set whether gdb forwards Ctrl-C to the child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (c = add_set_cmd ("dos-path-style", class_support, var_boolean,
		      (char *) &dos_path_style,
        "Set whether paths in child's environment are shown in dos style.",
		      &setlist),
     &showlist);
  c->function.sfunc = set_pathstyle_dos;

  add_show_from_set
    (add_set_cmd ("debugexec", class_support, var_boolean,
		  (char *) &debug_exec,
		  "Set whether to display execution in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugevents", class_support, var_boolean,
		  (char *) &debug_events,
		  "Set whether to display kernel events in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugmemory", class_support, var_boolean,
		  (char *) &debug_memory,
		  "Set whether to display memory accesses in child process.",
		  &setlist),
     &showlist);

  add_show_from_set
    (add_set_cmd ("debugexceptions", class_support, var_boolean,
		  (char *) &debug_exceptions,
		  "Set whether to display kernel exceptions in child process.",
		  &setlist),
     &showlist);

#if 0 /* def NeXT_PDO */
  /* RobinHood Hack */
  add_show_from_set
    (add_set_cmd ("robinhood", class_support, var_boolean,
		  (char *)&RobinHoodHack,
		  "*Special symbol loading for Microsoft Linker", 
                  &setlist),
     &showlist);
  RobinHoodHack = 1;
#endif /* NeXT_PDO */
  add_target (&child_ops);
}

char *
global_gdbinit_dir ()
{
  char *next_root = getenv ("NEXT_ROOT");
  char *gdbinit_dir = "NextDeveloper/Libraries/gdb";
  char *ret;

  if (next_root == 0 || *next_root == 0)
    next_root = "/";

  ret = malloc (strlen (next_root) + strlen (gdbinit_dir) + 10);
  make_cleanup (free, ret);

  strcpy (ret, next_root);
  if (ret[strlen (ret) - 1] != '/')
    strcat (ret, "/");
  strcat (ret, gdbinit_dir);

  return ret;
}

/* Code for creating a new thread within the inferior.  We create a new
 * thread for the call dummy so that it can run without
 * interfering with the current thread if it is sitting in a system
 * call (such as WaitForMultipleObjects) which it is 90% of the time.
 */
void create_call_dummy_thread()
{
  if (isWin95()) return;		// no CreateRemoteThread() in win95

  /* Create the new thread if needed.  We have one extra thread that 
   * we create and we reuse it for all call dummy invocations.
   */
  if (!call_dummy_saved_thread.dummythread.handle) {
    call_dummy_saved_thread.dummythread.handle = CreateRemoteThread(
                                current_process.handle,
                                (LPSECURITY_ATTRIBUTES)NULL, 	// default security
                                4096,				// small stack
                                0,				// start pc (ignored)
                                0,				// start param (ignored)
                                CREATE_SUSPENDED,		// don't run just yet
                                &call_dummy_saved_thread.dummythread.id);
    CHECK(call_dummy_saved_thread.dummythread.handle);
    call_dummy_saved_thread.dummythread.suspended = 1;  // so we'll start it up
    }

  /* Use this to make sure registers in thread struct are up to date */
  child_fetch_inferior_registers(0);
  
  /* Save the old info */
  call_dummy_saved_thread.oldthread = current_thread;
  call_dummy_saved_thread.handle = current_thread->handle;
  call_dummy_saved_thread.id = current_thread->id;
  call_dummy_saved_thread.haltedByCtrlC = current_process.haltedByCtrlC;
      
  /* replace the current thread with the call dummy thread */
  current_thread->id = call_dummy_saved_thread.dummythread.id;
  current_thread->handle = call_dummy_saved_thread.dummythread.handle;

  /* Force the current thread's registers to get written to the
   * dummy thread when it continues.
   */
  current_thread->context.ContextFlags = WRITE_TO_CHILD;
  inferior_pid = current_thread->id;

  /* Bump the old thread's suspend count so it doesn't run when we continue */
  CHECK(SuspendThread(call_dummy_saved_thread.handle) != 0xffffffff);
}

void destroy_call_dummy_thread()
{
  DbgThread *original_thread = call_dummy_saved_thread.oldthread;
  value_ptr exit_thread;
  struct type *exit_type;
  CORE_ADDR exit_addr;

  if (isWin95()) return;

  /* Make sure the registers of the original thread
   * get read in from the call dummy thread
   */
  switch_context(original_thread->id);
  original_thread->context.ContextFlags = READ_FROM_CHILD;
  child_fetch_inferior_registers(-1);

  /* Restore original thread values */
  original_thread->handle = call_dummy_saved_thread.handle;
  original_thread->id = call_dummy_saved_thread.id;
  original_thread->context.ContextFlags = WRITE_TO_CHILD;
  inferior_pid = original_thread->id;
  switch_context(inferior_pid);

  /* Put the call dummy thread to sleep */
  CHECK(SuspendThread(call_dummy_saved_thread.dummythread.handle) != 0xffffffff);
  call_dummy_saved_thread.dummythread.suspended = 1;
  
  /* Resume the old thread */
  CHECK(ResumeThread(call_dummy_saved_thread.handle) != 0xffffffff);
  if (call_dummy_saved_thread.haltedByCtrlC)
    CHECK(ResumeThread(call_dummy_saved_thread.handle) != 0xffffffff);
  
  call_dummy_saved_thread.oldthread = 0;
}

/* Check for a reasonable stopping point for the backtraces.
 * The symbol for the entry point for has been cached away in
 * the DbgThread structure.  Check to see if it
 * is the same as the symbol for the frame's pc.
 */
int frame_chain_valid(CORE_ADDR chain, struct frame_info *thisframe)
{
  if ((!chain) || (!current_thread)) return 0;
  
  /* If we're in the same function as the entry point, quit. */
  if ((current_thread->startSym) &&
      (current_thread->startSym == lookup_minimal_symbol_by_pc(thisframe->pc)))
      return 0;
  return 1;
}
