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

#include "defs.h"
#include "inferior.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "terminal.h"
#include "gdbcmd.h"
#include "regcache.h"

#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-debug.h"

#include <mach-o/nlist.h>
#include <mach-o/dyld_debug.h>

#include <mach/mach_error.h>

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#include <unistd.h>

#include <AvailabilityMacros.h>

#define MACH64 (MAC_OS_X_VERSION_MAX_ALLOWED >= 1040)

#if MACH64

#include <mach/mach_vm.h>

#else /* ! MACH64 */

#define mach_vm_size_t vm_size_t
#define mach_vm_address_t vm_address_t
#define mach_vm_read vm_read
#define mach_vm_write vm_write
#define mach_vm_region vm_region
#define mach_vm_protect vm_protect
#define VM_REGION_BASIC_INFO_COUNT_64 VM_REGION_BASIC_INFO_COUNT
#define VM_REGION_BASIC_INFO_64 VM_REGION_BASIC_INFO

#endif /* MACH64 */

#define MAX_INSTRUCTION_CACHE_WARNINGS 0

/* MINUS_INT_MIN is the absolute value of the minimum value that can
   be stored in a int.  We can't just use -INT_MIN, as that would
   implicitly be a int, not an unsigned int, and would overflow on 2's
   complement machines. */

#define MINUS_INT_MIN (((unsigned int) (- (INT_MAX + INT_MIN))) + INT_MAX)

static FILE *mutils_stderr = NULL;
static int mutils_debugflag = 0;

extern macosx_inferior_status *macosx_status;

void
mutils_debug (const char *fmt, ...)
{
  va_list ap;
  if (mutils_debugflag)
    {
      va_start (ap, fmt);
      fprintf (mutils_stderr, "[%d mutils]: ", getpid ());
      vfprintf (mutils_stderr, fmt, ap);
      va_end (ap);
      fflush (mutils_stderr);
    }
}

unsigned int
child_get_pagesize ()
{
  kern_return_t status;
  static int result = -1;

  if (result == -1)
    {
      status = host_page_size (mach_host_self (), &result);
      /* This is probably being over-careful, since if we
         can't call host_page_size on ourselves, we probably
         aren't going to get much further.  */
      if (status != KERN_SUCCESS)
        result = -1;
      MACH_CHECK_ERROR (status);
    }

  return result;
}

/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   WRITE is nonzero.

   Returns the length copied. */

static int
mach_xfer_memory_remainder (CORE_ADDR memaddr, char *myaddr,
                            int len, int write,
                            struct mem_attrib *attrib,
                            struct target_ops *target)
{
  unsigned int pagesize = child_get_pagesize ();

  vm_offset_t mempointer;       /* local copy of inferior's memory */
  mach_msg_type_number_t memcopied;     /* for vm_read to use */

  CORE_ADDR pageaddr = memaddr - (memaddr % pagesize);

  kern_return_t kret;

  CHECK_FATAL (((memaddr + len - 1) - ((memaddr + len - 1) % pagesize))
               == pageaddr);

  kret = mach_vm_read (macosx_status->task, pageaddr, pagesize,
                       &mempointer, &memcopied);

  if (kret != KERN_SUCCESS)
    {
      mutils_debug
        ("Unable to read page for region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
         paddr_nz (pageaddr), (unsigned long) len,
         MACH_ERROR_STRING (kret), kret);
      return 0;
    }
  if (memcopied != pagesize)
    {
      kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
      if (kret != KERN_SUCCESS)
        {
          warning
            ("Unable to deallocate memory used by failed read from inferior: %s (0x%lx)",
             MACH_ERROR_STRING (kret), (unsigned long) kret);
        }
      mutils_debug
        ("Unable to read region at 0x%s with length %lu from inferior: "
         "vm_read returned %lu bytes instead of %lu\n",
         paddr_nz (pageaddr), (unsigned long) pagesize,
         (unsigned long) memcopied, (unsigned long) pagesize);
      return 0;
    }

  if (!write)
    {
      memcpy (myaddr, ((unsigned char *) 0) + mempointer
              + (memaddr - pageaddr), len);
    }
  else
    {
      vm_machine_attribute_val_t flush = MATTR_VAL_CACHE_FLUSH;
      memcpy (((unsigned char *) 0) + mempointer
              + (memaddr - pageaddr), myaddr, len);
      kret = vm_machine_attribute (mach_task_self (), mempointer,
                                   pagesize, MATTR_CACHE, &flush);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to flush GDB's address space after memcpy prior to vm_write: %s (0x%lx)\n",
             MACH_ERROR_STRING (kret), kret);
        }
      kret =
        mach_vm_write (macosx_status->task, pageaddr, (pointer_t) mempointer,
                       pagesize);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to write region at 0x%s with length %lu to inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }

  kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
  if (kret != KERN_SUCCESS)
    {
      warning
        ("Unable to deallocate memory used to read from inferior: %s (0x%ulx)",
         MACH_ERROR_STRING (kret), kret);
      return 0;
    }

  return len;
}

static int
mach_xfer_memory_block (CORE_ADDR memaddr, char *myaddr,
                        int len, int write,
                        struct mem_attrib *attrib, struct target_ops *target)
{
  unsigned int pagesize = child_get_pagesize ();

  vm_offset_t mempointer;       /* local copy of inferior's memory */
  mach_msg_type_number_t memcopied;     /* for vm_read to use */

  kern_return_t kret;

  CHECK_FATAL ((memaddr % pagesize) == 0);
  CHECK_FATAL ((len % pagesize) == 0);

  if (!write)
    {
      kret =
        mach_vm_read (macosx_status->task, memaddr, len, &mempointer,
                      &memcopied);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to read region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
      if (memcopied != len)
        {
          kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
          if (kret != KERN_SUCCESS)
            {
              warning
                ("Unable to deallocate memory used by failed read from inferior: %s (0x%ux)",
                 MACH_ERROR_STRING (kret), kret);
            }
          mutils_debug
            ("Unable to read region at 0x%s with length %lu from inferior: "
             "vm_read returned %lu bytes instead of %lu\n",
             paddr_nz (memaddr), (unsigned long) len,
             (unsigned long) memcopied, (unsigned long) len);
          return 0;
        }
      memcpy (myaddr, ((unsigned char *) 0) + mempointer, len);
      kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
      if (kret != KERN_SUCCESS)
        {
          warning
            ("Unable to deallocate memory used by read from inferior: %s (0x%ulx)",
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }
  else
    {
      kret =
        mach_vm_write (macosx_status->task, memaddr, (pointer_t) myaddr, len);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to write region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }

  return len;
}

int
mach_xfer_memory (CORE_ADDR memaddr, char *myaddr,
                  int len, int write,
                  struct mem_attrib *attrib, struct target_ops *target)
{
  mach_vm_address_t r_start;
  mach_vm_address_t r_end;
  mach_vm_size_t r_size;
  mach_port_t r_object_name;

  vm_region_basic_info_data_64_t r_data;
  mach_msg_type_number_t r_info_size;

  CORE_ADDR cur_memaddr;
  unsigned char *cur_myaddr;
  int cur_len;

  unsigned int pagesize = child_get_pagesize ();
  vm_machine_attribute_val_t flush = MATTR_VAL_CACHE_FLUSH;
  kern_return_t kret;
  int ret;

  /* check for out-of-range address */
  r_start = memaddr;
  if (r_start != memaddr)
    {
      errno = EINVAL;
      return 0;
    }

  if (len == 0)
    {
      return 0;
    }

  CHECK_FATAL (myaddr != NULL);
  errno = 0;

  /* check for case where memory available only at address greater than address specified */
  {
    r_start = memaddr;
    r_info_size = VM_REGION_BASIC_INFO_COUNT_64;
    kret = mach_vm_region (macosx_status->task, &r_start, &r_size,
                           VM_REGION_BASIC_INFO_64,
                           (vm_region_info_t) &r_data, &r_info_size,
                           &r_object_name);
    if (kret != KERN_SUCCESS)
      {
        return 0;
      }
    if (r_start > memaddr)
      {
        if ((r_start - memaddr) <= MINUS_INT_MIN)
          {
            mutils_debug
              ("First available address near 0x%s is at 0x%s; returning\n",
               paddr_nz (memaddr), paddr_nz (r_start));
            return -(r_start - memaddr);
          }
        else
          {
            mutils_debug ("First available address near 0x%s is at 0x%s "
                          "(too far; returning 0)\n",
                          paddr_nz (memaddr), paddr_nz (r_start));
            return 0;
          }
      }
  }

  cur_memaddr = memaddr;
  cur_myaddr = myaddr;
  cur_len = len;

  while (cur_len > 0)
    {

      r_start = cur_memaddr;

      r_info_size = VM_REGION_BASIC_INFO_COUNT_64;
      kret = mach_vm_region (macosx_status->task, &r_start, &r_size,
                             VM_REGION_BASIC_INFO_64,
                             (vm_region_info_t) & r_data, &r_info_size,
                             &r_object_name);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to read region information for memory at 0x%s: %s (0x%lx)\n",
             paddr_nz (cur_memaddr), MACH_ERROR_STRING (kret), kret);
          break;
        }

      if (r_start > cur_memaddr)
        {
          mutils_debug
            ("Next available region for address at 0x%s is 0x%s\n",
             paddr_nz (cur_memaddr), paddr_nz (r_start));
          break;
        }

      if (write)
        {
          kret = mach_vm_protect (macosx_status->task, r_start, r_size, 0,
                             VM_PROT_READ | VM_PROT_WRITE);
          if (kret != KERN_SUCCESS)
            {
              kret = mach_vm_protect (macosx_status->task, r_start, r_size, 0,
                                 VM_PROT_COPY | VM_PROT_READ | VM_PROT_WRITE);
            }
          if (kret != KERN_SUCCESS)
            {
              mutils_debug
                ("Unable to add write access to region at 0x%s: %s (0x%lx)",
                 paddr_nz (r_start), MACH_ERROR_STRING (kret), kret);
              break;
            }
        }

      r_end = r_start + r_size;

      CHECK_FATAL (r_start <= cur_memaddr);
      CHECK_FATAL (r_end >= cur_memaddr);
      CHECK_FATAL ((r_start % pagesize) == 0);
      CHECK_FATAL ((r_end % pagesize) == 0);
      CHECK_FATAL (r_end >= (r_start + pagesize));

      if ((cur_memaddr % pagesize) != 0)
        {
          int max_len = pagesize - (cur_memaddr % pagesize);
          int op_len = cur_len;
          if (op_len > max_len)
            {
              op_len = max_len;
            }
          ret = mach_xfer_memory_remainder (cur_memaddr, cur_myaddr, op_len,
                                            write, attrib, target);
        }
      else if (cur_len >= pagesize)
        {
          int max_len = r_end - cur_memaddr;
          int op_len = cur_len;
          if (op_len > max_len)
            {
              op_len = max_len;
            }
          op_len -= (op_len % pagesize);
          ret = mach_xfer_memory_block (cur_memaddr, cur_myaddr, op_len,
                                        write, attrib, target);
        }
      else
        {
          ret = mach_xfer_memory_remainder (cur_memaddr, cur_myaddr, cur_len,
                                            write, attrib, target);
        }

      cur_memaddr += ret;
      cur_myaddr += ret;
      cur_len -= ret;

      if (write)
        {
          kret = vm_machine_attribute (macosx_status->task, r_start, r_size,
                                       MATTR_CACHE, &flush);
          if (kret != KERN_SUCCESS)
            {
              static int nwarn = 0;
              nwarn++;
              if (nwarn <= MAX_INSTRUCTION_CACHE_WARNINGS)
                {
                  warning
                    ("Unable to flush data/instruction cache for region at 0x%s: %s",
                     paddr_nz (r_start), MACH_ERROR_STRING (ret));
                }
              if (nwarn == MAX_INSTRUCTION_CACHE_WARNINGS)
                {
                  warning
                    ("Support for flushing the data/instruction cache on this machine appears broken");
                  warning ("No further warning messages will be given.");
                }
              break;
            }
          kret = mach_vm_protect (macosx_status->task, r_start, r_size, 0,
                             r_data.protection);
          if (kret != KERN_SUCCESS)
            {
              warning
                ("Unable to restore original permissions for region at 0x%s",
                 paddr_nz (r_start));
              break;
            }
        }

      if (ret == 0)
        {
          break;
        }
    }

  return len - cur_len;
}

int
macosx_port_valid (port_t port)
{
  mach_port_type_t ptype;
  kern_return_t ret;

  ret = mach_port_type (mach_task_self (), port, &ptype);
  return (ret == KERN_SUCCESS);
}

int
macosx_task_valid (task_t task)
{
  kern_return_t ret;
  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  ret = task_info (task, TASK_BASIC_INFO, (task_info_t) & info, &info_count);
  return (ret == KERN_SUCCESS);
}

int
macosx_thread_valid (task_t task, thread_t thread)
{
  thread_array_t thread_list;
  unsigned int thread_count;
  kern_return_t kret;

  unsigned int found = 0;
  unsigned int i;

  CHECK_FATAL (task != TASK_NULL);

  kret = task_threads (task, &thread_list, &thread_count);
  if ((kret == KERN_INVALID_ARGUMENT)
      || (kret == MACH_SEND_INVALID_RIGHT) || (kret == MACH_RCV_INVALID_NAME))
    {
      return 0;
    }
  MACH_CHECK_ERROR (kret);

  for (i = 0; i < thread_count; i++)
    {
      if (thread_list[i] == thread)
        {
          found = 1;
        }
    }

  kret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                        (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (kret);

  if (!found)
    {
      mutils_debug ("thread 0x%lx no longer valid for task 0x%lx\n",
                    (unsigned long) thread, (unsigned long) task);
    }
  return found;
}

int
macosx_pid_valid (int pid)
{
  int ret;
  ret = kill (pid, 0);
  mutils_debug ("kill (%d, 0) : ret = %d, errno = %d (%s)\n", pid,
                ret, errno, strerror (errno));
  return ((ret == 0) || ((errno != ESRCH) && (errno != ECHILD)));
}

void
mach_check_error (kern_return_t ret, const char *file,
                  unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  error ("error on line %u of \"%s\" in function \"%s\": %s (0x%lx)\n",
         line, file, func, MACH_ERROR_STRING (ret), (unsigned long) ret);
}

void
mach_warn_error (kern_return_t ret, const char *file,
                 unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS)
    {
      return;
    }
  if (func == NULL)
    {
      func = "[UNKNOWN]";
    }

  warning ("error on line %u of \"%s\" in function \"%s\": %s (0x%ux)",
           line, file, func, MACH_ERROR_STRING (ret), ret);
}

thread_t
macosx_primary_thread_of_task (task_t task)
{
  thread_array_t thread_list;
  unsigned int thread_count;
  thread_t tret = THREAD_NULL;
  kern_return_t ret;

  CHECK_FATAL (task != TASK_NULL);

  ret = task_threads (task, &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  tret = thread_list[0];

  ret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  return tret;
}

kern_return_t
macosx_msg_receive (mach_msg_header_t * msgin, size_t msg_size,
                    unsigned long timeout, port_t port)
{
  kern_return_t kret;
  mach_msg_option_t options;

  mutils_debug ("macosx_msg_receive: waiting for message\n");

  options = MACH_RCV_MSG;
  if (timeout > 0)
    {
      options |= MACH_RCV_TIMEOUT;
    }
  kret = mach_msg (msgin, options, 0, msg_size, port,
                   timeout, MACH_PORT_NULL);

  if (mutils_debugflag)
    {
      if (kret == KERN_SUCCESS)
        {
          macosx_debug_message (msgin);
        }
      else
        {
          mutils_debug ("macosx_msg_receive: returning %s (0x%lx)\n",
                        MACH_ERROR_STRING (kret), kret);
        }
    }

  return kret;
}

void
_initialize_macosx_mutils ()
{
  struct cmd_list_element *cmd;

  mutils_stderr = fdopen (fileno (stderr), "w+");

  cmd = add_set_cmd ("mutils", class_obscure, var_boolean,
                     (char *) &mutils_debugflag,
                     "Set if printing inferior memory debugging statements.",
                     &setdebuglist), add_show_from_set (cmd, &showdebuglist);
}
