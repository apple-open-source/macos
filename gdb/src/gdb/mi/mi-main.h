/* MI Internal Functions for GDB, the GNU debugger.

   Copyright 2003 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef MI_MAIN_H
#define MI_MAIN_H

/* APPLE LOCAL: These are data from mi-main.c that are needed
   externally.  FIXME: make interfaces for these instead.  */
extern int mi_dont_register_continuation;
extern char *current_command_token;
extern struct interp *mi_interp;
struct mi_continuation_arg;

/* APPLE LOCAL: This is a function for the mi_async event vector.  */
void mi_async_breakpoint_resolve_event (int b, int pending_b);

extern void
mi_interpreter_exec_continuation (struct continuation_arg *in_arg);

extern void mi_setup_architecture_data (void);

extern struct mi_continuation_arg *
  mi_setup_continuation_arg (struct cleanup *cleanups);

/* There should be a generic mi .h file where these should go... */
extern void mi_print_frame_more_info (struct ui_out *uiout,
				       struct symtab_and_line *sal,
				       struct frame_info *fi);

/* APPLE LOCAL: Fixme - I don't think these routines should be in
   mi-main.c.  They are only really used in the mi-interp.c  */

extern void mi_execute_command_wrapper (char *cmd);
extern void mi_execute_command (char *cmd, int from_tty);

extern void mi_output_async_notification (char *notification);

/* These are hooks that we put in place while doing interpreter_exec
   so we can report interesting things that happened "behind the mi's 
   back" in this command */

extern void mi_interp_create_breakpoint_hook (struct breakpoint *bpt);
extern void mi_interp_delete_breakpoint_hook (struct breakpoint *bpt);
extern void mi_interp_modify_breakpoint_hook (struct breakpoint *bpt);
extern void mi_interp_stack_changed_hook (void);
extern void mi_interp_frame_changed_hook (int new_frame_number);
extern void mi_interp_context_hook (int thread_id);
extern void mi_interp_stepping_command_hook(void);
extern void mi_interp_continue_command_hook(void);
extern int mi_interp_run_command_hook(void);
extern void mi_interp_hand_call_function_hook (void);

extern int mi_interp_exec_cmd_did_run;
extern void mi_interp_sync_stepping_command_hook(void);
extern void mi_interp_sync_continue_command_hook(void);

void mi_insert_notify_hooks (void);
void mi_remove_notify_hooks (void);

#endif

