#include "defs.h"

#include "DisplayTypes.h"

extern void (*window_hook) PARAMS ((FILE *, char *));

void tell_displayer_display_lines
PARAMS ((struct symtab *symtab, int first_line, int last_line));

/* for GuiGdbManager */
void displayer_command_loop PARAMS (());
int tell_displayer_do_query PARAMS ((char *query, va_list args));
void tell_displayer_fputs_output (const char *linebuffer, FILE *stream);

void tell_displayer_state_changed PARAMS ((Debugger_state newState));
void tell_displayer_frame_changed PARAMS ((int newFrame));

/* called when the inferior stops and we aren't in the same
   frame 0 as the previous stop. */
void tell_displayer_stack_changed ();

void displayer_create_breakpoint_hook PARAMS ((struct breakpoint *bp));
void displayer_delete_breakpoint_hook PARAMS ((struct breakpoint *bp));
void displayer_modify_breakpoint_hook PARAMS ((struct breakpoint *bp));

/* used internally; not a hook */
extern void tell_displayer_breakpoint_changed 
PARAMS ((struct breakpoint *b, BreakpointState newState));

/* command line input hook */
const char *tell_displayer_get_input PARAMS ((char *prropmpt, int repeat, char *anno_suffix));



