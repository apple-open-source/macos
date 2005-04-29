/* Manages interpreters for gdb.
   Copyright 2000 Free Software Foundation, Inc.
   Written by Jim Ingham <jingham@apple.com> of Apple Computer, Inc.

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
   Boston, MA 02111-1307, USA. */

#ifndef GDB_INTERPRETER_H
#define GDB_INTERPRETER_H

#error removed

typedef int (*interp_init_ftype) (void *data);
typedef int (*interp_resume_ftype) (void *data);
typedef int (*interp_do_one_event_ftype) (void *data);
typedef int (*interp_suspend_ftype) (void *data);
typedef int (*interp_check_done_ftype) (void *data);
typedef int (*interp_delete_ftype) (void *data);
typedef int (*interp_prompt_ftype) (void *data, char *new_prompt);
typedef int (*interp_exec_ftype) (void *data, char *command);
typedef int (*interp_complete_ftype) (void *data, char *word, char *command_buffer, int cursor);

struct gdb_interpreter 
{
  char *name;               /* This is the name in "-i=" and set interpreter. */
  struct gdb_interpreter *next; /* Interpreters are stored in a linked list, 
				   this is the next one... */
  void *data;                /* This is a cookie that the instance of the 
				interpreter can use, for instance to call 
				itself in hook functions */
  int inited;                /* Has the init_proc been run? */
  struct ui_out *interpreter_out; /* This is the ui_out used to collect 
				     results for this interpreter.  It can 
				     be a formatter for stdout, as is the 
				     case for the console & mi outputs, or it 
				     might be a result formatter. */
  int quiet_p;
                                                 
  interp_init_ftype         init_proc;
  interp_resume_ftype       resume_proc;
  interp_do_one_event_ftype do_one_event_proc;
  interp_suspend_ftype      suspend_proc;
  interp_check_done_ftype   check_done_proc;
  interp_delete_ftype       delete_proc;
  interp_exec_ftype         exec_proc;
  interp_prompt_ftype       prompt_proc;
  interp_complete_ftype     complete_proc;
 };

extern struct gdb_interpreter 
*gdb_new_interpreter (char *name, 
		      void *data, 
		      struct ui_out *uiout,
		      interp_init_ftype init_proc, 
		      interp_resume_ftype  resume_proc,
		      interp_do_one_event_ftype do_one_event_proc,
		      interp_suspend_ftype suspend_proc, 
		      interp_delete_ftype delete_proc,
		      interp_exec_ftype   exec_proc,
		      interp_prompt_ftype prompt_proc,
		      interp_complete_ftype complete_proc);

extern int gdb_add_interpreter (struct gdb_interpreter *interp);
extern int gdb_delete_interpreter(struct gdb_interpreter *interp);
extern int gdb_set_interpreter (struct gdb_interpreter *interp);
extern struct gdb_interpreter *gdb_lookup_interpreter (char *name);
extern struct gdb_interpreter *gdb_current_interpreter ();
extern struct ui_out *gdb_interpreter_ui_out (struct gdb_interpreter *interp);
extern int gdb_current_interpreter_is_named(char *interp_name);
extern int gdb_interpreter_exec (char *command_str);
extern int gdb_interpreter_display_prompt (char *new_prompt);
extern int gdb_interpreter_set_quiet (struct gdb_interpreter *interp, 
				       int quiet);
extern int gdb_interpreter_is_quiet (struct gdb_interpreter *interp);
extern int gdb_interpreter_complete (struct gdb_interpreter *interp, 
				     char *word, char *command_buffer, int cursor);

extern int interpreter_do_one_event ();

void clear_interpreter_hooks ();

/* well-known interpreters */
#define GDB_INTERPRETER_CONSOLE		"console"
#define GDB_INTERPRETER_MI		"mi"

#endif /* GDB_INTERPRETER_H */

