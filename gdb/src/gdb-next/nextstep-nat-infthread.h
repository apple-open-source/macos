#ifndef __NEXTSTEP_NAT_INFTHREADS_H__
#define __NEXTSTEP_NAT_INFTHREADS_H__

struct next_inferior_status;

void set_trace_bit PARAMS ((thread_t thread));
void clear_trace_bit PARAMS ((thread_t thread));
void clear_suspend_count PARAMS ((thread_t thread));

void prepare_threads_before_run 
  PARAMS ((struct next_inferior_status *inferior, int step, thread_t current, int stop_others));

void prepare_threads_after_stop PARAMS ((struct next_inferior_status *inferior));

char *unparse_run_state PARAMS ((int run_state));

void print_thread_info PARAMS ((thread_t tid));

void info_task_command PARAMS ((char *args, int from_tty));
void info_thread_command PARAMS ((char *tidstr, int from_tty));

#endif /* __NEXTSTEP_NAT_INFTHREADS_H__ */
