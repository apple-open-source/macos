#ifndef __GDB_MACOSX_NAT_INFERIOR_H__
#define __GDB_MACOSX_NAT_INFERIOR_H__

#include "macosx-nat-threads.h"
#include "macosx-nat-sigthread.h"
#include "macosx-nat-excthread.h"
#include "macosx-nat-dyld.h"
#include "macosx-nat-dyld-info.h"

#if WITH_CFM
#include "macosx-nat-cfmthread.h"
#endif /* WITH_CFM */

#include "defs.h"
#include "target.h"

#include <mach/mach.h>

struct kinfo_proc;

struct macosx_inferior_status
{
  int pid;
  task_t task;

  int attached_in_ptrace;
  int stopped_in_ptrace;
  int stopped_in_softexc;

  unsigned int suspend_count;

  thread_t last_thread;

  macosx_signal_thread_status signal_status;
  macosx_exception_thread_status exception_status;
#if WITH_CFM
  macosx_cfm_thread_status cfm_status;
#endif /* WITH_CFM */
  macosx_dyld_thread_status dyld_status;
};
typedef struct macosx_inferior_status macosx_inferior_status;

struct private_thread_info
{
  thread_t app_thread_port;
};

void macosx_check_new_threads ();
ptid_t macosx_wait (struct macosx_inferior_status *inferior, 
                  struct target_waitstatus *status,
		  gdb_client_data client_data);

extern int inferior_bind_exception_port_flag;
extern int inferior_bind_notify_port_flag;
extern int inferior_handle_exceptions_flag;

/* from rhapsody-nat.c and macosx-nat.c */

void macosx_create_inferior_for_task
  PARAMS ((struct macosx_inferior_status *inferior, task_t task, int pid));

void macosx_fetch_task_info PARAMS ((struct kinfo_proc **info, size_t *count));

char **macosx_process_completer PARAMS ((char *text, char *word));

#endif /* __GDB_MACOSX_NAT_INFERIOR_H__ */
