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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include "bfd.h"
#include "defs.h"
#include "gdbcore.h"
#include "serial.h"
#include "ser-base.h"
#include "ser-unix.h"
#include "inferior.h"
#include "objc-lang.h"
#include "infcall.h"

#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-info.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-dyld-process.h"

extern macosx_inferior_status *macosx_status;
extern macosx_dyld_thread_status macosx_dyld_status;
extern int inferior_auto_start_cfm_flag;
extern int inferior_auto_start_dyld_flag;

/* classic-inferior-support
   FIXME: All of these macosx_classic_ functions belong over in
   serial.c, but we're sticking them in here for now so as to perturb
   things the least amount possible.  jmolenda/2005-05-02  */

static void
macosx_classic_unix_close (struct serial *scb)
{
  if (scb->fd < 0)
    return;
    
  close (scb->fd);
  scb->fd = -1;
}

/* classic-inferior-support
   This should really be over in serial.c.  */

static int
macosx_classic_unix_open (struct serial *scb, const char *name)
{
  struct sockaddr_un sockaddr;
  int n;
    
  if (strncmp (name, "unix:", 5) != 0)
    return -1;
    
  name += 5;
  
  scb->fd = socket (PF_UNIX, SOCK_STREAM, 0);
  if (scb->fd == -1)
    return -1;
    
  sockaddr.sun_family = PF_UNIX;
  strcpy (sockaddr.sun_path, name);
  sockaddr.sun_len = sizeof (sockaddr);
    
  n = connect (scb->fd, (struct sockaddr *)&sockaddr, sizeof (sockaddr));
  if (n == -1) 
    {
      macosx_classic_unix_close (scb);
      return -1;
    }
    
  signal (SIGPIPE, SIG_IGN);
    
  return 0;   
}

/* classic-inferior-support
   This should really be over in serial.c.  */

static void
macosx_classic_deliver_signal (int sig)
{
  printf_filtered ("sending signal %d to pid %d\n", sig, macosx_status->pid);
  kill (macosx_status->pid, sig);
}

/* classic-inferior-support
   This should really be over in serial.c.  */

static void
macosx_classic_stop_inferior (void)
{
  macosx_classic_deliver_signal (SIGINT);
}

/* classic-inferior-support */

static void
macosx_classic_create_inferior (pid_t pid)
{   
  int kr;
    
  task_t task;
  kr = task_for_pid (mach_task_self (),  pid, &task);
  if (kr != KERN_SUCCESS)
    {
      if (macosx_get_task_for_pid_rights () == 1)
	kr = task_for_pid (mach_task_self (), pid, &task);
    }
  
  if (kr != KERN_SUCCESS) 
    {
      error ("task_for_pid failed for pid %d: %s", pid, 
             mach_error_string (kr));
    } 
   else 
    {
      macosx_create_inferior_for_task (macosx_status, task, pid);
      printf_filtered ("pid %d -> mach task %d\n", macosx_status->pid, 
                        macosx_status->task);
      if (inferior_auto_start_dyld_flag)
        {
          int i;
          struct dyld_objfile_entry *e;

          // remove all the currently cached objfiles since we've started 
          // a new session

          DYLD_ALL_OBJFILE_INFO_ENTRIES (&macosx_dyld_status.current_info, e, i)
          {
              dyld_remove_objfile (e);
              dyld_objfile_entry_clear (e);
          }

          macosx_dyld_init (&macosx_dyld_status, exec_bfd);
          // remove all the old objfiles to work around rdar://4091532
          DYLD_ALL_OBJFILE_INFO_ENTRIES (&macosx_dyld_status.current_info, e, i)
          {
              dyld_remove_objfile (e);
              dyld_objfile_entry_clear (e);
          }
          macosx_dyld_status.dyld_minsyms_have_been_relocated = 1;
          macosx_dyld_update (1);
        }
#if WITH_CFM
      if (inferior_auto_start_cfm_flag) 
        {
          macosx_cfm_thread_init (&macosx_status->cfm_status);
        }
#endif
    }
}

/* classic-inferior-support
   Classic applications that are in debug mode will have
   a socket in /tmp under a specific filename.
   Returns 1 if the socket exists.
   Returns 0 if the socket doesn't exist, or there was an error in checking.  */

static int
classic_socket_exists_p (pid_t pid)
{
  char name[PATH_MAX];
  struct stat sb;

  sprintf (name, "/tmp/translate.gdb.%d", pid);
  if (stat (name, &sb) != 0)
    return 0;
  if (sb.st_mode & S_IFSOCK)
    return 1;

  return 0;
}


/* classic-inferior-support
   Determine if PID is a classic process or not.
   Returns 
   1 if it's classic, 
   0 if it's a normal process, or 
   -1 if there was an error making the determination (most likely
   because the process is running under a different uid and gdb isn't
   being run by root.)  */

int
is_pid_classic (pid_t pid)
{
  int mib[] = { CTL_KERN, KERN_CLASSIC, pid };
  size_t len = sizeof (int);
  int ret = 0;
  if (sysctl (mib, 3, &ret, &len, NULL, 0) == -1)
    return -1;
  return ret;
}

/* classic-inferior-support
   Determine if this gdb process can attach to target process TARGET_PID.

   Returns 1 if this gdb can attach to PID.
   Returns 0 if not.  */

int
can_attach (pid_t target_pid)
{
  int gdb_is_classic, target_is_classic;

  target_is_classic = is_pid_classic (target_pid);
  gdb_is_classic = is_pid_classic (getpid ());

  /* We can't tell if we're classic ourselves -- the kernel probably
     doesn't support this call, so let's assume everything is attachable.  */
  if (gdb_is_classic == -1)
      return 1;

  if (gdb_is_classic == 1)
    {
      if (target_is_classic == 1)
        target_is_classic = classic_socket_exists_p (target_pid);

      if (target_is_classic == 1)
        return 1;
      if (target_is_classic == 0)
        return 0;

      /* Couldn't tell -- don't include it in list of process.  */
      if (target_is_classic == -1)
        return 0;
    }

  if (gdb_is_classic == 0) 
    {
      if (target_is_classic == 0)
          return 1;

      /* List processes we couldn't get classic status of.  Fixme - these
         are all processes running under other uids so we can't inspect
         them.  It'd be a nice refinement to suppress these.  Could look
         at errno after the call to see if we got EPERM.  */
      if (target_is_classic == -1)
        return 1;
    }

  /* notreached */
  return 1;
}

/* classic-inferior-support
   We're about to attach to the process at TARGET_PID.  If we should use the 
   classic process attach process (involving the remote protocol), return 1.

   If this is a normal process attach, return 0. */

int
attaching_to_classic_process_p (pid_t target_pid)
{
  int gdb_is_classic, target_is_classic;

  target_is_classic = is_pid_classic (target_pid);
  gdb_is_classic = is_pid_classic (getpid ());

  /* gdb and target are classic - we're set. */
  if (gdb_is_classic == 1 && target_is_classic == 1)
      return 1;

  /* When in doubt, follow the standard dyld/mach attach procedure.  */

  return 0;
}
  
extern struct target_ops remote_ops;

/* classic-inferior-support
   Use the classic process attach method instead of the normal
   Mach attach code. */

void
attach_to_classic_process (pid_t pid)
{
  char name[PATH_MAX];
  sprintf (name, "unix:/tmp/translate.gdb.%d", pid);
  push_remote_target (name, 0);
  macosx_classic_create_inferior (pid);
  remote_ops.to_stop = macosx_classic_stop_inferior;

  /* Debugging translated processes means no inferior function calls.  So
     no malloc, no calling into the objc runtime to look anything up, etc.  */

  inferior_function_calls_disabled_p = 1;
  lookup_objc_class_p = 0;

  update_current_target ();
}

static int orig_rlimit;

void
restore_orig_rlimit ()
{
  struct rlimit limit;

  getrlimit (RLIMIT_NOFILE, &limit);
  limit.rlim_cur = orig_rlimit;
  setrlimit (RLIMIT_NOFILE, &limit);
}

void
_initialize_macosx_nat ()
{
  struct rlimit limit;
  rlim_t reserve;
  int ret;

  getrlimit (RLIMIT_NOFILE, &limit);
  orig_rlimit = limit.rlim_cur;
  limit.rlim_cur = limit.rlim_max;
  ret = setrlimit (RLIMIT_NOFILE, &limit);
  if (ret != 0)
    {
      /* Okay, that didn't work, let's try something that's at least
	 reasonably big: */
      limit.rlim_cur = 10000;
      ret = setrlimit (RLIMIT_NOFILE, &limit);
    }
  /* rlim_max is set to RLIM_INFINITY on X, at least on Leopard &
     SnowLeopard.  so it's better to see what we really got and use
     cur, not max below...  */ 
  getrlimit (RLIMIT_NOFILE, &limit);

  /* Reserve 10% of file descriptors for non-BFD uses, or 5, whichever
     is greater.  Allocate at least one file descriptor for use by
     BFD. */

  reserve = limit.rlim_cur * 0.1;
  reserve = (reserve > 5) ? reserve : 5;
  if (reserve >= limit.rlim_cur)
    {
      bfd_set_cache_max_open (1);
    }
  else
    {
      bfd_set_cache_max_open (limit.rlim_cur - reserve);
    }

  /* classic-inferior-support */
  struct serial_ops *ops = XMALLOC (struct serial_ops);
  memset (ops, 0, sizeof (struct serial_ops));
  ops->name = "unix";
  ops->next = 0;
  ops->open = macosx_classic_unix_open;
  ops->close = macosx_classic_unix_close;
  ops->readchar = ser_base_readchar;
  ops->write = ser_base_write;
  ops->flush_output = ser_base_flush_output;
  ops->flush_input = ser_base_flush_input;
  ops->send_break = ser_base_send_break;
  ops->go_raw = ser_base_raw;
  ops->get_tty_state = ser_base_get_tty_state;
  ops->set_tty_state = ser_base_set_tty_state;
  ops->print_tty_state = ser_base_print_tty_state;
  ops->noflush_set_tty_state = ser_base_noflush_set_tty_state;
  ops->setbaudrate = ser_base_setbaudrate;
  ops->setstopbits = ser_base_setstopbits;
  ops->drain_output = ser_base_drain_output;
  ops->async = ser_base_async;
  ops->read_prim = ser_unix_read_prim;
  ops->write_prim = ser_unix_write_prim;
  serial_add_interface (ops);

}
