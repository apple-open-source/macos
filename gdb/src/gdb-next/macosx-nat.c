#include <stdio.h>

#include "defs.h"
#include "target.h"
#include "obstack.h"

#include "nextstep-nat-inferior.h"
#include "nextstep-nat-inferior-util.h"
#include "nextstep-nat-inferior-debug.h"
#include "nextstep-nat-mutils.h"

static struct target_waitstatus *exception_status = NULL;
static struct next_inferior_status *exception_inferior = NULL;

static kern_return_t forward_exception
(mach_port_t thread_port, mach_port_t task_port,
 exception_type_t exception_type, exception_data_t exception_data, mach_msg_type_number_t data_count)
{
  kern_return_t kret;
  int original_port_index = 0;

  mach_port_t port;
  exception_behavior_t behavior;
  thread_state_flavor_t flavor;    

  thread_state_data_t thread_state;
  mach_msg_type_number_t thread_state_count;

  struct next_inferior_status *inferior = exception_inferior;

  CHECK_FATAL (inferior != NULL);
  CHECK_FATAL ((inferior->saved_exceptions.masks[original_port_index] & (1 << exception_type)) != 0);

  port = inferior->saved_exceptions.ports[original_port_index];
  behavior = inferior->saved_exceptions.behaviors[original_port_index];
  flavor = inferior->saved_exceptions.flavors[original_port_index];

  if (behavior != EXCEPTION_DEFAULT) {
    thread_state_count = THREAD_STATE_MAX;
    kret = thread_get_state (thread_port, flavor, thread_state, &thread_state_count);
    MACH_CHECK_ERROR (kret);
  }

  switch (behavior) {

  case EXCEPTION_DEFAULT:
    inferior_debug (2, "forwarding to exception_raise\n");
    kret = exception_raise
      (port, thread_port, task_port, exception_type, exception_data, data_count);
    MACH_CHECK_ERROR (kret);
    break;
  
  case EXCEPTION_STATE:
    inferior_debug (2, "forwarding to exception_raise_state\n");
    kret = exception_raise_state
      (port, exception_type, exception_data, data_count, &flavor,
       thread_state, thread_state_count, thread_state, &thread_state_count);
    MACH_CHECK_ERROR (kret);
    break;

  case EXCEPTION_STATE_IDENTITY:
    inferior_debug (2, "forwarding to exception_raise_state_identity\n");
    kret = exception_raise_state_identity
      (port, thread_port, task_port, exception_type, exception_data,  data_count,
       &flavor, thread_state, thread_state_count, thread_state,  &thread_state_count);
    MACH_CHECK_ERROR (kret);
    break;

  default:
    inferior_debug (2, "forward_exception got unknown behavior\n");
    break;
  }

  if (behavior != EXCEPTION_DEFAULT) {
    kret = thread_set_state (thread_port, flavor, thread_state, thread_state_count);
    MACH_CHECK_ERROR (kret);
  }

  return KERN_SUCCESS;
}

kern_return_t catch_exception_raise_state
(mach_port_t port,
 exception_type_t exception_type, exception_data_t exception_data, mach_msg_type_number_t data_count,
 thread_state_flavor_t *state_flavor,
 thread_state_t in_state, mach_msg_type_number_t in_state_count,
 thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  inferior_debug (2, "catch_exception_raise_state called\n");
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise_state_identity
(mach_port_t port, mach_port_t thread_port, mach_port_t task_port,
 exception_type_t exception_type, exception_data_t exception_data, mach_msg_type_number_t data_count,
 thread_state_flavor_t *state_flavor,
 thread_state_t in_state, mach_msg_type_number_t in_state_count,
 thread_state_t out_state, mach_msg_type_number_t out_state_count)
{
  kern_return_t kret;

  inferior_debug (2, "catch_exception_raise_state_identity called\n");

  kret = mach_port_deallocate (mach_task_self(), task_port);
  MACH_CHECK_ERROR (kret);
  kret = mach_port_deallocate (mach_task_self(), thread_port);
  MACH_CHECK_ERROR (kret);
  return KERN_FAILURE;
}

kern_return_t catch_exception_raise
(mach_port_t port, mach_port_t thread_port, mach_port_t task_port, 
 exception_type_t exception_type, exception_data_t exception_data,
 mach_msg_type_number_t data_count)
{
  kern_return_t kret, kret2;
  int i;

  struct next_inferior_status *inferior = exception_inferior;

  CHECK_FATAL (inferior != NULL);

  inferior_debug (2, "catch_exception_raise called\n");
  inferior_debug (2, "                  port: 0x%lx\n", port);
  inferior_debug (2, "           thread_port: 0x%lx\n", thread_port);
  inferior_debug (2, "             task_port: 0x%lx\n", task_port);
  inferior_debug (2, "        exception_type: 0x%lx (%s)\n", exception_type, unparse_exception_type (exception_type));
  inferior_debug (2, "            data_count: 0x%lx\n", data_count);

  for (i = 0; i < data_count; ++i) {
    inferior_debug (2, "   exception_data[%d]: 0x%lx\n", i, exception_data[i]);
  }

  if (task_port != inferior->task) {
    inferior_debug (2, "ignoring exception forwarded from subprocess\n");
    kret = forward_exception (thread_port, task_port, exception_type, exception_data, data_count);
    return kret;
  }

  inferior->last_thread = thread_port;

  if (inferior_handle_exceptions_flag) {
    
    kret = next_inferior_suspend_mach (inferior);
    MACH_CHECK_ERROR (kret);

    next_mach_check_new_threads ();

    prepare_threads_after_stop (inferior);

    CHECK_FATAL (exception_status != NULL);
    exception_status->kind = TARGET_WAITKIND_STOPPED;

    switch (exception_type) {
    case EXC_BAD_ACCESS:
      exception_status->value.sig = TARGET_EXC_BAD_ACCESS;
      break;
    case EXC_BAD_INSTRUCTION:
      exception_status->value.sig = TARGET_EXC_BAD_INSTRUCTION;
      break;
    case EXC_ARITHMETIC:
      exception_status->value.sig = TARGET_EXC_ARITHMETIC;
      break;
    case EXC_EMULATION:
      exception_status->value.sig = TARGET_EXC_EMULATION;
      break;
    case EXC_SOFTWARE:
      exception_status->value.sig = TARGET_EXC_SOFTWARE;
      break;
    case EXC_BREAKPOINT:
#if 0
      /* Many internal GDB routines expect breakpoints to be reported
         as TARGET_SIGNAL_TRAP, and will report TARGET_EXC_BREAKPOINT
         as a spurious signal. */
      exception_status->value.sig = TARGET_EXC_BREAKPOINT;
#endif /* 0 */
      exception_status->value.sig = TARGET_SIGNAL_TRAP;
      break;
    default:
      exception_status->value.sig = TARGET_SIGNAL_UNKNOWN;
      break;
    }
        
    kret = KERN_SUCCESS;

  } else {
    kret = forward_exception (thread_port, task_port, exception_type, exception_data, data_count);
  }
    
  kret2 = mach_port_deallocate (mach_task_self(), task_port);
  MACH_CHECK_ERROR (kret2);
  kret2 = mach_port_deallocate (mach_task_self(), thread_port);
  MACH_CHECK_ERROR (kret2);

  return kret;
}

void next_handle_exception 
(struct next_inferior_status *inferior, msg_header_t *message, struct target_waitstatus *status)
{
  char reply_buffer[1024];
  mach_msg_header_t *reply = (mach_msg_header_t *) reply_buffer;
  boolean_t server_result;
  kern_return_t kret;

  exception_status = status;
  exception_inferior = inferior;

  server_result = exc_server (message, reply);
  
  exception_status = NULL;
  exception_inferior = NULL;

  kret = mach_msg
    (reply, (MACH_SEND_MSG | MACH_MSG_OPTION_NONE),
     reply->msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK_ERROR (kret);
}

void next_create_inferior_for_task
(struct next_inferior_status *inferior, task_t task, int pid)
{
  kern_return_t ret;

  CHECK_FATAL (inferior != NULL);

  next_inferior_destroy (inferior);
  next_inferior_reset (inferior);

  inferior->task = task;
  inferior->pid = pid;

  inferior->attached_in_ptrace = 0;
  inferior->stopped_in_ptrace = 0;
  inferior->suspend_count = 0;

  /* get notification messages for current task */

  ret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &inferior->notify_port);
  MACH_CHECK_ERROR (ret);
  ret = mach_port_insert_right
    (mach_task_self(), inferior->notify_port, inferior->notify_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (ret);

  if (inferior_bind_notify_port_flag) {
    mach_port_t previous_notify_port;
    ret = mach_port_request_notification
      (mach_task_self(), inferior->task, MACH_NOTIFY_DEAD_NAME,
       1, inferior->notify_port, MACH_MSG_TYPE_MAKE_SEND, &previous_notify_port);
    MACH_CHECK_ERROR (ret);
    ret = mach_port_deallocate (mach_task_self(), previous_notify_port);
    MACH_CHECK_ERROR (ret);
    }
    
  /* initialize signal port */

  ret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &inferior->signal_port);
  MACH_CHECK_ERROR (ret);
  ret = mach_port_insert_right (mach_task_self(), inferior->signal_port,
				inferior->signal_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR(ret);
  
  /* initialize dyld port */

  ret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &inferior->dyld_port);
  MACH_CHECK_ERROR (ret);
  ret = mach_port_insert_right (mach_task_self(), inferior->dyld_port,
				inferior->dyld_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (ret);

  /* initialize gdb exception port */

  ret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &inferior->exception_port);
  MACH_CHECK_ERROR (ret);
  ret = mach_port_insert_right (mach_task_self(), inferior->exception_port,
				inferior->exception_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (ret);

  ret = mach_port_allocate (mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &inferior->exception_reply_port);
  MACH_CHECK_ERROR (ret);
  ret = mach_port_insert_right (mach_task_self(), inferior->exception_reply_port,
				inferior->exception_reply_port, MACH_MSG_TYPE_MAKE_SEND);
  MACH_CHECK_ERROR (ret);

  /* commandeer inferior exception port */

  if (inferior_bind_exception_port_flag) {

    next_save_exception_ports (inferior->task, &inferior->saved_exceptions);

    ret = task_set_exception_ports
      (inferior->task,
       EXC_MASK_ALL & ~(EXC_MASK_MACH_SYSCALL | EXC_MASK_SYSCALL | EXC_MASK_RPC_ALERT | EXC_MASK_SOFTWARE),
       inferior->exception_port, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    MACH_CHECK_ERROR (ret);
  }

  inferior->last_thread = next_primary_thread_of_task (inferior->task);
}
