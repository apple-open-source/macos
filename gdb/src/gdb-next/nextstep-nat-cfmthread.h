#ifndef _GDB_NEXTSTEP_NAT_CFMTHREAD_H_
#define _GDB_NEXTSTEP_NAT_CFMTHREAD_H_

#include "CodeFragmentInfoPriv.h"

#include "defs.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-threads.h"

#include <sys/wait.h>

struct next_cfm_thread_status
{
  void *info_api_cookie;

  mach_port_t cfm_receive_right;
  mach_port_t cfm_send_right;

  int transmit_fd;
  int receive_fd;

  gdb_thread_t cfm_thread;
};
typedef struct next_cfm_thread_status next_cfm_thread_status;

struct next_cfm_message
{
  msg_header_t header;
  struct CFragNotifyInfo data;
};
typedef struct next_cfm_message next_cfm_message;

struct next_cfm_thread_message
{
  struct CFragNotifyInfo data;
};
typedef struct next_cfm_thread_message next_cfm_thread_message;

void next_cfm_thread_init (next_cfm_thread_status *s);

void next_cfm_thread_create (next_cfm_thread_status *s, task_t task);
void next_cfm_thread_destroy (next_cfm_thread_status *s);

#endif /* _GDB_NEXTSTEP_NAT_SIGTHREAD_H_*/
