#include "defs.h"
#include "symtab.h"
#include "symfile.h"
#include "target.h"
#include "gdbcmd.h"
#include "objfiles.h"
#include "breakpoint.h"
#include "nextstep-nat-utils-pb.h"

static void annotation_level_command (char *arg, int from_tty)
{
  extern int annotation_level;

  if (strcmp (arg, "AnnotateForPB") == 0) {
    annotation_level = 2;
  } else {
    annotation_level = 0;
  }
}

/* stolen from ../breakpoint.c */

#define ALL_BREAKPOINTS(b)  for (b = breakpoint_chain; b; b = b->next)
extern struct breakpoint *breakpoint_chain;

void pb_breakpoint_move_command (char *arg, int from_tty)
{
  int bp_number;
  int line_number;
  struct breakpoint *bp, *b;

  if (sscanf(arg,"%d %d", &bp_number, &line_number) != 2) {
    printf_unfiltered ("Error parsing breakpoint command \"%s\"\n", arg);
    return;
  }

  bp = NULL;
  ALL_BREAKPOINTS (b) {
    if (b->number == bp_number) {
      bp = b;
      break;
    }
  }
  if (bp) {
    do_breakpoint_move (bp, line_number);
  } else {
    printf_unfiltered ("No breakpoint number %d.\n", bp_number);
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
_initialize_pb ()
{
  add_com ("annotation_level", class_support, annotation_level_command,
	   "Sets the annotation level.");

  add_com ("pb_breakpoint_move", class_support, pb_breakpoint_move_command,
	   "<bp_num> <line_number> : moves breakpoint bp_num to line line_number in same file.");
}
