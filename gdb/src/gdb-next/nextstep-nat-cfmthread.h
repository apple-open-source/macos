#ifndef _GDB_NEXTSTEP_NAT_CFMTHREAD_H_
#define _GDB_NEXTSTEP_NAT_CFMTHREAD_H_

#include "defs.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-threads.h"
#include "nextstep-nat-cfm.h"

#include <sys/wait.h>

struct next_cfm_thread_status
{
  CORE_ADDR notify_debugger;
  CORE_ADDR info_api_cookie;
  CORE_ADDR breakpoint_offset;
  struct cfm_parser parser;
};
typedef struct next_cfm_thread_status next_cfm_thread_status;

void next_cfm_thread_init (next_cfm_thread_status *s);
void next_cfm_thread_create (next_cfm_thread_status *s, task_t task);
void next_cfm_thread_destroy (next_cfm_thread_status *s);

#endif /* _GDB_NEXTSTEP_NAT_SIGTHREAD_H_*/
