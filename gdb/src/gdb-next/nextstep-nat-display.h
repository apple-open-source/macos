#ifndef _NEXTSTEP_NAT_DISPLAY_H_
#define _NEXTSTEP_NAT_DISPLAY_H_

#include "defs.h"

void set_view_program PARAMS ((const char *name));
void set_view_host PARAMS ((const char *name));

void connect_to PARAMS ((char *arg));

void set_noprompt PARAMS (());
char *get_view_connection PARAMS (());
void turn_off_viewing PARAMS (());
char *get_view_host PARAMS (());
void pb_breakpoint_move_command PARAMS ((char *arg, int from_tty));
void print_sel_frame PARAMS ((int just_source));
void print_selected_frame PARAMS (());

#endif /* _NEXTSTEP_NAT_DISPLAY_H_ */
