#include "defs.h"
#include "symtab.h"
#include "symfile.h"
#include "target.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "gdbthread.h"
#include "frame.h"
#include "inferior.h"
#include "top.h"
#include "nextstep-nat-utils-pb.h"

#include "nextstep-nat-inferior.h"

#include <ctype.h>
#include <string.h>

char *pb_gdb_util_prompt ()
{
    return get_prompt();
}


struct ui_file *pb_gdb_util_stdout ()
{
    return gdb_stdout;
}


struct ui_file *pb_gdb_util_stderr ()
{
  return gdb_stderr;
}


/*
 Crawling up the stack, may cause gdb to access an invalid memory location.
 So, do the counting inside a catch_errors() scope.
 */

static int
count_frames(char *ptr)
{
    struct frame_info		*frame0;
    struct frame_info		*f;
    int				*num_frames_ptr;

    num_frames_ptr = (int *)ptr;

    frame0 = get_current_frame();
    *num_frames_ptr = 1;

    if (frame0) {
        for (f = get_prev_frame(frame0);
             f && (f != frame0);
             f = get_prev_frame(f))  {
            *num_frames_ptr += 1;
        }
    }
    return 1;
}

/* count number of frames in current stack;
   return negative number of frames if stack is malformed
 */
int pb_gdb_util_count_frames()
{
    int	num_frames = 0;
    int was_error;

    was_error =  catch_errors(count_frames, (void *)&num_frames, NULL, RETURN_MASK_ALL);

    if (was_error == 0)
        num_frames *= -1;

    return num_frames;
}


char *
pb_gdb_util_symtab_filename_path(struct symtab *symtab)
{
    if (symtab->fullname == NULL) {
        /* returns full path */
        /* may not need full path ... */
        symtab_to_filename (symtab);
    }

    return symtab->fullname;
}

char *
pb_gdb_util_breakpoint_filename(struct breakpoint *bp)
{
    return bp->source_file;
}


int
pb_gdb_util_breakpoint_number(struct breakpoint *bp)
{
    return bp->number;
}

int
pb_gdb_util_breakpoint_line_number(struct breakpoint *bp)
{
    return bp->line_number;
}

int
pb_gdb_util_breakpoint_type_is_breakpoint(struct breakpoint *bp)
{
    return (bp->type == bp_breakpoint);
}

static int useTty = 0;

static int
execute_command_for_PB (char *command)
{
  execute_command (command, useTty);
  bpstat_do_actions (&stop_bpstat);
  return 0;
}

int
pb_gdb_util_execute_command (char 	*command_string,
                         int	use_annotation,
                         int	use_tty)
{
    extern int	annotation_level;
    int		old_verbose;
    extern int	info_verbose;
    int 	wasError;


    if (use_annotation) {
        annotation_level = 2;
        printf_unfiltered("\n\032\032annotation-begin\n");
    }

    old_verbose = info_verbose;
    info_verbose = 0;
    useTty = use_tty;
    wasError = catch_errors((catch_errors_ftype *)execute_command_for_PB,
                            (char *)command_string,
                            NULL, RETURN_MASK_ALL);

    info_verbose = old_verbose;

    if (use_annotation) {
        annotation_level = 0;
        printf_unfiltered("\n\032\032annotation-end\n");
    }


    return wasError;
}

void
pb_gdb_util_add_set_cmd_string(const char *name, void *var_ptr, const char *desc)
{
    add_show_from_set (add_set_cmd (name, class_obscure,
                                    var_string_noescape, (char *)var_ptr,
                                    desc, &setlist),
                       &showlist);
}

#if (defined (TARGET_POWERPC) && defined (__ppc__)) || (defined (TARGET_I386) && defined (__i386__))

extern next_inferior_status *next_status;

int pb_gdb_util_inferior_pid()
{
  if (inferior_pid > 0) {
    int pid;
    thread_t thread;
    next_thread_list_lookup_by_id (next_status, inferior_pid, &pid, &thread);
    return pid;
  }

  return -1;
}

#else

int pb_gdb_util_inferior_pid()
{
  return -1;
}

#endif

void
pb_gdb_util_add_cmd(const char *name, void *func_ptr, const char *desc)
{
    add_com (name, class_obscure, func_ptr, desc);
}
