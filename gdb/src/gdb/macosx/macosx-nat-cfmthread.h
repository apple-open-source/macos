#ifndef __GDB_MACOSX_NAT_CFMTHREAD_H__
#define __GDB_MACOSX_NAT_CFMTHREAD_H__

#include "defs.h"
#include "macosx-nat-mutils.h"
#include "macosx-nat-threads.h"
#include "macosx-nat-cfm.h"

#include <sys/wait.h>

struct macosx_cfm_thread_status
{
  CORE_ADDR notify_debugger;
  CORE_ADDR info_api_cookie;
  CORE_ADDR breakpoint_offset;
  struct cfm_parser parser;
};
typedef struct macosx_cfm_thread_status macosx_cfm_thread_status;

void macosx_cfm_thread_init (macosx_cfm_thread_status *s);
void macosx_cfm_thread_create (macosx_cfm_thread_status *s, task_t task);
void macosx_cfm_thread_destroy (macosx_cfm_thread_status *s);

#endif /* __GDB_MACOSX_NAT_CFMTHREAD_H__ */
