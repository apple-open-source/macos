#include <sys/param.h>
#include <sys/sysctl.h>

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "gdbcore.h"
#include "value.h"

#include "nextstep-nat-mutils.h"
#include "nextstep-nat-inferior.h"

extern next_inferior_status *next_status;

#define CHECK_ARGS(what, args) \
{ if ((NULL == args) || ((args[0] != '0') && (args[1] != 'x'))) error(what" must be specified with 0x..."); }

#define PRINT_FIELD(structure, field) \
printf_unfiltered(#field":\t%#x\n", (structure)->field)

#if defined (__MACH30__)
#define task_self mach_task_self
#define port_names mach_port_names
#define task_by_unix_pid task_for_pid
#define port_name_array_t mach_port_array_t
#define port_type_array_t mach_port_array_t
#endif

static void
info_mach_tasks_command(char* args, int from_tty)
{
    int	sysControl[4];
    int	count, index;
    size_t length;
    struct kinfo_proc* procInfo;
    
    sysControl[0] = CTL_KERN;
    sysControl[1] = KERN_PROC;
    sysControl[2] = KERN_PROC_ALL;

    sysctl(sysControl, 3, NULL, &length, NULL, 0);
    procInfo = (struct kinfo_proc*) malloc(length);
    sysctl(sysControl, 3, procInfo, &length, NULL, 0);

    count = (length / sizeof(struct kinfo_proc));
    printf_unfiltered("%d processes:\n", count);
    for (index = 0; index < count; ++index)
    {
        kern_return_t result;
        mach_port_t taskPort;
        
        result = task_by_unix_pid(mach_task_self(), procInfo[index].kp_proc.p_pid, &taskPort);
        if (KERN_SUCCESS == result)
        {
            printf_unfiltered("    %s is %d has task %#x\n",
                              procInfo[index].kp_proc.p_comm,
                              procInfo[index].kp_proc.p_pid,
                              taskPort);
        }
        else
        {
           printf_unfiltered("    %s is %d unknown task port\n",
                              procInfo[index].kp_proc.p_comm,
                              procInfo[index].kp_proc.p_pid);
        }
    }

    free(procInfo);
}

static void
info_mach_task_command(char* args, int from_tty)
{
    union {
        struct task_basic_info		basic;
        struct task_events_info		events;
        struct task_thread_times_info	thread_times;
    } task_info_data;

    kern_return_t result;
    unsigned int info_count;
    task_t task;

    CHECK_ARGS("Task", args);
    sscanf(args, "0x%x", &task);

    printf_unfiltered("TASK_BASIC_INFO:\n");
    info_count = TASK_BASIC_INFO_COUNT;
    result = task_info(task,
                       TASK_BASIC_INFO,
                       (task_info_t) &task_info_data.basic,
                       &info_count);
    MACH_CHECK_ERROR(result);

    PRINT_FIELD(&task_info_data.basic, suspend_count);
#if !defined (__MACH30__)    
    PRINT_FIELD(&task_info_data.basic, base_priority);
#endif    
    PRINT_FIELD(&task_info_data.basic, virtual_size);
    PRINT_FIELD(&task_info_data.basic, resident_size);
    PRINT_FIELD(&task_info_data.basic, user_time);
    PRINT_FIELD(&task_info_data.basic, system_time);
#if 0
    printf_unfiltered("\nTASK_EVENTS_INFO:\n");
    info_count = TASK_EVENTS_INFO_COUNT;
    result = task_info(task,
                       TASK_EVENTS_INFO,
                       (task_info_t) &task_info_data.events,
                       &info_count);
    MACH_CHECK_ERROR(result);

    PRINT_FIELD(&task_info_data.events, faults);
    PRINT_FIELD(&task_info_data.events, zero_fills);
    PRINT_FIELD(&task_info_data.events, reactivations);
    PRINT_FIELD(&task_info_data.events, pageins);
    PRINT_FIELD(&task_info_data.events, cow_faults);
    PRINT_FIELD(&task_info_data.events, messages_sent);
    PRINT_FIELD(&task_info_data.events, messages_received);
#endif
    printf_unfiltered("\nTASK_THREAD_TIMES_INFO:\n");
    info_count = TASK_THREAD_TIMES_INFO_COUNT;
    result = task_info(task,
                       TASK_THREAD_TIMES_INFO,
                       (task_info_t) &task_info_data.thread_times,
                       &info_count);
    MACH_CHECK_ERROR(result);

    PRINT_FIELD(&task_info_data.thread_times, user_time);
    PRINT_FIELD(&task_info_data.thread_times, system_time);
}

static void
info_mach_ports_command(char* args, int from_tty)
{
    port_name_array_t	port_names_data;
    port_type_array_t	port_types_data;
    unsigned int	name_count, type_count;
    kern_return_t	result;
    int			index;
    task_t		task;

    CHECK_ARGS("Task", args);
    sscanf(args, "0x%x", &task);

    result = port_names(task,
                        &port_names_data,
                        &name_count,
                        &port_types_data,
                        &type_count);
    MACH_CHECK_ERROR(result);

    CHECK_FATAL(name_count == type_count);

    printf_unfiltered("Ports for task %#x:\n", task);
    for (index = 0; index < name_count; ++index)
      {
        printf_unfiltered("port name: %#x, type %#x\n",
                          port_names_data[index], port_types_data[index]);
      }

    vm_deallocate(task_self(), port_names_data, (name_count * sizeof(port_t)));
    vm_deallocate(task_self(), port_types_data, (type_count * sizeof(port_type_t)));
}

static void
info_mach_port_command (char* args, int from_tty)
{
    task_t task;
    port_t port;

    CHECK_ARGS ("Task and port", args);
    sscanf (args, "0x%x 0x%x", &task, &port);

    next_debug_port_info (task, port);
}

static void
info_mach_threads_command(char* args, int from_tty)
{
    thread_array_t	thread_array;
    unsigned int	thread_count;
    kern_return_t	result;
    task_t		task;
    int			i;

    CHECK_ARGS("Task", args);
    sscanf(args, "0x%x", &task);

    result = task_threads(task,
                          &thread_array,
                          &thread_count);
    MACH_CHECK_ERROR(result);

    printf_unfiltered("Threads in task %#x:\n", task);
    for (i = 0; i < thread_count; ++i)
      {
        printf_unfiltered("    %#x\n", thread_array[i]);
      }
    
    vm_deallocate(task_self(), thread_array, (thread_count * sizeof(thread_t)));
}

static void
info_mach_thread_command(char* args, int from_tty)
{
    union {
        struct thread_basic_info	basic;
    } thread_info_data;

    thread_t thread;
    kern_return_t result;
    unsigned int info_count;

    CHECK_ARGS("Thread", args);
    sscanf(args, "0x%x", &thread);

    printf_unfiltered("THREAD_BASIC_INFO\n");
    info_count = THREAD_BASIC_INFO_COUNT;
    result = thread_info(thread,
                         THREAD_BASIC_INFO,
                         (thread_info_t) &thread_info_data.basic,
                         &info_count);
    MACH_CHECK_ERROR(result);

    PRINT_FIELD(&thread_info_data.basic, user_time);
    PRINT_FIELD(&thread_info_data.basic, system_time);
    PRINT_FIELD(&thread_info_data.basic, cpu_usage);
#if !defined(__MACH30__)    
    PRINT_FIELD(&thread_info_data.basic, base_priority);
    PRINT_FIELD(&thread_info_data.basic, cur_priority);
#endif    
    PRINT_FIELD(&thread_info_data.basic, run_state);
    PRINT_FIELD(&thread_info_data.basic, flags);
    PRINT_FIELD(&thread_info_data.basic, suspend_count);
    PRINT_FIELD(&thread_info_data.basic, sleep_time);

#ifdef __ppc__
    {
      union {
	struct ppc_thread_state	thread;
	struct ppc_exception_state	exception;
      } thread_state;
      int register_count, i;
      unsigned int* register_data;

      info_count = PPC_THREAD_STATE_COUNT;
      result = thread_get_state(thread,
				PPC_THREAD_STATE,
				(thread_state_t) &thread_state.thread,
				&info_count);
      MACH_CHECK_ERROR(result);

      printf_unfiltered("\nPPC_THREAD_STATE \n");
      register_data = &thread_state.thread.r0;
      register_count = 0;
      for (i = 0; i < 8; ++i)
	{
	  printf_unfiltered("r%02d: 0x%08x    r%02d: 0x%08x    r%02d: 0x%08x    r%02d: 0x%08x\n",
			    register_count++, *register_data++,
			    register_count++, *register_data++,
			    register_count++, *register_data++,
			    register_count++, *register_data++);
	}

      printf_unfiltered("srr0: 0x%08x    srr1: 0x%08x\n",
			thread_state.thread.srr0, thread_state.thread.srr1);
      printf_unfiltered("cr:   0x%08x    xer:  0x%08x\n",
			thread_state.thread.cr, thread_state.thread.xer);
      printf_unfiltered("lr:   0x%08x    ctr:  0x%08x\n",
			thread_state.thread.lr, thread_state.thread.ctr);
    }
#endif
}

void info_mach_regions_command (char *exp, int from_tty)
{
  if ((! next_status) || (next_status->task == TASK_NULL)) {
    error ("Inferior not available");
  }

  next_debug_regions (next_status->task);
}

void info_mach_region_command (char *exp, int from_tty)
{
  struct expression *expr;
  struct value *val;
  
  vm_address_t address;

  expr = parse_expression (exp);
  val = evaluate_expression (expr);
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_REF) {
    val = value_ind (val);
  }
  /* In rvalue contexts, such as this, functions are coerced into
     pointers to functions. */
  if (TYPE_CODE (VALUE_TYPE (val)) == TYPE_CODE_FUNC
      && VALUE_LVAL (val) == lval_memory) {
    address = VALUE_ADDRESS (val);
  } else {
    address = value_as_pointer (val);
  }
  
  if ((! next_status) || (next_status->task == TASK_NULL)) {
    error ("Inferior not available");
  }

  next_debug_region (next_status->task, address);
}

void
_initialize_next_info_commands (void)
{
  add_info ("mach-tasks", info_mach_tasks_command, "Get list of tasks in system.");
  add_info ("mach-ports", info_mach_ports_command, "Get list of ports in a task.");
  add_info ("mach-port", info_mach_port_command, "Get info on a specific port.");
  add_info ("mach-task", info_mach_task_command, "Get info on a specific task.");
  add_info ("mach-threads", info_mach_threads_command, "Get list of threads in a task.");
  add_info ("mach-thread", info_mach_thread_command, "Get info on a specific thread.");

  add_info ("mach-regions", info_mach_regions_command, "Get information on all mach region for the current inferior.");
  add_info ("mach-region", info_mach_region_command, "Get information on mach region at given address.");
}
