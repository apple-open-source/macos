#ifndef _GDB_NEXTSTEP_NAT_EXCTHREAD_H_
#define _GDB_NEXTSTEP_NAT_EXCTHREAD_H_

#include "defs.h"
#include "nextstep-nat-mutils.h"
#include "nextstep-nat-threads.h"

#include <sys/wait.h>

struct next_exception_info
{
  exception_mask_t masks[EXC_TYPES_COUNT];
  mach_port_t ports[EXC_TYPES_COUNT];
  exception_behavior_t behaviors[EXC_TYPES_COUNT];
  thread_state_flavor_t flavors[EXC_TYPES_COUNT];
  mach_msg_type_number_t count;
};
typedef struct next_exception_info next_exception_info;

struct next_exception_thread_status
{
  gdb_thread_t exception_thread;

  int transmit_fd;
  int receive_fd;

  mach_port_t inferior_exception_port;

  next_exception_info saved_exceptions;
  next_exception_info saved_exceptions_step;
  int saved_exceptions_stepping;
};
typedef struct next_exception_thread_status next_exception_thread_status;

struct next_exception_thread_message
{
  task_t task_port;
  thread_t thread_port;
  exception_type_t exception_type;
  exception_data_t exception_data;
  mach_msg_type_number_t data_count;
};
typedef struct next_exception_thread_message next_exception_thread_message;

void next_exception_thread_init (next_exception_thread_status *s);

void next_exception_thread_create (next_exception_thread_status *s, task_t task);
void next_exception_thread_destroy (next_exception_thread_status *s);

#endif /* _GDB_NEXTSTEP_NAT_SIGTHREAD_H_*/
