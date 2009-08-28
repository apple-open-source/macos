#include "server.h"
#include "target.h"

#include "macosx-threads.h"
#include "macosx-excthread.h"

#define get_process_thread(proc) ((struct thread_info *) \
				  find_inferior_id (&all_threads, \
				  get_process (proc)->tid))

/* The Linux side doesn't have a firm distinction between process and
   thread.  But on MacOS X there is a process that is separate from it's
   threads.  So I have a macosx_process_info - which is a singleton, and
   then a macosx_thread_info for each thread.  Since I'm using the inferiors.c
   code to manage all the threads, I include a backpointer to the macosx_process_info
   in the macosx_thread_info.  THat way I'm not just poking a global all the time.  */

struct macosx_process_info
{
  pid_t pid;
  struct macosx_exception_thread_status *status;
  /* If this is non-zero, it is a breakpoint to be reinserted at our next
     stop (SIGTRAP stops only).  */
  CORE_ADDR bp_reinsert;

  /* If this flag is set, the last continue operation on this process
     was a single-step.  */
  int stepping;
  thread_t thread_to_step;
  

  /* This is the thread that we stopped on.  */
  thread_t stopped_thread;

  /* FIXME: Not sure I'll need this yet...
     A link used when resuming.  It is initialized from the resume request,
     and then processed and cleared in macosx_resume_one_process.  */

  struct thread_resume *resume;
};

struct macosx_thread_info
{
  struct macosx_process_info *process;
  int suspend_count;
  thread_t app_thread_id;
};

struct macosx_target_ops
{
  void (*low_fetch_registers) (int regno);
  void (*low_store_registers) (int regno);
  void (*low_single_step_thread) (thread_t thread, int on);
  int (*low_clear_single_step) (thread_t thread);
};

/* Use this accessor to get the process info from a thread.  */
#define get_thread_process(thr) (((struct macosx_thread_info *) (inferior_target_data (thr)))->process)

extern struct macosx_target_ops the_low_target;

/* This originally comes from macosx-nat-mutils.h - though I moved it to
   macosx-tdep.h because it was needed for kdp...  */
#if (defined __GNUC__)
#define __MACH_CHECK_FUNCTION __PRETTY_FUNCTION__
#else
#define __MACH_CHECK_FUNCTION ((__const char *) 0)
#endif

#define MACH_PROPAGATE_ERROR(ret) \
{ MACH_WARN_ERROR(ret); if ((ret) != KERN_SUCCESS) { return ret; } }

#define MACH_CHECK_ERROR(ret) \
mach_check_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_WARN_ERROR(ret) \
mach_warn_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_ERROR_STRING(ret) \
(mach_error_string (ret) ? mach_error_string (ret) : "[UNKNOWN]")

void mach_check_error (kern_return_t ret, const char *file, unsigned int line,
                       const char *func);
void mach_warn_error (kern_return_t ret, const char *file, unsigned int line,
                      const char *func);


