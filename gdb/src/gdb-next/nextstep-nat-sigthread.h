#ifndef _GDB_NEXTSTEP_NAT_SIGTHREAD_H_
#define _GDB_NEXTSTEP_NAT_SIGTHREAD_H_

#include "defs.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-threads.h"

#include <sys/wait.h>

typedef int WAITSTATUS;

struct next_signal_thread_message
{
  int pid;
  WAITSTATUS status;
};

struct next_signal_thread_status
{
  gdb_thread_t signal_thread;

  int transmit_fd;
  int receive_fd;

  int inferior_pid;
};

typedef struct next_signal_thread_message next_signal_thread_message;
typedef struct next_signal_thread_status next_signal_thread_status;

void next_signal_thread_debug (FILE *f, struct next_signal_thread_status *s);
void next_signal_thread_debug_status (FILE *f, WAITSTATUS status);

void next_signal_thread_init (next_signal_thread_status *s);

void next_signal_thread_create (next_signal_thread_status *s, int pid);
void next_signal_thread_destroy (next_signal_thread_status *s);

#endif /* _GDB_NEXTSTEP_NAT_SIGTHREAD_H_*/
