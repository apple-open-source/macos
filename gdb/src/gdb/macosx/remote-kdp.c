/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
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

#if TARGET_I386
#define KDP_TARGET_I386 1
#else
#undef KDP_TARGET_I386
#endif

#if TARGET_POWERPC
#define KDP_TARGET_POWERPC 1
#else
#undef KDP_TARGET_POWERPC
#endif

#if KDP_TARGET_POWERPC
#include "ppc-macosx-thread-status.h"
#include "ppc-macosx-regs.h"
#include "ppc-macosx-regnums.h"
#endif

#if KDP_TARGET_I386
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

#include "kdp-udp.h"
#include "kdp-transactions.h"

#ifndef CPU_TYPE_I386
#define CPU_TYPE_I386 (7)
#endif

#ifndef CPU_TYPE_POWERPC
#define CPU_TYPE_POWERPC (18)
#endif

#ifndef KDP_REMOTE_ID
#define KDP_REMOTE_ID 3
#endif

#define KDP_MAX_BREAKPOINTS 100

#include <stdarg.h>
#include <string.h>
#include <ctype.h>

extern int standard_is_async_p (void);
extern int standard_can_async_p (void);

static unsigned int kdp_debug_level = 3;
static unsigned int kdp_default_port = 41139;

static char *kdp_default_host_type_str = "powerpc";
static int kdp_default_host_type = CPU_TYPE_POWERPC;

static kdp_connection c;
static int kdp_host_type = -1;

static int kdp_stopped = 0;
static int kdp_timeout = 5000;
static int kdp_retries = 10;
static int remote_kdp_version = 0;
static int remote_kdp_feature = 0;

struct target_ops kdp_ops;

static void kdp_mourn_inferior ();

static void
set_timeouts (char *args, int from_tty, struct cmd_list_element *cmd)
{
  kdp_set_timeouts (&c, kdp_timeout, kdp_retries);
}

static int
parse_host_type (const char *host)
{
  if ((strcasecmp (host, "powerpc") == 0) || (strcasecmp (host, "ppc") == 0))
    {
#if KDP_TARGET_POWERPC
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
#if KDP_TARGET_I386
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
logger (kdp_log_level l, const char *format, ...)
{
  va_list ap;

  if (l > kdp_debug_level)
    {
      return;
    }

  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}

static void
kdp_open (char *name, int from_tty)
{
  push_target (&kdp_ops);
}

static void
kdp_close (int quitting)
{
}

static int
convert_host_type (unsigned int mach_type)
{
  switch (mach_type)
    {
    case CPU_TYPE_POWERPC:
      return bfd_arch_powerpc;
    case CPU_TYPE_I386:
      return bfd_arch_i386;
    default:
      return -1;
    }
}

static int
kdp_insert_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  kdp_return_t kdpret;

  if (!kdp_is_connected (&c))
    {
      logger (KDP_LOG_DEBUG,
              "kdp_insert_breakpoint: unable to set breakpoint - (not connected)");
      return -1;
    }

  c.request->breakpoint_req.hdr.request = KDP_BREAKPOINT_SET;
  c.request->breakpoint_req.address = addr;
  kdpret =
    kdp_transaction (&c, c.request, c.response, "kdp_insert_breakpoint");
  logger (KDP_LOG_DEBUG,
          "kdp_insert_breakpoint: kdp_transaction returned %d\n", kdpret);
  if (c.response->breakpoint_reply.error != RR_SUCCESS)
    {
      /* We've reached the maximum number of breakpoints the kernel can support
         so revert to the old model of directly writing to memory */
      if (c.response->breakpoint_reply.error == KDP_MAX_BREAKPOINTS)
        {
          kdp_ops.to_insert_breakpoint = memory_insert_breakpoint;
          kdp_ops.to_remove_breakpoint = memory_remove_breakpoint;
          printf_unfiltered ("Max number of kernel breakpoints reached,"
                             "Reverting to memory_insert_breakpoint.\n");
          memory_insert_breakpoint (addr, contents_cache);
          return 0;
        }
      kdpret = c.response->breakpoint_reply.error;
      logger (KDP_LOG_DEBUG,
              "kdp_insert_breakpoint: response contained error code %d\n",
              kdpret);
      return -1;
    }
  return 0;
}

static int
kdp_remove_breakpoint (CORE_ADDR addr, char *contents_cache)
{
  kdp_return_t kdpret;

  if (!kdp_is_connected (&c))
    {
      logger (KDP_LOG_DEBUG,
              "kdp_remove_breakpoint: unable to remove breakpoint - (not connected)");
      return -1;
    }

  c.request->breakpoint_req.hdr.request = KDP_BREAKPOINT_REMOVE;
  c.request->breakpoint_req.address = addr;
  kdpret =
    kdp_transaction (&c, c.request, c.response, "kdp_remove_breakpoint");
  logger (KDP_LOG_DEBUG,
          "kdp_remove_breakpoint: kdp_transaction returned %d\n", kdpret);
  if (c.response->breakpoint_reply.error != RR_SUCCESS)
    {
      kdpret = c.response->breakpoint_reply.error;
      logger (KDP_LOG_DEBUG,
              "kdp_remove_breakpoint: response contained error code %d\n",
              kdpret);
      return -1;
    }
  return 0;
}

static void
kdp_attach (char *args, int from_tty)
{
  kdp_return_t kdpret, kdpret2;
  unsigned int old_seqno, old_exc_seqno;

  if (args == NULL)
    {
      args = "";
    }

  {
    char *s = args;
    while ((*s != '\0') && isspace (*s))
      {
        s++;
      }
    if (*s == '\0')
      {
        error ("usage: attach <hostname>");
      }
    while ((*s != '\0') && !isspace (*s))
      {
        s++;
      }
    while ((*s != '\0') && isspace (*s))
      {
        s++;
      }
    if (*s != '\0')
      {
        error ("usage: attach <hostname>");
      }
  }

  if (kdp_is_connected (&c))
    {
      kdpret = kdp_disconnect (&c);
      if (kdpret != RR_SUCCESS)
        {
          error ("unable to disconnect from host: %s",
                 kdp_return_string (kdpret));
        }
    }

  if (kdp_is_bound (&c))
    {
      kdpret = kdp_destroy (&c);
      if (kdpret != RR_SUCCESS)
        {
          error ("unable to deallocate KDP connection: %s",
                 kdp_return_string (kdpret));
        }
    }

  old_seqno = c.seqno;
  old_exc_seqno = c.exc_seqno;

  kdpret =
    kdp_create (&c, logger, args, kdp_default_port, kdp_timeout, kdp_retries);
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to create connection for host \"%s\": %s", args,
             kdp_return_string (kdpret));
    }

  c.seqno = old_seqno;
  c.exc_seqno = old_exc_seqno;

#if TARGET_POWERPC
  kdp_set_little_endian (&c);
#elif TARGET_I386
  kdp_set_big_endian (&c);
#else
#error "unsupported architecture"
#endif

  kdpret = kdp_connect (&c);
  if (kdpret != RR_SUCCESS)
    {
      kdpret2 = kdp_destroy (&c);
      if (kdpret2 != RR_SUCCESS)
        {
          warning
            ("unable to destroy host connection after error connecting: %s",
             kdp_return_string (kdpret2));
        }
      error ("unable to connect to host \"%s\": %s", args,
             kdp_return_string (kdpret));
    }

  {
    c.request->readregs_req.hdr.request = KDP_HOSTINFO;

    kdpret = kdp_transaction (&c, c.request, c.response, "kdp_attach");
    if (kdpret != RR_SUCCESS)
      {
        kdpret2 = kdp_disconnect (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to disconnect from host after error determining cpu type: %s",
               kdp_return_string (kdpret2));
          }
        kdpret2 = kdp_destroy (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to destroy host connection after error determining cpu type: %s",
               kdp_return_string (kdpret2));
          }
        error ("kdp_attach: unable to determine host type: %s",
               kdp_return_string (kdpret));
      }

    kdp_host_type = convert_host_type (c.response->hostinfo_reply.cpu_type);


    if (kdp_host_type == -1)
      {
        warning
          ("kdp_attach: unknown host type 0x%lx; trying default (0x%lx)\n",
           (unsigned long) c.response->hostinfo_reply.cpu_type,
           (unsigned long) kdp_default_host_type);
        kdp_host_type = convert_host_type (kdp_default_host_type);
      }

    c.request->readregs_req.hdr.request = KDP_VERSION;

    kdpret = kdp_transaction (&c, c.request, c.response, "kdp_attach");
    if (kdpret != RR_SUCCESS)
      {
        kdpret2 = kdp_disconnect (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to disconnect from host after error determining protocol version: %s",
               kdp_return_string (kdpret2));
          }
        kdpret2 = kdp_destroy (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to destroy host connection after error determining protocol version: %s",
               kdp_return_string (kdpret2));
          }
        error ("kdp_attach: unable to determine protocol version: %s",
               kdp_return_string (kdpret));
      }

    remote_kdp_version = c.response->version_reply.version;
    remote_kdp_feature = c.response->version_reply.feature;

    if (kdp_host_type == -1)
      {
        kdpret2 = kdp_disconnect (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to disconnect from host after error determining cpu type: %s",
               kdp_return_string (kdpret2));
          }
        kdpret2 = kdp_destroy (&c);
        if (kdpret2 != RR_SUCCESS)
          {
            warning
              ("unable to destroy host connection after error determining cpu type: %s",
               kdp_return_string (kdpret2));
          }
        error ("kdp_attach: unknown host type");
      }
  }

  /* Use breakpoint packets only if the kernel supports them */
  if ((remote_kdp_version >= 10) && (remote_kdp_feature & KDP_FEATURE_BP))
    {
      kdp_ops.to_insert_breakpoint = kdp_insert_breakpoint;
      kdp_ops.to_remove_breakpoint = kdp_remove_breakpoint;
    }
  kdp_ops.to_has_all_memory = 1;
  kdp_ops.to_has_memory = 1;
  kdp_ops.to_has_stack = 1;
  kdp_ops.to_has_registers = 1;
  kdp_ops.to_has_execution = 1;

  update_current_target ();

  inferior_ptid = pid_to_ptid (KDP_REMOTE_ID);
  kdp_stopped = 1;

  printf_unfiltered ("Connected.\n");
}

static void
kdp_detach (char *args, int from_tty)
{
  kdp_return_t kdpret;

  if (kdp_is_connected (&c))
    {
      kdpret = kdp_disconnect (&c);
      if (kdpret != RR_SUCCESS)
        {
          warning ("unable to disconnect from host: %s",
                   kdp_return_string (kdpret));
        }
    }

  kdp_ops.to_has_all_memory = 0;
  kdp_ops.to_has_memory = 0;
  kdp_ops.to_has_stack = 0;
  kdp_ops.to_has_registers = 0;
  kdp_ops.to_has_execution = 0;

  update_current_target ();

  if (kdp_is_bound (&c))
    {
      kdpret = kdp_destroy (&c);
      if (kdpret != RR_SUCCESS)
        {
          error ("unable to deallocate KDP connection: %s",
                 kdp_return_string (kdpret));
        }
    }
  kdp_mourn_inferior ();

  printf_unfiltered ("Disconnected.\n");
}

static void
kdp_reattach_command (char *args, int from_tty)
{
  kdp_return_t kdpret;
  char **argv;
  char *host;

  argv = buildargv (args);

  if ((argv == NULL) || (argv[0] == NULL) || (argv[1] != NULL))
    error ("usage: kdp-reattach <hostname>");

  host = argv[0];

  kdp_open (NULL, 0);

  kdp_reset (&c);

#if TARGET_POWERPC
  kdp_set_little_endian (&c);
#elif TARGET_I386
  kdp_set_big_endian (&c);
#else
#error "unsupported architecture"
#endif

  kdpret =
    kdp_create (&c, logger, argv[0], kdp_default_port, kdp_timeout,
                kdp_retries);
  if (kdpret != RR_SUCCESS)
    error ("unable to create connection for host \"%s\": %s", args,
           kdp_return_string (kdpret));

  kdpret = kdp_reattach (&c);

  if (kdpret != RR_SUCCESS)
    warning ("unable to disconnect host: %s", kdp_return_string (kdpret));
  if (kdp_is_bound (&c))
    {
      kdpret = kdp_destroy (&c);
      if (kdpret != RR_SUCCESS)
        error ("unable to deallocate KDP connection: %s",
               kdp_return_string (kdpret));
    }

  kdp_ops.to_has_all_memory = 0;
  kdp_ops.to_has_memory = 0;
  kdp_ops.to_has_stack = 0;
  kdp_ops.to_has_registers = 0;
  kdp_ops.to_has_execution = 0;

  update_current_target ();

  kdp_mourn_inferior ();

  kdp_open (NULL, 0);

  kdp_attach (host, 0);
}

static void
kdp_reboot_command (char *args, int from_tty)
{
  kdp_return_t kdpret;
  char **argv;
  char *host;

  argv = buildargv (args);

  if ((argv == NULL) || (argv[0] == NULL) || (argv[1] != NULL))
    error ("usage: kdp-reboot <hostname>");

  host = argv[0];

  kdp_open (NULL, 0);

  kdp_reset (&c);

#if TARGET_POWERPC
  kdp_set_little_endian (&c);
#elif TARGET_I386
  kdp_set_big_endian (&c);
#else
#error "unsupported architecture"
#endif

  kdpret =
    kdp_create (&c, logger, argv[0], kdp_default_port, kdp_timeout,
                kdp_retries);
  if (kdpret != RR_SUCCESS)
    error ("unable to create connection for host \"%s\": %s", args,
           kdp_return_string (kdpret));

  kdpret = kdp_hostreboot (&c);

  if (kdpret != RR_SUCCESS)
    warning ("unable to disconnect host: %s", kdp_return_string (kdpret));
  if (kdp_is_bound (&c))
    {
      kdpret = kdp_destroy (&c);
      if (kdpret != RR_SUCCESS)
        error ("unable to deallocate KDP connection: %s",
               kdp_return_string (kdpret));
    }

  kdp_ops.to_has_all_memory = 0;
  kdp_ops.to_has_memory = 0;
  kdp_ops.to_has_stack = 0;
  kdp_ops.to_has_registers = 0;
  kdp_ops.to_has_execution = 0;

  update_current_target ();

  kdp_mourn_inferior ();
}

static void
kdp_detach_command (char *args, int from_tty)
{
  kdp_connection c2;
  kdp_return_t kdpret;

  char **argv;
  char *host;
  unsigned int seqno;

  argv = buildargv (args);

  if ((argv == NULL) || (argv[0] == NULL) || (argv[1] == NULL)
      || (argv[2] != NULL))
    {
      error ("usage: kdp-detach <hostname> <seqno>");
    }
  host = argv[0];
  seqno = atoi (argv[1]);

  kdp_reset (&c2);

  kdpret =
    kdp_create (&c2, logger, argv[0], kdp_default_port, kdp_timeout,
                kdp_retries);
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to create connection for host \"%s\": %s", args,
             kdp_return_string (kdpret));
    }

  c2.request->disconnect_req.hdr.request = KDP_DISCONNECT;
  c2.request->hdr.key = 0;
  c2.request->hdr.is_reply = 0;
  c2.request->hdr.seq = seqno;

  kdp_set_big_endian (&c2);
  kdpret = kdp_transmit_debug (&c2, c2.request);
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to send reset request: %s", kdp_return_string (kdpret));
    }

  kdp_set_little_endian (&c2);
  kdpret = kdp_transmit_debug (&c2, c2.request);
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to send reset request: %s", kdp_return_string (kdpret));
    }

  kdpret = kdp_destroy (&c2);
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to destroy connection: %s", kdp_return_string (kdpret));
    }
}

static void
kdp_set_trace_bit (int step)
{
  switch (kdp_host_type)
    {

    case bfd_arch_powerpc:
      {
#if KDP_TARGET_POWERPC
        LONGEST srr1 = read_register (PS_REGNUM);
        if (step)
          {
            srr1 |= 0x400UL;
          }
        else
          {
            srr1 &= ~0x400UL;
          }
        write_register (PS_REGNUM, srr1);
#else
        error ("kdp_set_trace_bit: not configured to support powerpc");
#endif
      }
      break;

    case bfd_arch_i386:
      {
#ifdef KDP_TARGET_I386
        LONGEST eflags = read_register (PS_REGNUM);
        if (step)
          {
            eflags |= 0x100UL;
          }
        else
          {
            eflags &= ~0x100UL;
          }
        write_register (PS_REGNUM, eflags);
#else
        error ("kdp_set_trace_bit: not configured to support i386");
#endif
      }
      break;

    default:
      error ("kdp_set_trace_bit: unknown host type 0x%lx",
             (unsigned long) kdp_host_type);
    }
}

static void
kdp_resume (ptid_t pid, int step, enum target_signal sig)
{
  kdp_return_t kdpret;

  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to resume (not connected)");
    }

  if (step)
    {
      kdp_set_trace_bit (1);
    }
  else
    {
      kdp_set_trace_bit (0);
    }

  c.request->resumecpus_req.hdr.request = KDP_RESUMECPUS;

  if (proceed_to_finish || (step_over_calls != STEP_OVER_UNDEBUGGABLE))
    c.request->resumecpus_req.cpu_mask = 1L;
  else
    c.request->resumecpus_req.cpu_mask = ~0L;

  kdpret = kdp_transaction (&c, c.request, c.response, "kdp_resume");
  if (kdpret != RR_SUCCESS)
    {
      error ("unable to resume processing on host: %s",
             kdp_return_string (kdpret));
    }

  kdp_stopped = 0;

  if (event_loop_p && target_can_async_p ())
    target_async (inferior_event_handler, 0);

  if (target_is_async_p ())
    target_executing = 1;
}

static ptid_t
kdp_wait (ptid_t pid,
          struct target_waitstatus *status, gdb_client_data client_data)
{
  kdp_return_t kdpret;

  if (PIDGET (pid) == -1)
    {
      pid = pid_to_ptid (KDP_REMOTE_ID);
    }
  if (PIDGET (pid) != KDP_REMOTE_ID)
    {
      error ("kdp: unable to switch to process-id %s",
             target_pid_to_str (pid));
    }

  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to wait for activity (not connected)");
    }

  if (kdp_stopped)
    {
      status->kind = TARGET_WAITKIND_STOPPED;
      status->value.sig = TARGET_SIGNAL_TRAP;
      return pid;
    }

  kdpret = kdp_exception_wait (&c, c.response, 0);

  if (kdpret != RR_SUCCESS)
    {
      error ("unable to wait for result from host: %s\n",
             kdp_return_string (kdpret));
    }

  kdp_set_trace_bit (0);

  kdp_stopped = 1;
  select_frame (get_current_frame ());

  status->kind = TARGET_WAITKIND_STOPPED;
  status->value.sig = TARGET_SIGNAL_TRAP;

  return pid;
}

#if KDP_TARGET_POWERPC
static void
kdp_fetch_registers_ppc (int regno)
{
  unsigned int i;

  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to fetch registers (not connected)");
    }

  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {
      kdp_return_t kdpret;
      gdb_ppc_thread_state_t gp_regs;

      c.request->readregs_req.hdr.request = KDP_READREGS;
      c.request->readregs_req.cpu = 0;
      c.request->readregs_req.flavor = GDB_PPC_THREAD_STATE;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_fetch_registers_ppc");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_fetch_registers_ppc: unable to fetch PPC_THREAD_STATE: %s",
             kdp_return_string (kdpret));
        }
      if (c.response->readregs_reply.nbytes !=
          (GDB_PPC_THREAD_STATE_COUNT * 4))
        {
          error
            ("kdp_fetch_registers_ppc: kdp returned %lu bytes of register data (expected %lu)",
             c.response->readregs_reply.nbytes,
             (GDB_PPC_THREAD_STATE_COUNT * 4));
        }

      memcpy (&gp_regs, c.response->readregs_reply.data,
              (GDB_PPC_THREAD_STATE_COUNT * 4));
      ppc_macosx_fetch_gp_registers (&gp_regs);
    }

#if 0
  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      kdp_return_t kdpret;
      gdb_ppc_thread_fpstate_t fp_regs;

      c.request->readregs_req.hdr.request = KDP_READREGS;
      c.request->readregs_req.cpu = 0;
      c.request->readregs_req.flavor = GDB_PPC_THREAD_FPSTATE;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_fetch_registers_ppc");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_fetch_registers_ppc: unable to fetch PPC_THREAD_FPSTATE: %s",
             kdp_return_string (kdpret));
        }
      if (c.response->readregs_reply.nbytes !=
          (GDB_PPC_THREAD_FPSTATE_COUNT * 4))
        {
          error
            ("kdp_fetch_registers_ppc: kdp returned %lu bytes of register data (expected %lu)",
             c.response->readregs_reply.nbytes,
             (GDB_PPC_THREAD_FPSTATE_COUNT * 4));
        }

      memcpy (&fp_regs, c.response->readregs_reply.data,
              (GDB_PPC_THREAD_FPSTATE_COUNT * 4));
      ppc_macosx_fetch_fp_registers (&fp_regs);
    }
#else
  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {
      /* Accesses to the fp registers aren't currently supported in
         the kernel. */
      for (i = PPC_MACOSX_FIRST_FP_REGNUM; i <= PPC_MACOSX_LAST_FP_REGNUM;
           i++)
        deprecated_register_valid[i] = 1;
      for (i = PPC_MACOSX_FIRST_FSP_REGNUM; i <= PPC_MACOSX_LAST_FSP_REGNUM;
           i++)
        deprecated_register_valid[i] = 1;
    }
#endif

  if ((regno == -1) || (regno >= PPC_MACOSX_FIRST_VP_REGNUM)
      || PPC_MACOSX_IS_VSP_REGNUM (regno))
    {
      /* Accesses to the vector, fpscr and vrsave registers aren't currently
         supported in the kernel. */
      for (i = PPC_MACOSX_FIRST_VP_REGNUM; i <= PPC_MACOSX_LAST_VP_REGNUM;
           i++)
        deprecated_register_valid[i] = 1;
      for (i = PPC_MACOSX_FIRST_VSP_REGNUM; i <= PPC_MACOSX_LAST_VSP_REGNUM;
           i++)
        deprecated_register_valid[i] = 1;
    }
}
#endif /* KDP_TARGET_POWERPC */

#if KDP_TARGET_POWERPC
static void
kdp_store_registers_ppc (int regno)
{
  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to store registers (not connected)");
    }

  if ((regno == -1) || PPC_MACOSX_IS_GP_REGNUM (regno)
      || PPC_MACOSX_IS_GSP_REGNUM (regno))
    {

      gdb_ppc_thread_state_t gp_regs;
      kdp_return_t kdpret;

      ppc_macosx_store_gp_registers (&gp_regs);

      memcpy (c.request->writeregs_req.data, &gp_regs,
              (GDB_PPC_THREAD_STATE_COUNT * 4));

      c.request->writeregs_req.hdr.request = KDP_WRITEREGS;
      c.request->writeregs_req.cpu = 0;
      c.request->writeregs_req.flavor = GDB_PPC_THREAD_STATE;
      c.request->writeregs_req.nbytes = GDB_PPC_THREAD_STATE_COUNT * 4;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_store_registers_ppc");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_store_registers_ppc: unable to store PPC_THREAD_STATE: %s",
             kdp_return_string (kdpret));
        }
    }

  if ((regno == -1) || PPC_MACOSX_IS_FP_REGNUM (regno)
      || PPC_MACOSX_IS_FSP_REGNUM (regno))
    {

      gdb_ppc_thread_fpstate_t fp_regs;
      kdp_return_t kdpret;

      ppc_macosx_store_fp_registers (&fp_regs);

      memcpy (c.response->readregs_reply.data, &fp_regs,
              (GDB_PPC_THREAD_FPSTATE_COUNT * 4));

      c.request->writeregs_req.hdr.request = KDP_WRITEREGS;
      c.request->writeregs_req.cpu = 0;
      c.request->writeregs_req.flavor = GDB_PPC_THREAD_FPSTATE;
      c.request->writeregs_req.nbytes = GDB_PPC_THREAD_FPSTATE_COUNT * 4;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_store_registers_ppc");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_store_registers_ppc: unable to store PPC_THREAD_FPSTATE: %s",
             kdp_return_string (kdpret));
        }
    }
}
#endif /* KDP_TARGET_POWERPC */

#if KDP_TARGET_I386
static void
kdp_fetch_registers_i386 (int regno)
{
  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to fetch registers (not connected)");
    }

  if ((regno == -1) || IS_GP_REGNUM (regno))
    {
      kdp_return_t kdpret;
      gdb_i386_thread_state_t gp_regs;

      c.request->readregs_req.hdr.request = KDP_READREGS;
      c.request->readregs_req.cpu = 0;
      c.request->readregs_req.flavor = GDB_i386_THREAD_STATE;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_fetch_registers_i386");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_fetch_registers_i386: unable to fetch i386_THREAD_STATE: %s",
             kdp_return_string (kdpret));
        }
      if (c.response->readregs_reply.nbytes !=
          (GDB_i386_THREAD_STATE_COUNT * 4))
        {
          error
            ("kdp_fetch_registers_i386: kdp returned %lu bytes of register data (expected %lu)",
             c.response->readregs_reply.nbytes,
             (GDB_i386_THREAD_STATE_COUNT * 4));
        }

      memcpy (&gp_regs, c.response->readregs_reply.data,
              (GDB_i386_THREAD_STATE_COUNT * 4));
      i386_macosx_fetch_gp_registers (&gp_regs);
    }

  if ((regno == -1) || IS_FP_REGNUM (regno))
    {
      kdp_return_t kdpret;
      gdb_i386_thread_fpstate_t fp_regs = { };

      /* FIXME: For now we hang the kdp stub asking for FP registers,
         so till the kernel can handle the request, don't send it.  */
#if 0
      c.request->readregs_req.hdr.request = KDP_READREGS;
      c.request->readregs_req.cpu = 0;
      c.request->readregs_req.flavor = GDB_i386_THREAD_FPSTATE;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_fetch_registers_i386");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_fetch_registers_i386: unable to fetch GDB_i386_THREAD_FPSTATE: %s",
             kdp_return_string (kdpret));
        }
      if (c.response->readregs_reply.nbytes !=
          (GDB_i386_THREAD_FPSTATE_COUNT * 4))
        {
          error
            ("kdp_fetch_registers_i386: kdp returned %lu bytes of register data (expected %lu)",
             c.response->readregs_reply.nbytes,
             (GDB_i386_THREAD_FPSTATE_COUNT * 4));
        }

      memcpy (&fp_regs.hw_state, c.response->readregs_reply.data,
              (GDB_i386_THREAD_FPSTATE_COUNT * 4));
#endif
      i386_macosx_fetch_fp_registers (&fp_regs);
    }
}
#endif /* KDP_TARGET_I386 */

#if KDP_TARGET_I386
static void
kdp_store_registers_i386 (int regno)
{
  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to store registers (not connected)");
    }

  if ((regno == -1) || IS_GP_REGNUM (regno))
    {

      gdb_i386_thread_state_t gp_regs;
      kdp_return_t kdpret;

      i386_macosx_store_gp_registers (&gp_regs);

      memcpy (c.request->writeregs_req.data, &gp_regs,
              (GDB_i386_THREAD_STATE_COUNT * 4));

      c.request->writeregs_req.hdr.request = KDP_WRITEREGS;
      c.request->writeregs_req.cpu = 0;
      c.request->writeregs_req.flavor = GDB_i386_THREAD_STATE;
      c.request->writeregs_req.nbytes = GDB_i386_THREAD_STATE_COUNT * 4;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_store_registers_i386");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_store_registers_i386: unable to store i386_THREAD_STATE: %s",
             kdp_return_string (kdpret));
        }
    }

  if ((regno == -1) || IS_FP_REGNUM (regno))
    {

      gdb_i386_thread_fpstate_t fp_regs;
      kdp_return_t kdpret;

      i386_macosx_store_fp_registers (&fp_regs);

      memcpy (c.response->readregs_reply.data, &fp_regs.hw_state,
              (GDB_i386_THREAD_FPSTATE_COUNT * 4));

      c.request->writeregs_req.hdr.request = KDP_WRITEREGS;
      c.request->writeregs_req.cpu = 0;
      c.request->writeregs_req.flavor = GDB_i386_THREAD_FPSTATE;
      c.request->writeregs_req.nbytes = GDB_i386_THREAD_FPSTATE_COUNT * 4;

      kdpret =
        kdp_transaction (&c, c.request, c.response,
                         "kdp_store_registers_i386");
      if (kdpret != RR_SUCCESS)
        {
          error
            ("kdp_store_registers_i386: unable to store i386_THREAD_FPSTATE: %s",
             kdp_return_string (kdpret));
        }
    }
}
#endif /* KDP_TARGET_I386 */

static void
kdp_store_registers (int regno)
{
  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to store registers (not connected)");
    }

  switch (kdp_host_type)
    {

    case bfd_arch_powerpc:
#if KDP_TARGET_POWERPC
      kdp_store_registers_ppc (regno);
#else
      error ("kdp_store_registers: not configured to support powerpc");
#endif
      break;

    case bfd_arch_i386:
#if KDP_TARGET_I386
      kdp_store_registers_i386 (regno);
#else
      error ("kdp_store_registers: not configured to support i386");
#endif
      break;

    default:
      error ("kdp_store_registers: unknown host type 0x%lx",
             (unsigned long) kdp_host_type);
    }
}

static void
kdp_fetch_registers (int regno)
{
  if (!kdp_is_connected (&c))
    {
      error ("kdp: unable to fetch registers (not connected)");
    }

  switch (kdp_host_type)
    {

    case bfd_arch_powerpc:
#if KDP_TARGET_POWERPC
      kdp_fetch_registers_ppc (regno);
#else
      error ("kdp_fetch_registers: not configured to support powerpc");
#endif
      break;

    case bfd_arch_i386:
#if KDP_TARGET_I386
      kdp_fetch_registers_i386 (regno);
#else
      error ("kdp_fetch_registers: not configured to support i386");
#endif
      break;

    default:
      error ("kdp_fetch_registers: unknown host type 0x%lx",
             (unsigned long) kdp_host_type);
    }
}

static void
kdp_prepare_to_store (void)
{
  kdp_fetch_registers (-1);
}

static int
kdp_xfer_memory (CORE_ADDR memaddr, char *myaddr, int len, int write,
                 struct mem_attrib *attrib, struct target_ops *target)
{
  kdp_return_t kdpret;

  if (!kdp_is_connected (&c))
    {
      logger (KDP_LOG_DEBUG,
              "kdp: unable to transfer memory (not connected)");
      return 0;
    }

  if (len > KDP_MAX_DATA_SIZE)
    {
      len = KDP_MAX_DATA_SIZE;
    }

  if (write)
    {
      c.request->writemem_req.hdr.request = KDP_WRITEMEM;
      c.request->writemem_req.address = memaddr;
      c.request->writemem_req.nbytes = len;
      memcpy (c.request->writemem_req.data, myaddr, len);

      kdpret = kdp_transaction (&c, c.request, c.response, "kdp_xfer_memory");
      if (c.response->writemem_reply.error != RR_SUCCESS)
        {
          kdpret = c.response->writemem_reply.error;
        }
      if (kdpret != RR_SUCCESS)
        {
          logger (KDP_LOG_DEBUG,
                  "kdp_xfer_memory: unable to store %d bytes at 0x%s: %s\n",
                  len, paddr_nz (memaddr), kdp_return_string (kdpret));
          return 0;
        }
    }
  else
    {
      c.request->readmem_req.hdr.request = KDP_READMEM;
      c.request->readmem_req.address = memaddr;
      c.request->readmem_req.nbytes = len;

      kdpret = kdp_transaction (&c, c.request, c.response, "kdp_xfer_memory");
      if (c.response->readmem_reply.error != RR_SUCCESS)
        {
          kdpret = c.response->readmem_reply.error;
        }
      if (kdpret != RR_SUCCESS)
        {
          logger (KDP_LOG_DEBUG,
                  "kdp_xfer_memory: unable to fetch %d bytes from 0x%s: %s\n",
                  len, paddr_nz (memaddr), kdp_return_string (kdpret));
          return 0;
        }
      if (c.response->readmem_reply.nbytes != len)
        {
          logger (KDP_LOG_DEBUG,
                  "kdp_xfer_memory: kdp read only %d bytes of data (expected %d)\n",
                  c.response->readmem_reply.nbytes, len);
          return 0;
        }
      memcpy (myaddr, c.response->readregs_reply.data, len);
    }

  return len;
}

static void
kdp_files_info (struct target_ops *ops)
{
  printf_unfiltered ("\tNo connection information available.\n");
}

static void
kdp_kill (void)
{
  kdp_detach ("", 0);
}

static void
kdp_load (char *args, int from_tty)
{
  error ("unsupported operation kdp_load");
}

static void
kdp_create_inferior (char *execfile, char *args, char **env)
{
  error ("unsupported operation kdp_create_inferior");
}

static void
kdp_mourn_inferior ()
{
  unpush_target (&kdp_ops);
  generic_mourn_inferior ();
}

static int remote_async_terminal_ours_p = 1;
static void (*ofunc) (int);
static PTR sigint_remote_twice_token;
static PTR sigint_remote_token;

static void remote_interrupt_twice (int signo);
static void remote_interrupt (int signo);
static void handle_remote_sigint_twice (int sig);
static void handle_remote_sigint (int sig);
static void async_remote_interrupt_twice (gdb_client_data arg);
static void async_remote_interrupt (gdb_client_data arg);

static void
interrupt_query (void)
{
  target_terminal_ours ();

  if (query ("Interrupted while waiting for the program.\n\
Give up (and stop debugging it)? "))
    {
      target_mourn_inferior ();
      throw_exception (RETURN_QUIT);
    }

  target_terminal_inferior ();
}

static void
remote_interrupt_twice (int signo)
{
  signal (signo, ofunc);
  interrupt_query ();
  signal (signo, remote_interrupt);
}

static void
remote_interrupt (int signo)
{
  signal (signo, remote_interrupt_twice);
  target_stop ();
}

static void
handle_remote_sigint_twice (int sig)
{
  signal (sig, handle_sigint);
  sigint_remote_twice_token =
    create_async_signal_handler (inferior_event_handler_wrapper, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_twice_token);
}

static void
handle_remote_sigint (int sig)
{
  signal (sig, handle_remote_sigint_twice);
  sigint_remote_twice_token =
    create_async_signal_handler (async_remote_interrupt_twice, NULL);
  mark_async_signal_handler_wrapper (sigint_remote_token);
}

static void
async_remote_interrupt_twice (gdb_client_data arg)
{
  if (target_executing)
    {
      signal (SIGINT, handle_remote_sigint);
      interrupt_query ();
    }
}

static void
async_remote_interrupt (gdb_client_data arg)
{
  target_stop ();
}

static void
cleanup_sigint_signal_handler (void *dummy)
{
  signal (SIGINT, handle_sigint);
  if (sigint_remote_twice_token)
    delete_async_signal_handler ((struct async_signal_handler **)
                                 &sigint_remote_twice_token);
  if (sigint_remote_token)
    delete_async_signal_handler ((struct async_signal_handler **)
                                 &sigint_remote_token);
}

static void
initialize_sigint_signal_handler (void)
{
  sigint_remote_token =
    create_async_signal_handler (async_remote_interrupt, NULL);
  signal (SIGINT, handle_remote_sigint);
}

static void
kdp_terminal_inferior (void)
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
  initialize_sigint_signal_handler ();
}

static void
kdp_terminal_ours (void)
{
  /* terminal_ours (); */

  if (!sync_execution)
    return;
  if (remote_async_terminal_ours_p)
    return;
  CHECK_FATAL (sync_execution);
  CHECK_FATAL (!remote_async_terminal_ours_p);
  cleanup_sigint_signal_handler (NULL);

  add_file_handler (input_fd, stdin_event_handler, 0);

  remote_async_terminal_ours_p = 1;
}

static void (*async_client_callback) (enum inferior_event_type event_type,
                                      void *context);
static void *async_client_context;

static void
kdp_file_handler (int error, gdb_client_data client_data)
{
  async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
kdp_file_handler_callback (void *arg)
{
  async_client_callback (INF_REG_EVENT, async_client_context);
}

static void
kdp_async (void (*callback) (enum inferior_event_type event_type,
                             void *context), void *context)
{
  if (current_target.to_async_mask_value == 0)
    internal_error (__FILE__, __LINE__,
                    "Calling remote_async when async is masked");

  if (callback != NULL)
    {
      async_client_callback = callback;
      async_client_context = context;
      if (c.reqfd > 0)
        add_file_handler (c.reqfd, kdp_file_handler, NULL);
      if (c.excfd > 0)
        add_file_handler (c.excfd, kdp_file_handler, NULL);
    }
  else
    {
      if (c.reqfd > 0)
        delete_file_handler (c.reqfd);
      if (c.excfd > 0)
        delete_file_handler (c.excfd);
    }

  if ((callback != NULL) && (c.saved_exception_pending))
    {

      gdb_queue_event (kdp_file_handler_callback, (void *) 0, TAIL);

    }
}

static void
init_kdp_ops (void)
{
  kdp_ops.to_shortname = "remote-kdp";
  kdp_ops.to_longname = "Remote Mac OS X system via KDP";
  kdp_ops.to_doc = "Remotely debug a Mac OS X system using KDP\n\
    Use the attach <hostname> command subsequently to connect via KDP.";
  kdp_ops.to_open = kdp_open;
  kdp_ops.to_close = kdp_close;
  kdp_ops.to_attach = kdp_attach;
  kdp_ops.to_resume = kdp_resume;
  kdp_ops.to_wait = kdp_wait;
  kdp_ops.to_fetch_registers = kdp_fetch_registers;
  kdp_ops.to_store_registers = kdp_store_registers;
  kdp_ops.to_prepare_to_store = kdp_prepare_to_store;
  kdp_ops.to_xfer_memory = kdp_xfer_memory;
  kdp_ops.to_files_info = kdp_files_info;
  kdp_ops.to_insert_breakpoint = memory_insert_breakpoint;
  kdp_ops.to_remove_breakpoint = memory_remove_breakpoint;
  kdp_ops.to_detach = kdp_detach;
  kdp_ops.to_kill = kdp_kill;
  kdp_ops.to_load = kdp_load;
  kdp_ops.to_create_inferior = kdp_create_inferior;
  kdp_ops.to_mourn_inferior = kdp_mourn_inferior;
  kdp_ops.to_stratum = process_stratum;
  kdp_ops.to_can_async_p = standard_can_async_p;
  kdp_ops.to_is_async_p = standard_is_async_p;
  kdp_ops.to_terminal_inferior = kdp_terminal_inferior;
  kdp_ops.to_terminal_ours = kdp_terminal_ours;
  kdp_ops.to_async = kdp_async;
  kdp_ops.to_async_mask_value = 1;
  kdp_ops.to_magic = OPS_MAGIC;
}

static void
update_kdp_default_host_type (char *args,
                              int from_tty, struct cmd_list_element *c)
{
  int htype;

  if (args == NULL)
    {
      args = kdp_default_host_type_str;
    }
  htype = parse_host_type (args);
  if (htype < 0)
    {
      if (htype == -2)
        {
          error ("Known but unsupported host type: \"%s\".", args);
        }
      else
        {
          error ("Unknown host type: \"%s\".", args);
        }
    }

  kdp_default_host_type = htype;
}

void
_initialize_remote_kdp (void)
{
  static const char *archlist[] = { "powerpc", "ia32", NULL };

  struct cmd_list_element *cmd = NULL;

  init_kdp_ops ();
  add_target (&kdp_ops);

  add_com ("kdp-reattach", class_run, kdp_reattach_command,
           "Re-attach to a (possibly connected) remote Mac OS X kernel.\nThe kernel must support the reattach packet.");
  add_com ("kdp-reboot", class_run, kdp_reboot_command,
           "Reboot a (possibly connected) remote Mac OS X kernel.\nThe kernel must support the reboot packet.");
  add_com ("kdp-detach", class_run, kdp_detach_command,
           "Reset a (possibly disconnected) remote Mac OS X kernel.\n");

  cmd = add_set_enum_cmd
    ("kdp-default-host-type", class_obscure, archlist,
     (void *) &kdp_default_host_type_str,
     "Set CPU type to be used for hosts providing incorect information (powerpc/ia32).",
     &setlist);
  set_cmd_sfunc (cmd, update_kdp_default_host_type);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd
    ("kdp-timeout", class_obscure, var_zinteger,
     (char *) &kdp_timeout,
     "Set UDP timeout in milliseconds for (non-exception) KDP transactions.",
     &setlist);
  add_show_from_set (cmd, &showlist);
  set_cmd_sfunc (cmd, set_timeouts);

  cmd = add_set_cmd
    ("kdp-retries", class_obscure, var_zinteger,
     (char *) &kdp_retries,
     "Set number of UDP retries for (non-exception) KDP transactions.",
     &setlist);
  add_show_from_set (cmd, &showlist);
  set_cmd_sfunc (cmd, set_timeouts);

  cmd = add_set_cmd
    ("kdp-default-port", class_obscure, var_zinteger,
     (char *) &kdp_default_port,
     "Set default UDP port on which to attempt to contact KDP.", &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd
    ("kdp-debug-level", class_obscure, var_zinteger,
     (char *) &kdp_debug_level,
     "Set level of verbosity for KDP debugging information.", &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd
    ("kdp-sequence-number", class_obscure, var_zinteger,
     (char *) &c.seqno,
     "Set current sequence number for KDP transactions.", &setlist);
  add_show_from_set (cmd, &showlist);

  cmd = add_set_cmd
    ("kdp-exception-sequence-number", class_obscure, var_zinteger,
     (char *) &c.exc_seqno,
     "Set current sequence number for KDP exception transactions.", &setlist);
  add_show_from_set (cmd, &showlist);

  kdp_reset (&c);
}
