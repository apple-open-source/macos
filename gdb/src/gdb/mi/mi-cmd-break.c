/* MI Command Set - breakpoint and watchpoint commands.
   Copyright 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions (a Red Hat company).

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

#include "defs.h"
#include "target.h"
#include "mi-cmds.h"
#include "ui-out.h"
#include "mi-out.h"
#include "breakpoint.h"
#include "gdb_string.h"
#include "mi-getopt.h"
#include "mi-main.h"
#include "gdb-events.h"
#include "interps.h"
#include "gdb.h"
#include "gdbcmd.h" /* For print_command_lines.  */
#include "filenames.h"

enum
  {
    FROM_TTY = 0
  };

/* Output a single breakpoint. */

static void
breakpoint_notify (int b)
{
  gdb_breakpoint_query (uiout, b, NULL);
}


struct gdb_events breakpoint_hooks =
{
  breakpoint_notify,
  breakpoint_notify,
  breakpoint_notify,
};


enum bp_type
  {
    REG_BP,
    HW_BP,
    FUT_BP,
    REGEXP_BP
  };

/* Insert a breakpoint. The type of breakpoint is specified by the
   first argument: 
   -break-insert <location> --> insert a regular breakpoint.  
   -break-insert -t <location> --> insert a temporary breakpoint.  
   -break-insert -h <location> --> insert an hardware breakpoint.  
   -break-insert -t -h <location> --> insert a temporary hw bp.
   -break-insert -f <location> --> insert a future breakpoint.  
   -break-insert -r <regexp> --> insert a bp at functions matching
   <regexp> 

   You can also specify the shared-library in which to set the breakpoint
   by passing the -s argument, as:

   -break-insert -s <shlib>

   If this is a path, the full path must match, otherwise the base
   filename must match.  This can be given in combination with
   the other options above.

   You can also specify a list of indices which will be applied to the
   list of matches that gdb builds up for the breakpoint if there are
   multiple matches.  If the first element of the list is "-1" ALL
   matches will be accepted.

   -break-insert -l "<NUM> <NUM> ... " <expression>

*/

enum mi_cmd_result
mi_cmd_break_insert (char *command, char **argv, int argc)
{
  char *address = NULL;
  enum bp_type type = REG_BP;
  int temp_p = 0;
  int thread = -1;
  int ignore_count = 0;
  char *condition = NULL;
  char *requested_shlib = NULL;
  char realpath_buf[PATH_MAX];
  enum gdb_rc rc;
  int *indices = NULL;
  struct gdb_events *old_hooks;
  enum opt
    {
      HARDWARE_OPT, TEMP_OPT, FUTURE_OPT /*, REGEXP_OPT */ , CONDITION_OPT,
      IGNORE_COUNT_OPT, THREAD_OPT, SHLIB_OPT, LIST_OPT
    };
  static struct mi_opt opts[] =
  {
    {"h", HARDWARE_OPT, 0},
    {"t", TEMP_OPT, 0},
    {"f", FUTURE_OPT, 0},
    {"c", CONDITION_OPT, 1},
    {"i", IGNORE_COUNT_OPT, 1},
    {"p", THREAD_OPT, 1},
    {"s", SHLIB_OPT, 1},
    {"l", LIST_OPT, 1},
    0
  };

  /* Parse arguments. It could be -r or -h or -t, <location> or ``--''
     to denote the end of the option list. */
  int optind = 0;
  char *optarg;
  struct cleanup *indices_cleanup = NULL;

  while (1)
    {
      int opt = mi_getopt ("mi_cmd_break_insert", argc, argv, opts, &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case TEMP_OPT:
	  temp_p = 1;
	  break;
	case HARDWARE_OPT:
	  type = HW_BP;
	  break;
	case FUTURE_OPT:
	  type = FUT_BP;
	  break;
#if 0
	case REGEXP_OPT:
	  type = REGEXP_BP;
	  break;
#endif
	case CONDITION_OPT:
	  condition = optarg;
	  break;
	case IGNORE_COUNT_OPT:
	  ignore_count = atol (optarg);
          /* APPLE LOCAL: Same behavior as set_ignore_count().  */
          if (ignore_count < 0)
            ignore_count = 0;
	  break;
	case THREAD_OPT:
	  thread = atol (optarg);
	  break;
        case SHLIB_OPT:
          requested_shlib = optarg;
          break;
	case LIST_OPT:
	  {
	    char *numptr;
	    int nelem = 0, i;
	    /* First count the number of elements, which is the
	       number of spaces plus one.  */
	    numptr = optarg;
	    while (*numptr)
	      {
		if (*numptr != ' ')
		  {
		    nelem++;
		    while (*numptr != ' ' && *numptr != '\0')
		      numptr++;
		  }
		else
		  numptr++;
	      }

	    if (nelem == 0)
	      error ("mi_cmd_break_insert: Got index with no elements");

	    indices = (int *) xmalloc ((nelem + 1) * sizeof (int *));
	    indices_cleanup = make_cleanup (xfree, indices);

	    /* Now extract the elements.  */

	    numptr = optarg;
	    i = 0;
	    errno = 0;
	    while (*numptr != '\0')
	      {
		indices[i++] = strtol (numptr, &numptr, 10);
		if (errno == EINVAL)
		    error ("mi_cmd_break_insert: bad index at \"%s\"", numptr);
	      }

	    /* Since we aren't passing a number of elements, we terminate the
	       indices by putting in a -1 element.  */
	    
	    indices[i] = -1;

	    break;
	  }
	}
    }

  if (optind >= argc)
    error (_("mi_cmd_break_insert: Missing <location>"));
  if (optind < argc - 1)
    error (_("mi_cmd_break_insert: Garbage following <location>"));
  address = argv[optind];

  /* APPLE LOCAL: realpath() the incoming shlib name, as we do with all
     objfile/dylib/executable names.  NB this condition is incorrect if
     we're passed something like "./foo.dylib", "../foo.dylib", or
     "~/bin/foo.dylib", but that shouldn't happen....  */
  if (requested_shlib && IS_ABSOLUTE_PATH (requested_shlib))
    {
      realpath (requested_shlib, realpath_buf);
      /* It'll be xstrdup()'ed down in the breakpoint command, so just point
         to the stack array until then. */
      requested_shlib = realpath_buf; 
    }

  /* Now we have what we need, let's insert the breakpoint! */
  old_hooks = deprecated_set_gdb_event_hooks (&breakpoint_hooks);
  switch (type)
    {
    case REG_BP:
      rc = gdb_breakpoint (address, condition,
			   0 /*hardwareflag */ , temp_p,
			   0 /* futureflag */, thread, 
			   ignore_count, indices, requested_shlib,
			   &mi_error_message);
      break;
    case HW_BP:
      rc = gdb_breakpoint (address, condition,
			   1 /*hardwareflag */ , temp_p,
			   0 /* futureflag */, thread, 
			   ignore_count, indices, requested_shlib,
			   &mi_error_message);
      break;
    case FUT_BP:
      rc = gdb_breakpoint (address, condition,
			   0, temp_p,
			   1 /* futureflag */, thread, 
			   ignore_count, indices, requested_shlib,
			   &mi_error_message);
      break;

#if 0
    case REGEXP_BP:
      if (temp_p)
	error (_("mi_cmd_break_insert: Unsupported tempoary regexp breakpoint"));
      else
	rbreak_command_wrapper (address, FROM_TTY);
      return MI_CMD_DONE;
      break;
#endif
    default:
      internal_error (__FILE__, __LINE__,
		      _("mi_cmd_break_insert: Bad switch."));
    }

  deprecated_set_gdb_event_hooks (old_hooks);

  /* APPLE LOCAL huh? */
  if (indices_cleanup != NULL)
    do_cleanups (indices_cleanup);

  if (rc == GDB_RC_FAIL)
    return MI_CMD_ERROR;
  else
    return MI_CMD_DONE;
}

enum wp_type
{
  REG_WP,
  READ_WP,
  ACCESS_WP
};

/* Insert a watchpoint. The type of watchpoint is specified by the
   first argument: 
   -break-watch <expr> --> insert a regular wp.  
   -break-watch -r <expr> --> insert a read watchpoint.
   -break-watch -a <expr> --> insert an access wp. */

enum mi_cmd_result
mi_cmd_break_watch (char *command, char **argv, int argc)
{
  char *expr = NULL;
  enum wp_type type = REG_WP;
  int watch_location = 0;
  enum opt
    {
      READ_OPT, ACCESS_OPT, LOCATION_OPT
    };
  static struct mi_opt opts[] =
  {
    {"r", READ_OPT, 0},
    {"a", ACCESS_OPT, 0},
    {"l", LOCATION_OPT, 0},
    0
  };

  /* Parse arguments. */
  int optind = 0;
  char *optarg;
  while (1)
    {
      int opt = mi_getopt ("mi_cmd_break_watch", argc, argv, opts, &optind, &optarg);
      if (opt < 0)
	break;
      switch ((enum opt) opt)
	{
	case READ_OPT:
	  type = READ_WP;
	  break;
	case ACCESS_OPT:
	  type = ACCESS_WP;
	  break;
	case LOCATION_OPT:
	  watch_location = 1;
	  break;
	}
    }
  if (optind >= argc)
    error (_("mi_cmd_break_watch: Missing <expression>"));
  if (optind < argc - 1)
    error (_("mi_cmd_break_watch: Garbage following <expression>"));
  expr = argv[optind];

  /* Now we have what we need, let's insert the watchpoint! */
  switch (type)
    {
    case REG_WP:
      watch_command_wrapper (expr, watch_location, FROM_TTY);
      break;
    case READ_WP:
      rwatch_command_wrapper (expr, watch_location, FROM_TTY);
      break;
    case ACCESS_WP:
      awatch_command_wrapper (expr, watch_location, FROM_TTY);
      break;
    default:
      error (_("mi_cmd_break_watch: Unknown watchpoint type."));
    }
  return MI_CMD_DONE;
}

char **mi_command_line_array;
int mi_command_line_array_cnt;
int mi_command_line_array_ptr;

static char *
mi_read_next_line ()
{

  if (mi_command_line_array_ptr == mi_command_line_array_cnt)
    return NULL;
  else
    {
      return mi_command_line_array[mi_command_line_array_ptr++];
    }
}

enum mi_cmd_result
mi_cmd_break_commands (char *command, char **argv, int argc)
{
  struct command_line *break_command;
  char *endptr;
  int bnum;
  struct breakpoint *b;

  if (argc < 1)
    error ("%s: USAGE: %s <BKPT> [<COMMAND> [<COMMAND>...]]", command, command);

  bnum = strtol (argv[0], &endptr, 0);
  if (endptr == argv[0])
    {
      xasprintf (&mi_error_message,
		 "%s: breakpoint number argument \"%s\" is not a number.",
		 command, argv[0]);
      return MI_CMD_ERROR;
    }
  else if (*endptr != '\0')
    {
      xasprintf (&mi_error_message,
		 "%s: junk at the end of breakpoint number argument \"%s\".",
		 command, argv[0]);
      return MI_CMD_ERROR;
    }

  b = find_breakpoint (bnum);
  if (b == NULL)
    {
      xasprintf (&mi_error_message,
		 "%s: breakpoint %d not found.",
		 command, bnum);
      return MI_CMD_ERROR;
    }

  /* With no commands set, just print the current command set. */
  if (argc == 1)
    {
      breakpoint_print_commands (uiout, b);
      return MI_CMD_DONE;
    }

  mi_command_line_array = argv;
  mi_command_line_array_ptr = 1;
  mi_command_line_array_cnt = argc;

  break_command = read_command_lines_1 (mi_read_next_line);
  breakpoint_add_commands (b, break_command);

  return MI_CMD_DONE;
  
}

enum mi_cmd_result
mi_cmd_break_catch (char *command, char **argv, int argc)
{
  enum exception_event_kind ex_event;

  if (argc < 1)
    error ("mi_cmd_break_catch: USAGE: %s [catch|throw] [on|off]", command);

  if (strcmp(argv[0], "catch") == 0)
    ex_event = EX_EVENT_CATCH;
  else if (strcmp(argv[0], "throw") == 0)
    ex_event = EX_EVENT_THROW;
  else
    error ("mi_cmd_break_catch: bad argument, should be \"catch\""
	   " or \"throw\"");

  if (argc == 2)
    {
      if (strcmp (argv[1], "off") == 0)
	{
	  if (exception_catchpoints_enabled (ex_event))
	    {
	      disable_exception_catch (ex_event);
	      return MI_CMD_DONE;
	    }
	  else
	    {
	      return MI_CMD_ERROR;
	    }
	}
      else if (strcmp (argv[1], "on") != 0)
	error ("mi_cmd_break_catch: bad argument 2, should be \"on\""
	       " or \"off\".");
    }

  /* If the catchpoints are already enabled, we are done... */
  if (exception_catchpoints_enabled (ex_event))
      return MI_CMD_DONE;

  /* See if we can find a callback routine */
  if (handle_gnu_v3_exceptions (ex_event))
    {
      gnu_v3_update_exception_catchpoints (ex_event, 0, NULL);
      return MI_CMD_DONE;
    }

  if (target_enable_exception_callback (ex_event, 1))
    {
      error ("mi_cmd_break_catch: error getting callback routine.");
    }
  return MI_CMD_DONE;
}

/* APPLE LOCAL: The FSF deleted all these hooks.  Supposedly we can
   get the same information from the gdb_events, but I am not 
   convinced that when running the console interpreter under the
   mi the gdb_events will work.  FIXME: see if the gdb_events
   actually can be made to work.  */

void
mi_interp_create_breakpoint_hook (struct breakpoint *bpt)
{
  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;

  /* Don't report internal breakpoints. */
  if (bpt->number == 0)
    return;

  uiout = interp_ui_out (mi_interp);

  /* This is a little inefficient, but it probably isn't worth adding
     a gdb_breakpoint_query that takes a bpt structure... */

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "breakpoint_create");
  gdb_breakpoint_query (uiout, bpt->number, NULL);
  do_cleanups (list_cleanup);
  uiout = saved_ui_out; 
}

void
mi_interp_modify_breakpoint_hook (struct breakpoint *bpt)
{

  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;

  /* Don't report internal breakpoints. */
  if (bpt->number == 0)
    return;

  uiout = interp_ui_out (mi_interp);

  /* This is a little inefficient, but it probably isn't worth adding
     a gdb_breakpoint_query that takes a bpt structure... */

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "breakpoint_modify");
  gdb_breakpoint_query (uiout, bpt->number, NULL);
  do_cleanups (list_cleanup);
  uiout = saved_ui_out; 
}

void
mi_interp_delete_breakpoint_hook (struct breakpoint *bpt)
{
  struct ui_out *saved_ui_out = uiout;
  struct cleanup *list_cleanup;

  /* Don't report internal breakpoints. */
  if (bpt->number == 0)
    return;

  uiout = interp_ui_out (mi_interp);

  /* This is a little inefficient, but it probably isn't worth adding
     a gdb_breakpoint_query that takes a bpt structure... */

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "MI_HOOK_RESULT");
  ui_out_field_string (uiout, "HOOK_TYPE", "breakpoint_delete");
  ui_out_field_int (uiout, "bkptno", bpt->number);
  do_cleanups (list_cleanup);
  uiout = saved_ui_out; 

}

void
mi_async_breakpoint_resolve_event (int pending_b, int new_b)
{
  struct cleanup *old_chain;
  struct breakpoint *bpt;

  /* Don't notify about internal breakpoint changes.  */
  if (pending_b <= 0)
    return;

  old_chain = make_cleanup_ui_out_notify_begin_end (uiout, 
						    "resolve-pending-breakpoint");
  ui_out_field_int (uiout, "new_bp", new_b);
  ui_out_field_int (uiout, "pended_bp", pending_b);
  bpt = find_breakpoint (new_b);
  if (bpt->addr_string != NULL)
    ui_out_field_string (uiout, "new_expr", bpt->addr_string);

  /* APPLE LOCAL: Need to tell the UI whether the breakpoint condition was not
     successfully evaluated, so it can put up an appropriate warning.  */
  if (bpt->cond_string != NULL)
    {
      if (bpt->cond == NULL)
	ui_out_field_int (uiout, "condition_valid", 0); 
      else
	ui_out_field_int (uiout, "condition_valid", 1); 
    }
  /* END APPLE LOCAL  */
  gdb_breakpoint_query (uiout, new_b, NULL);
  
  do_cleanups (old_chain);
}
