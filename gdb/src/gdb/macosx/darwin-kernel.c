/* Mac OS X support for GDB, the GNU debugger.
   Copyright 2007
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>

#if TARGET_POWERPC
#include "ppc-macosx-thread-status.h"
#include "ppc-macosx-regs.h"
#include "ppc-macosx-regnums.h"
#endif

#if TARGET_I386
#include "i386-macosx-thread-status.h"
#include "i386-macosx-tdep.h"
#endif

#include "defs.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "event-loop.h"
#include "event-top.h"
#include "inf-loop.h"
#include "regcache.h"

#ifndef CPU_TYPE_I386
#define CPU_TYPE_I386 (7)
#endif

#ifndef CPU_TYPE_POWERPC
#define CPU_TYPE_POWERPC (18)
#endif

#define DARWIN_KERNEL_ID 0

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

extern int standard_is_async_p (void);
extern int standard_can_async_p (void);

static unsigned int darwin_kernel_debug_level = 1;

static char *darwin_kernel_default_host_type_str = "powerpc";
static int darwin_kernel_default_host_type = CPU_TYPE_POWERPC;

static int darwin_kernel_host_type = -1;

static int darwin_kernel_stopped = 0;
static int darwin_kernel_timeout = 5000;

static int darwin_kernel_fd = -1;
static unsigned long darwin_kernel_mapaddr = -1;

struct target_ops darwin_kernel_ops;

static void darwin_kernel_mourn_inferior ();

typedef enum
{
	DARWIN_KERNEL_LOG_ERROR = 1,
	DARWIN_KERNEL_LOG_WARNING = 2,
	DARWIN_KERNEL_LOG_INFO = 3,
	DARWIN_KERNEL_LOG_DEBUG = 4
} darwin_kernel_log_level;

static void
set_timeouts (char *args, int from_tty, struct cmd_list_element *cmd)
{

}

static int
parse_host_type (const char *host)
{
	if ((strcasecmp (host, "powerpc") == 0) || (strcasecmp (host, "ppc") == 0))
	{
#if TARGET_POWERPC
		return CPU_TYPE_POWERPC;
#else
		return -2;
#endif
	}
	else if ((strcasecmp (host, "ia32") == 0)
	    || (strcasecmp (host, "i386") == 0)
	    || (strcasecmp (host, "i486") == 0)
	    || (strcasecmp (host, "i586") == 0)
	    || (strcasecmp (host, "pentium") == 0))
	{
#if TARGET_I386
		return CPU_TYPE_I386;
#else
		return -2;
#endif
	}
	else
	{
		return -1;
	}
}

static void
darwin_kernel_logger (darwin_kernel_log_level l, char * call, const char *format, ...)
{
	va_list ap;

	if (l > darwin_kernel_debug_level)
	{
		return;
	}

	if (call)
		perror(call);

	va_start (ap, format);
	vfprintf (stderr, format, ap);
	va_end (ap);
}

static void
darwin_kernel_open (char *name, int from_tty)
{
	push_target (&darwin_kernel_ops);
}

static void
darwin_kernel_close (int quitting)
{

}

static void
darwin_kernel_attach (char *args, int from_tty)
{
	if ((darwin_kernel_fd = open("/dev/kmem", O_RDONLY)) < 0)
	{
		char errstr[128];
		if (errno == ENOENT)
			strcpy(errstr, "The /dev/kmem device doesn't exist; perhaps this kernel wasn't started with the kmem=1 kernel boot-arg.\n");
		else
			if (errno == EACCES)
				strcpy(errstr, "The /dev/kmem device is inaccessible; GDB must run as the superuser to access the kernel address space.\n");
		darwin_kernel_logger(DARWIN_KERNEL_LOG_ERROR, "open", errstr);
		return;
	}

	darwin_kernel_ops.to_has_all_memory = 1;
	darwin_kernel_ops.to_has_memory = 1;
	darwin_kernel_ops.to_has_stack = 0;
	darwin_kernel_ops.to_has_registers = 0;
	darwin_kernel_ops.to_has_execution = 0;

	update_current_target ();

	inferior_ptid = pid_to_ptid (DARWIN_KERNEL_ID);
	darwin_kernel_stopped = 1;

	printf_unfiltered ("Connected.\n");
}

static void
darwin_kernel_detach (char *args, int from_tty)
{
	close(darwin_kernel_fd);

	darwin_kernel_ops.to_has_all_memory = 0;
	darwin_kernel_ops.to_has_memory = 0;
	darwin_kernel_ops.to_has_stack = 0;
	darwin_kernel_ops.to_has_registers = 0;
	darwin_kernel_ops.to_has_execution = 0;

	update_current_target ();

	darwin_kernel_mourn_inferior ();

	printf_unfiltered ("Disconnected.\n");
}

static int
darwin_kernel_xfer_memory (CORE_ADDR memaddr, gdb_byte *myaddr, int len, 
                          int write, struct mem_attrib *attrib, 
                          struct target_ops *target)
{
	int ret = 0;
	if (write)
	{
		darwin_kernel_logger(DARWIN_KERNEL_LOG_ERROR, "", "Writes not supported\n");
		return 0;
	}
	else
	{
		if ((ret = pread(darwin_kernel_fd, myaddr, len, (off_t) memaddr)) < 0)
			darwin_kernel_logger(DARWIN_KERNEL_LOG_WARNING, "pread", "Read Failed\n");
		else
			if (ret != len)
				darwin_kernel_logger(DARWIN_KERNEL_LOG_WARNING,"","Read %d bytes, requested %d\n",ret, len);
	}

	return len;
}

static int
darwin_kernel_insert_breakpoint (CORE_ADDR addr, gdb_byte *contents_cache)
{
	darwin_kernel_logger(DARWIN_KERNEL_LOG_ERROR, "", "Cannot set breakpoints for live kernel\n");
	return (-1);
}

static int
darwin_kernel_remove_breakpoint (CORE_ADDR addr, gdb_byte *contents_cache)
{
	darwin_kernel_logger(DARWIN_KERNEL_LOG_ERROR, "", "Cannot remove breakpoints for live kernel\n");
	return (-1);
}

static void
darwin_kernel_files_info (struct target_ops *ops)
{
	printf_unfiltered ("\tNo connection information available.\n");
}

static void
darwin_kernel_kill (void)
{
	darwin_kernel_detach ("", 0);
}

static void
darwin_kernel_load (char *args, int from_tty)
{
	error ("Unsupported operation darwin_kernel_load\n");
}

static void
darwin_kernel_create_inferior (char *execfile, char *args, char **env, 
                               int fromtty)
{
	error ("Unsupported operation darwin_kernel_create_inferior\n");
}

static void
darwin_kernel_mourn_inferior ()
{
	unpush_target (&darwin_kernel_ops);
	generic_mourn_inferior ();
}

static int remote_async_terminal_ours_p = 1;
static void (*ofunc) (int);

static void
darwin_kernel_terminal_inferior (void)
{
	/* terminal_inferior (); */

	if (!sync_execution)
		return;
	if (!remote_async_terminal_ours_p)
		return;
	CHECK_FATAL (sync_execution);
	CHECK_FATAL (remote_async_terminal_ours_p);
	delete_file_handler (input_fd);
	remote_async_terminal_ours_p = 0;

}

static void
darwin_kernel_terminal_ours (void)
{
	/* terminal_ours (); */

	if (!sync_execution)
		return;
	if (remote_async_terminal_ours_p)
		return;
	CHECK_FATAL (sync_execution);
	CHECK_FATAL (!remote_async_terminal_ours_p);

	add_file_handler (input_fd, stdin_event_handler, 0);

	remote_async_terminal_ours_p = 1;
}

static void (*async_client_callback) (enum inferior_event_type event_type,
    void *context);
static void *async_client_context;

static void
darwin_kernel_file_handler (int error, gdb_client_data client_data)
{
	async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
darwin_kernel_file_handler_callback (void *arg)
{
	async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
darwin_kernel_async (void (*callback) (enum inferior_event_type event_type,
	void *context), void *context)
{
	if (current_target.to_async_mask_value == 0)
		internal_error (__FILE__, __LINE__,
                    "Calling remote_async when async is masked");

	if (callback != NULL)
	{
		async_client_callback = callback;
		async_client_context = context;
	}
	else
	{

	}

}

static void
init_darwin_kernel_ops (void)
{
	darwin_kernel_ops.to_shortname = "darwin-kernel";
	darwin_kernel_ops.to_longname = "Debug the active Darwin Kernel";
	darwin_kernel_ops.to_doc = "Debug a running Darwin kernel; use 'attach' to begin, after issuing the 'target darwin-kernel' command";
	darwin_kernel_ops.to_open = darwin_kernel_open;
	darwin_kernel_ops.to_close = darwin_kernel_close;
	darwin_kernel_ops.to_attach = darwin_kernel_attach;
	darwin_kernel_ops.deprecated_xfer_memory = darwin_kernel_xfer_memory;

	darwin_kernel_ops.to_insert_breakpoint = darwin_kernel_insert_breakpoint;
	darwin_kernel_ops.to_remove_breakpoint = darwin_kernel_remove_breakpoint;

	darwin_kernel_ops.to_files_info = darwin_kernel_files_info;
	darwin_kernel_ops.to_detach = darwin_kernel_detach;
	darwin_kernel_ops.to_kill = darwin_kernel_kill;
	darwin_kernel_ops.to_load = darwin_kernel_load;
	darwin_kernel_ops.to_create_inferior = darwin_kernel_create_inferior;
	darwin_kernel_ops.to_mourn_inferior = darwin_kernel_mourn_inferior;
	darwin_kernel_ops.to_stratum = process_stratum;
	darwin_kernel_ops.to_can_async_p = standard_can_async_p;
	darwin_kernel_ops.to_is_async_p = standard_is_async_p;
	darwin_kernel_ops.to_terminal_inferior = darwin_kernel_terminal_inferior;
	darwin_kernel_ops.to_terminal_ours = darwin_kernel_terminal_ours;
	darwin_kernel_ops.to_async = darwin_kernel_async;
	darwin_kernel_ops.to_async_mask_value = 1;
	darwin_kernel_ops.to_magic = OPS_MAGIC;
	darwin_kernel_ops.to_has_all_memory = 0;
	darwin_kernel_ops.to_has_memory = 0;
	darwin_kernel_ops.to_has_stack = 0;
	darwin_kernel_ops.to_has_registers = 0;
	darwin_kernel_ops.to_has_execution = 0;
}

void
_initialize_remote_darwin_kernel (void)
{
	init_darwin_kernel_ops ();
	add_target (&darwin_kernel_ops);
	add_setshow_uinteger_cmd
	    ("darwin_kernel-debug-level", class_obscure, &darwin_kernel_debug_level, _("\
Set level of verbosity for Darwin Kernel debugging information."), _("\
Show level of verbosity for Darwin Kernel debugging information."), _("\
No additional help."),
	    NULL,
	    NULL,
	    &setlist, &showlist);
}
