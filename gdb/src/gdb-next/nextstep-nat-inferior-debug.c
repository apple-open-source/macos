#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-dyld.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-sigthread.h"
#include "nextstep-nat-threads.h"

#include "nextstep-xdep.h"

#include "defs.h"
#include "inferior.h"
#include "target.h"
#include "symfile.h"
#include "symtab.h"
#include "objfiles.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbthread.h"

#include "bfd.h"

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <string.h>
#include <ctype.h>

FILE *inferior_stderr = NULL;
int inferior_debug_flag = 0;
int timestamps_debug_flag = 0;

void inferior_debug (int level, const char *fmt, ...)
{
  va_list ap;
  if (inferior_debug_flag >= level) {
    va_start (ap, fmt);
    fprintf (inferior_stderr, "[%d inferior]: ", getpid ());
    vfprintf (inferior_stderr, fmt, ap);
    va_end (ap);
    fflush (inferior_stderr);
  }
}

const char *unparse_exception_type (unsigned int i)
{
  switch (i) {
  case EXC_BAD_ACCESS: return "EXC_BAD_ACCESS";
  case EXC_BAD_INSTRUCTION: return "EXC_BAD_INSTRUCTION";
  case EXC_ARITHMETIC: return "EXC_ARITHMETIC";
  case EXC_EMULATION: return "EXC_EMULATION";
  case EXC_SOFTWARE: return "EXC_SOFTWARE";
  case EXC_BREAKPOINT: return "EXC_BREAKPOINT";

#ifdef __MACH30__
  case EXC_SYSCALL: return "EXC_SYSCALL";
  case EXC_MACH_SYSCALL: return "EXC_MACH_SYSCALL";
  case EXC_RPC_ALERT: return "EXC_RPC_ALERT";
#endif /* __MACH30__ */

  default:
    return "???";
  }
}

const char *unparse_protection (vm_prot_t p)
{
  switch (p) {
  case VM_PROT_NONE: return "---";
  case VM_PROT_READ: return "r--";
  case VM_PROT_WRITE: return "-w-";
  case VM_PROT_READ | VM_PROT_WRITE: return "rw-";
  case VM_PROT_EXECUTE: return "--x";
  case VM_PROT_EXECUTE | VM_PROT_READ: return "r-x";
  case VM_PROT_EXECUTE | VM_PROT_WRITE: return "-wx";
  case VM_PROT_EXECUTE | VM_PROT_WRITE | VM_PROT_READ: return "rwx";
  default:
    return "???";
  }
}

const char *unparse_inheritance (vm_inherit_t i)
{
  switch (i) {
  case VM_INHERIT_SHARE: return "share";
  case VM_INHERIT_COPY: return "copy";
  case VM_INHERIT_NONE: return "none";
  default:
    return "???";
  }
}

#if defined (__MACH30__)

void next_debug_region (task_t task, vm_address_t address)
{
  kern_return_t kret;
  struct vm_region_basic_info info;
  vm_size_t size;
  mach_port_t object_name;
  mach_msg_type_number_t count;

  count = VM_REGION_BASIC_INFO_COUNT;
  kret = vm_region (task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t) &info, &count, &object_name);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Region from 0x%lx to 0x%lx (size 0x%lx) "
		   "(currently %s; max %s; inheritance \"%s\"; %s; %s\n",
		   address, address + size, size,
		   unparse_protection (info.protection),
		   unparse_protection (info.max_protection),
		   unparse_inheritance (info.inheritance),
		   info.shared ? "shared" : "private",
		   info.reserved ? "reserved" : "not reserved");
}

#else /* ! __MACH30__ */

void next_debug_region (task_t task, vm_address_t address)
{
  kern_return_t kret;
  vm_size_t size;
  vm_prot_t protection;
  vm_prot_t max_protection;
  vm_inherit_t inheritance;
  boolean_t shared;
  port_t object_name;
  vm_offset_t offset;
  
  kret = vm_region (task, &address, &size,
		    &protection, &max_protection, &inheritance, &shared,
		    &object_name, &offset);
  MACH_CHECK_ERROR (kret);

  printf_filtered ("Region from 0x%lx to 0x%lx (size 0x%lx) "
		   "(currently %s; max %s; inheritance \"%s\"; %s\n",
		   address, address + size, size,
		   unparse_protection (protection), unparse_protection (max_protection),
		   unparse_inheritance (inheritance),
		   shared ? "shared" : "private");
}

#endif /* __MACH30__ */

#if defined (__MACH30__)

void next_debug_regions (task)
{
  kern_return_t kret;
  struct vm_region_basic_info info;
  vm_size_t size;
  mach_port_t object_name;
  mach_msg_type_number_t count;

  vm_address_t address;
  vm_address_t last_address;

  address = 0;
  last_address = (vm_address_t) -1;

  for (;;) {
    count = VM_REGION_BASIC_INFO_COUNT;
    kret = vm_region (task, &address, &size, VM_REGION_BASIC_INFO, (vm_region_info_t) &info, &count, &object_name);
    if (kret == KERN_NO_SPACE) {
      break;
    }
    if (last_address == address) {
      printf_filtered ("   ... ");
    } else {
      printf_filtered ("Region ");
    }
    printf_filtered ("from 0x%lx to 0x%lx (size 0x%lx) "
		     "(currently %s; max %s; inheritance \"%s\"; %s; %s\n",
		     address, address + size, size,
		     unparse_protection (info.protection),
		     unparse_protection (info.max_protection),
		     unparse_inheritance (info.inheritance),
		     info.shared ? "shared" : "private",
		     info.reserved ? "reserved" : "not reserved");
    address += size;
    last_address = address;
    if (address == 0) {
      /* address space has wrapped around */
      break;
    }
  }
}

#else /* ! __MACH30__ */

void next_debug_regions (task)
{
  kern_return_t kret;
  vm_size_t size;
  vm_prot_t protection;
  vm_prot_t max_protection;
  vm_inherit_t inheritance;
  boolean_t shared;
  port_t object_name;
  vm_offset_t offset;

  vm_address_t address;
  vm_address_t last_address;

  address = 0;
  last_address = (vm_address_t) -1;

  for (;;) {
    kret = vm_region (task, &address, &size,
		      &protection, &max_protection, &inheritance, &shared,
		      &object_name, &offset);
    if (kret == KERN_NO_SPACE) {
      break;
    }
    if (last_address == address) {
      printf_filtered ("   ... ");
    } else {
      printf_filtered ("Region ");
    }
    printf_filtered ("from 0x%lx to 0x%lx (size 0x%lx) "
		     "(currently %s; max %s; inheritance \"%s\"; %s\n",
		     address, address + size, size,
		     unparse_protection (protection), unparse_protection (max_protection),
		     unparse_inheritance (inheritance),
		     shared ? "shared" : "private");
    address += size;
    last_address = address;
  }
}

#endif /* __MACH30__ */

#if defined (__MACH30__)

void next_debug_port_info (task_t task, port_t port)
{
#if 0
  kern_return_t	kret;
  mach_port_status_t status;

  kret = mach_port_get_receive_status (task, port, &status);
  MACH_CHECK_ERROR (kret);

  printf_unfiltered ("Port 0x%lx in task 0x%lx:\n", (unsigned long) port, (unsigned long) task);
  printf_unfiltered ("  port set: 0x%lx\n", status.mps_pset);
  printf_unfiltered ("     seqno: 0x%lx\n", status.mps_seqno);
  printf_unfiltered ("   mscount: 0x%lx\n", status.mps_mscount);
  printf_unfiltered ("    qlimit: 0x%lx\n", status.mps_qlimit);
  printf_unfiltered ("  msgcount: 0x%lx\n", status.mps_msgcount);
  printf_unfiltered ("  sorights: 0x%lx\n", status.mps_sorights);
  printf_unfiltered ("   srights: 0x%lx\n", status.mps_srights);
  printf_unfiltered (" pdrequest: 0x%lx\n", status.mps_pdrequest);
  printf_unfiltered (" nsrequest: 0x%lx\n", status.mps_nsrequest);
  printf_unfiltered ("     flags: 0x%lx\n", status.mps_flags);
#endif /* 0 */
}

#else /* ! __MACH30__ */

void next_debug_port_info (task_t task, port_t port)
{
  kern_return_t	kret;
  port_set_name_t port_set;
  int number_messages, backlog;
  boolean_t ownership, receive_rights;

  kret = port_status (task, port, &port_set, &number_messages, &backlog, &ownership, &receive_rights);
  MACH_CHECK_ERROR (kret);

  printf_unfiltered ("Port 0x%lx in task 0x%lx:\n", (unsigned long) port, (unsigned long) task);
  printf_unfiltered ("  queued messages: %d\n", number_messages);
  printf_unfiltered ("  queue limit: %d\n", backlog);
  printf_unfiltered ("  task %s receive rights\n",
		     ((receive_rights) ? "has" : "does not have"));
  if (port_set != PORT_NULL) {
    printf_unfiltered ("  is in port set 0x%lx\n", (unsigned long) port_set);
  } else {
    printf_unfiltered ("  is not in a port set\n");
  }
}

#endif /* __MACH30__ */

void next_debug_task_port_info (mach_port_t task)
{
#if ! defined (__MACH30__)

  kern_return_t ret;
  unsigned int i;
  port_name_array_t names;
  port_type_array_t types;
  unsigned int nnames, ntypes;

  if (! inferior_debug_flag) { return; }

  ret = port_names (task, &names, &nnames, &types, &ntypes);
  MACH_WARN_ERROR (ret);
  if (ret != KERN_SUCCESS) { return; }

  CHECK_FATAL (nnames == ntypes);

  fprintf (inferior_stderr, "next_debug_task_port_info: ports for task 0x%lx:\n", (long) task);
  for (i = 0; i < nnames; i++) {
    char *s = NULL;
    switch (types[i]) {
    case PORT_TYPE_SEND: s = "SEND"; break;
    case PORT_TYPE_RECEIVE_OWN: s = "RECEIVE_OWN"; break;
    case PORT_TYPE_SET: s = "SET"; break;
    default: s = "[UNKNOWN]"; break;
    }
    fprintf (inferior_stderr, "  0x%lx: %s\n", (long) names[i], s);
  }

  ret = vm_deallocate (task_self(), (vm_address_t) names, nnames * sizeof (port_name_t));
  MACH_WARN_ERROR (ret);

  ret = vm_deallocate (task_self(), (vm_address_t) types, ntypes * sizeof (port_type_t));
  MACH_WARN_ERROR (ret);

#endif /* __MACH30__ */
}

void next_debug_inferior_status (next_inferior_status *s)
{
  kern_return_t ret;
  thread_array_t thread_list;
  unsigned int thread_count;
  unsigned int i;

  fprintf (inferior_stderr, "next_debug_inferior_status: current status:\n");
  fprintf (inferior_stderr, "              inferior task: 0x%lx\n", (unsigned long) s->task);

  next_signal_thread_debug (inferior_stderr, &s->signal_status);

  fprintf (inferior_stderr, "next_debug_inferior_status: information on debugger task:\n");
  next_debug_task_port_info (task_self());

  fprintf (inferior_stderr, "next_debug_inferior_status: information on inferior task:\n");
  next_debug_task_port_info (s->task);

  fprintf (inferior_stderr, "next_debug_inferior_status: information on debugger threads:\n");
  ret = task_threads (task_self(), &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  for (i = 0; i < thread_count; i++) {
    fprintf (inferior_stderr, "  thread: 0x%lx\n", (long) thread_list[i]);
  }

  ret = vm_deallocate (task_self(), (vm_address_t) thread_list, 
		       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  fprintf (inferior_stderr, "next_debug_inferior_status: information on inferior threads:\n");
  ret = task_threads (s->task, &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  for (i = 0; i < thread_count; i++) {
    fprintf (inferior_stderr, "  thread: 0x%lx\n", (long) thread_list[i]);
  }

  ret = vm_deallocate (task_self(), (vm_address_t) thread_list, 
		       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  fflush(inferior_stderr); /* just in case we are talking to a pipe */
}

#if !defined (__MACH30__)
void next_debug_exception (struct next_exception_data *e)
{
  char *s;
  inferior_debug (2, "exception for thread 0x%lx of task 0x%lx: "
		  "exception = 0x%lx, code = 0x%lx, subcode = 0x%lx\n",
		  (unsigned long) e->thread, 
		  (unsigned long) e->task,
		  (unsigned long) e->exception,
		  (unsigned long) e->code,
		  (unsigned long) e->subcode);
  s = mach_NeXT_exception_string (e->exception, e->code, e->subcode);
  inferior_debug (2, "exception for thread 0x%lx of task 0x%lx: %s\n",
		  (unsigned long) e->thread,
		  (unsigned long) e->task,
		  s);
}
#endif

void next_debug_message (msg_header_t *msg)
{
  if (! inferior_debug_flag) { return; }
  fprintf (inferior_stderr, "[%d inferior]: next_debug_message: message contents:\n", getpid ());
#if defined (__MACH30__)
    fprintf (inferior_stderr, "        msgh_bits: 0x%lx\n", (long) msg->msgh_bits);
    fprintf (inferior_stderr, "        msgh_size: 0x%lx\n", (long) msg->msgh_size);
    fprintf (inferior_stderr, " msgh_remote_port: 0x%lx\n", (long) msg->msgh_remote_port);
    fprintf (inferior_stderr, "  msgh_local_port: 0x%lx\n", (long) msg->msgh_local_port);
    fprintf (inferior_stderr, "    msgh_reserved: 0x%lx\n", (long) msg->msgh_reserved);
    fprintf (inferior_stderr, "          msgh_id: 0x%lx\n", (long) msg->msgh_id);
#else /* ! __MACH30__ */
  fprintf (inferior_stderr, "           id: 0x%lx\n", (long) msg->msg_id);
  fprintf (inferior_stderr, "   local port: 0x%lx\n", (long) msg->msg_local_port);
  fprintf (inferior_stderr, "  remote port: 0x%lx\n", (long) msg->msg_remote_port);
#endif /* __MACH30__ */
}

void next_debug_notification_message (struct next_inferior_status *inferior, msg_header_t *msg)
{
#if defined (__MACH30__)
    if (msg->msgh_id == MACH_NOTIFY_PORT_DELETED) {
      mach_port_deleted_notification_t *dmsg = (mach_port_deleted_notification_t *) msg;
      if (dmsg->not_port == inferior->task) {
	inferior_debug (2, "next_mach_process_message: deletion message for task port 0x%lx\n", 
			(unsigned long) dmsg->not_port);
      } else {
	inferior_debug (2, "next_mach_process_message: deletion message for unknown port 0x%lx; ignoring\n", 
			(unsigned long) dmsg->not_port);
      }
    } else {
      warning ("next_mach_process_message: unknown notification type 0x%lx; ignoring", 
	       (unsigned long) msg->msgh_id);
    }
#else /* ! __MACH30__ */
    if (msg->msg_id == NOTIFY_PORT_DELETED) {
      notification_t *dmsg = (notification_t *) msg;
      if (dmsg->notify_port == inferior->task) {
      } else {
	inferior_debug (2, "next_mach_process_message: deletion message for unknown port 0x%lx; ignoring\n", 
			(unsigned long) dmsg->notify_port);
      }
    } else {
      warning ("next_mach_process_message: unknown notification type 0x%lx; ignoring", 
	       (unsigned long) msg->msg_id);
    }
#endif /* __MACH30__ */
}

void 
_initialize_next_inferior_debug ()
{
  struct cmd_list_element *cmd;

  cmd = add_set_cmd ("debug-timestamps", class_obscure, var_boolean, 
		     (char *) &timestamps_debug_flag,
		     "Set if GDB print timestamps before any terminal output.",
		     &setlist);
  add_show_from_set (cmd, &showlist);	

  cmd = add_set_cmd ("debug-inferior", class_obscure, var_zinteger, 
		     (char *) &inferior_debug_flag,
		     "Set if printing inferior communication debugging statements.",
		     &setlist);
  add_show_from_set (cmd, &showlist);		
}  
