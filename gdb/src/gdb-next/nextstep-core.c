#include "nextstep-nat-mutils.h"
#include "nextstep-nat-inferior.h"
#include "nextstep-threads.h"

#include "defs.h"
#include "inferior.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "gdbcore.h"

#import "SegmentManagerThreads.h"

#include <sys/time.h>

int current_core_thread = 0;
static struct target_ops core_ops;

extern void child_attach (char *name, int from_tty);
extern void child_create_inferior (char *exec, char *args, char **env);
extern RegionManager *regionManager;
extern void core_get_registers (int regnum);
extern CORE_ADDR getThreadPCAndNameFromState (REGS_STRUCT *regsState, char **pName);
extern char *getThreadNameFromState (USER_REG_STRUCT *userRegState, int n);

static void
core_cleanup ()
{
  if (regionManager) {
    [regionManager free];
    regionManager = nil;
  }
}

void
core_open (filename, from_tty)
     char *filename;
     int from_tty;
{
  char *temp;
  struct cleanup *old_chain;
  SegmentManager *newCM;

  if (filename == NULL) {
    error (regionManager
	   ? "No core file specified."
	   "(Use `detach' to stop debugging a core file.)"
	   : "No core file specified.");
  }
  filename = tilde_expand (filename);
  if (filename[0] != '/') {
    temp = concat (current_directory, "/", filename, NULL);
    free (filename);
    filename = temp;
  }

  old_chain = make_cleanup (free, filename);
  newCM = [SegmentManager newCore: filename];
  if (newCM == NULL) {
    error ("\"%s\" is not a core file.", filename);
  }
  if (! [newCM validate]) {
    error ("\"%s\" is an invalid core file.", filename);
  }
  discard_cleanups (old_chain);
  core_cleanup ();

  regionManager = (RegionManager *) newCM;
  old_chain = make_cleanup (core_cleanup, NULL);
  push_target (&core_ops);
  core_get_registers (-1);
  set_current_frame (create_new_frame (read_register(FP_REGNUM), read_pc ()));
  select_frame (get_current_frame (), 0);
  print_sel_frame (0);
#ifdef GDB414
  if (state_change_hook) {
      state_change_hook(STATE_INFERIOR_STOPPED);
  }
#endif /* GDB414 */
  discard_cleanups (old_chain);
}

void
core_detach (quitting)
{
  core_cleanup ();
}

int
have_core_file_p ()
{
  return regionManager != NULL;
}

static void
core_files_info ()
{
}

static int
core_xfer_memory (memaddr, myaddr, len, write)
     CORE_ADDR memaddr;
     char *myaddr;
     int len;
     int write;
{
  if (write) {
    error ("Can't write to core files.");
  }
  return [regionManager getDataAt: (void *)memaddr nbytes: len into: myaddr];
}

void
core_thread_list ()
{
  int numThreads, i;
  ThreadInfo *tInfos, *tInfo;
  SegmentManager *coreManager = (SegmentManager *)regionManager;
  char *name, *fName;
  CORE_ADDR pc;

  numThreads = [coreManager numThreads];
  if (numThreads == 1)
    printf_filtered("There is 1 thread.\n");
  else
    printf_filtered("There are %d threads.\n", numThreads);
  if (numThreads == 0)
    return;		/* MVS: nothing more to do (no threads to list).  */
  tInfos = [coreManager threadInfos];	/* Note: tInfos must be freed below */
  printf_filtered("Thread PC         Name            Function\n");         
  for (i = 0; i < numThreads; i++) {
    name = getThreadNameFromState(THREADINFO_NAME_STATE(tInfos[i]), i);
    pc = getThreadPCAndNameFromState(THREADINFO_PC_STATE(tInfos[i]), &fName);
    printf_filtered ("%-6d 0x%-8x %-15s %-40s\n", i, pc, name, fName);
  }
  free(tInfos);
}

void
core_thread_select (args, from_tty)
     char *args;
     int from_tty;
{
  int numThreads, numThread;
  SegmentManager *coreManager = (SegmentManager *)regionManager;

  if (!args)
    error_no_arg ("thread index to select");
  numThreads = [coreManager numThreads];
  numThread = atoi(args);
  if ((numThreads < numThreads) || (numThreads <= numThread))
    error("Invalid thread.\n");
  else
    current_core_thread = numThread;
}

static struct target_ops core_ops = {
    "core",			/* to_shortname */
    "Local core dump file",		/* to_longname */
    "Use a core file as a target.\n"
    "Specify the filename of the core file.",	/* to_doc */
    core_open,			/* to_open */
    core_detach,		/* to_close */
    0,                          /* to_attach */
    core_detach, 		/* to_detach */
    0,				/* to_resume */
    0,				/* to_wait */
    core_get_registers,				/* to_fetch_registers */
    0,				/* to_store_registers */
    0,				/* to_prepare_to_store */
    core_xfer_memory,		/* to_xfer_memory */
    core_files_info,				/* to_files_info */
    0,				/* to_insert_breakpoint */
    0,				/* to_remove_breakpoint */
    0,				/* to_terminal_init */
    0, 				/* to_terminal_inferior */
    0,				/* to_terminal_ours_for_output */
    0,				/* to_terminal_ours */
    0,				/* to_terminal_info */
    0,				/* to_kill */
    0,				/* to_load */
    0,				/* to_lookup_symbol */
    find_default_create_inferior,	/* to_create_inferior */
    0,				/* to_mourn_inferior */
    0,				/* to_can_run */
    0, 				/* to_notice_signals */
    0,				/* to_thread_alive */
    0,   			/* to_stop */
    core_stratum,		/* to_stratum */
    0,				/* DONT_USE (formerly to_next) */
    0,				/* to_has_all_memory */
    1,				/* to_has_memory */
    1,				/* to_has_stack */
    1,				/* to_has_registers */
    0,				/* to_has_execution */
    0,				/* sections */
    0,				/* sections_end */
    OPS_MAGIC			/* to_magic */
};


void (*exec_file_display_hook) () = NULL;

void
_initialize_next_core ()
{
  add_target (&core_ops);
}
