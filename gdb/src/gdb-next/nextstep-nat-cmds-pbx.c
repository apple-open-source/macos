#include "defs.h"
#include "symtab.h"
#include "symfile.h"
#include "target.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "gdbthread.h"
#include "frame.h"

#include <ctype.h>
#include <string.h>

#ifdef NOT_YET

void pbx_switch_to_thread_number (int thread_number)
{
    extern void switch_to_thread (int); /* from thread.c */

    int pid = thread_id_to_pid(thread_number);
    if (pid >= 0)
        switch_to_thread(pid);
}

void pbx_switch_to_frame_number (int frame_number)
{
    int level = frame_number;
    struct frame_info *frame;

    frame = find_relative_frame (get_current_frame (), &level);

    if (frame) {
        select_frame (frame, frame_number);
    }
}

/* 
  state to save when restoring context or handling an error
 */

typedef struct {
    int		thread_number;
    int		frame_number;
    void 	(*frame_changed_hook) PARAMS ((int new_frame_number));
    void 	(*thread_changed_hook) PARAMS ((int new_thread_number));
} PBXContextState;


void
pbx_restore_context_state(PBXContextState *sp)
{
    if (sp->thread_number >= 0) {
        pbx_switch_to_thread_number(sp->thread_number);
    }

    if (sp->frame_number >= 0) {
        pbx_switch_to_frame_number(sp->frame_number);
    }

    if (sp->thread_changed_hook) {
        thread_changed_hook = sp->thread_changed_hook;
    }

    if (sp->frame_changed_hook) {
        frame_changed_hook = sp->frame_changed_hook;
    }
}

/*
 * Perform a command within the context of the given thread and stack frame.
 * If the stackframe number is < 0, then ignore the stack frame.
 *
 * e.g.
 *	pb_command_with_context 2 20 print x
 * prints x at frame 20 in thread 2
 */


void pbx_command_with_context_command (char *arg, int from_tty)
{
    int			thread_number = -1;
    int			frame_number = -1;
    char		*cmd = NULL;
    char		*cp;
    PBXContextState	saved_state;
    struct cleanup 	*old_chain;

    /* find thread id */
    while((*arg != '\000') && isblank(*arg))
        arg++;

    if (*arg) {
        thread_number = strtol(arg, &cp, 10);
        arg = cp;
    }
    
    /* find stack frame number */
    while((*arg != '\000') && isblank(*arg))
        arg++;

    if ((*arg != '\000')) {
        frame_number = strtol(arg, &cp, 10);
        arg = cp;
    }
    
    /* find start of command */
    while((*arg != '\000') && isblank(*arg))
        arg++;

    if ((*arg != '\000'))
        cmd = arg;

    /* save current context */
    if (thread_number >= 0) {
        extern int inferior_pid;

        saved_state.thread_number = pid_to_thread_id(inferior_pid);
        saved_state.thread_changed_hook = thread_changed_hook;
        thread_changed_hook = NULL;
    }
    else {
        saved_state.thread_number = -1;
        saved_state.thread_changed_hook = NULL;
    }

    if (frame_number >= 0) {
        saved_state.frame_number = selected_frame_level;
        saved_state.frame_changed_hook = frame_changed_hook;
        frame_changed_hook = NULL;
    }
    else {
        saved_state.frame_number = -1;
        saved_state.frame_changed_hook = NULL;
    }

    old_chain = make_cleanup (pbx_restore_context_state, &saved_state);

    if (thread_number >= 0) {
        pbx_switch_to_thread_number(thread_number);
    }
    if (frame_number >= 0) {
        pbx_switch_to_frame_number(frame_number);
    }
    if (cmd) {
        printf_filtered ("\nContext: Thread %d : Frame %d :\n", 
                         thread_number, frame_number);
        execute_command (cmd, from_tty);
    }
    
    pbx_restore_context_state(&saved_state);
}


void
pbx_stack_size_command(char *unused, int from_tty)
{
    int	num_frames = pb_util_count_frames();

    if (from_tty) {
        if (num_frames < 0) {
            num_frames *= -1;
            printf_filtered("The stack is corrupted after the last frame.\n");
        }
    }
    printf_filtered("number of frames: %d\n", num_frames);
}

/* prints a simple backtrace of just function names; given
   the start frame and the number of frames to print
 */

void
pbx_backtrace_command(char *args, int from_tty)
{
    int			start_level;
    int			count_frames, level;
    struct frame_info	*fi, *start_fi;
    
    if (!target_has_stack)
      error ("No stack.");

    if (sscanf(args,"%d %d", &start_level, &count_frames) != 2) {
      error("Error parsing pb_backtrace command \"%s\"\n", args);
      return;
    }

    level = start_level;
    start_fi =  find_relative_frame (get_current_frame (), &level);

    if (level == 0) {
        /* found relative frame */
        for (fi = start_fi; fi && (level < count_frames); level++, fi = get_prev_frame(fi)) {
            QUIT;
            print_frame_info (fi, (start_level + level), 0, 1);
        }
    }

}
void
print_sel_frame (just_source)
     int just_source;
{
  print_stack_frame (selected_frame, -1, just_source ? -1 : 1);
}

/* Print info on the selected frame, including level number
   but not source.  */

void
print_selected_frame ()
{
  print_stack_frame (selected_frame, selected_frame_level, 0);
}

void
_XXXinitXXXialize_cmds_pbx ()
{
    add_com ("pbx-command-with-context", class_obscure, pbx_command_with_context_command,
           "<thread_number> <frame_number> <cmd> : execute <cmd> in the given frame number of the given thread.");

    add_com ("pbx-stack-size", class_obscure, pbx_stack_size_command,
           "prints number of frames in stack.");

    add_com ("pbx-backtrace", class_obscure, pbx_backtrace_command,
           "<start frame> <count> : prints frame info from start frame for count frames.");

}

#endif