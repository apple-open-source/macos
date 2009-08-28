#ifndef __GDB_MACOSX_NAT_INFTHREAD_H__
#define __GDB_MACOSX_NAT_INFTHREAD_H__

struct macosx_inferior_status;

void set_trace_bit (thread_t thread);
void clear_trace_bit (thread_t thread);
void clear_suspend_count (thread_t thread);

void prepare_threads_before_run (struct macosx_inferior_status *inferior,
                                 int step, thread_t current, int stop_others);

void prepare_threads_after_stop (struct macosx_inferior_status *inferior);

char *unparse_run_state (int run_state);

void macosx_setup_registers_before_hand_call (void);

void info_task_command (char *args, int from_tty);
void info_thread_command (char *tidstr, int from_tty);
thread_t get_application_thread_port (thread_t our_name);

void macosx_prune_threads (thread_array_t thread_list, unsigned int nthreads);

void macosx_print_thread_details (struct ui_out *uiout, ptid_t ptid);

#endif /* __GDB_MACOSX_NAT_INFTHREAD_H__ */
