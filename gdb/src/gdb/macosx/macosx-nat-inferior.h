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
#endif                          /* WITH_CFM */
};
typedef struct macosx_inferior_status macosx_inferior_status;

struct private_thread_info
{
  thread_t app_thread_port;
  void* core_thread_state;
  int gdb_suspend_count;
  int gdb_dont_suspend_stepping;
};

void macosx_check_new_threads (thread_array_t thread_list, unsigned int nthreads);
ptid_t macosx_wait (struct macosx_inferior_status *inferior,
                    struct target_waitstatus *status,
                    gdb_client_data client_data);

extern int inferior_bind_exception_port_flag;
extern int inferior_bind_notify_port_flag;

/* from rhapsody-nat.c and macosx-nat.c */

void macosx_create_inferior_for_task (struct macosx_inferior_status *inferior,
                                      task_t task, int pid);

void macosx_fetch_task_info (struct kinfo_proc ** info, size_t * count);

char **macosx_process_completer (char *text, char *word);

int create_private_thread_info (struct thread_info *thrd_info);
void delete_private_thread_info (struct thread_info *thrd_info);
int create_core_thread_state_cache (struct thread_info *thrd_info);
void delete_core_thread_state_cache (struct thread_info *thrd_info);

/* This should probably go in a separate machoread.h, but since it's
   only one function, I'll wait on that:  */
void
macho_calculate_offsets_for_dsym (struct objfile *main_objfile,
				  bfd *sym_bfd,
				  struct section_addr_info *addrs,
				  struct section_offsets *in_offsets,
				  int in_num_offsets,
				  struct section_offsets **sym_offsets,
				  int *sym_num_offsets);

/* This one is called in macosx-nat-inferior.c, but needs to be provided by the
   platform specific nat code.  It allows each platform to add platform specific
   stuff to the macosx_child_target.  */
void macosx_complete_child_target (struct target_ops *target);
int macosx_get_task_for_pid_rights (void);
#endif /* __GDB_MACOSX_NAT_INFERIOR_H__ */
