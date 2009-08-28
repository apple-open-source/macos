/* APPLE LOCAL file checkpoints */
/* Checkpoints for GDB.
   Copyright 2005
   Free Software Foundation, Inc.

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

#include <string.h>
#include <signal.h>

#include "defs.h"
#include "symtab.h"
#include "target.h"
#include "frame.h"
#include "command.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "inferior.h"

extern void re_execute_command (char *args, int from_tty);
extern void rollback_stop (void);

#include <dlfcn.h>

#include "checkpoint.h"

#define LIBCHECKPOINT_NAME "/usr/libexec/gdb/libcheckpoint.dylib"
#define CP_FORK_NAME "_gdbcp_fork"
#define CP_CG_SAVE_NAME "_gdbcp_cg_save"
#define CP_CG_ROLLBACK_NAME "_gdbcp_cg_rollback"

void prune_checkpoint_list (void);
void delete_checkpoint (struct checkpoint *);
static void sigterm_handler (int signo);
  
extern struct checkpoint *rx_cp;

/* True when we want to create a checkpoint at every stop.  */

int auto_checkpointing;

/* This is the maximum number of checkpoints we want to retain.  */

int max_checkpoints = 50;

/* True when we want libraries, frameworks, and other special
   subsystems to checkpoint themselves.  */

int subsystem_checkpointing = 0;

int forking_checkpoints = 1;

/* Memory cache stuff.  */

/* Get a block from the inferior and save it.  */

void
memcache_get (struct checkpoint *cp, ULONGEST addr, int len)
{
  struct memcache *mc;
  int actual;

  mc = (struct memcache *) xmalloc (sizeof (struct memcache));
  mc->startaddr = addr;
  mc->len = len;
  mc->cache = (gdb_byte *) xmalloc (len);

  mc->next = cp->mem;
  cp->mem = mc;

  actual = target_read_partial (&current_target, TARGET_OBJECT_MEMORY,
				NULL, mc->cache, addr, len);
  /*  printf ("cached %d (orig %d) bytes at 0x%llx\n", actual, len, addr); */

  mc->cache = (gdb_byte *) xrealloc (mc->cache, actual);

  mc->len = actual;
}

void
memcache_put (struct checkpoint *cp)
{
  struct memcache *mc;

  for (mc = cp->mem; mc != NULL; mc = mc->next)
    target_write_partial (&current_target, TARGET_OBJECT_MEMORY,
			  NULL, mc->cache, mc->startaddr, mc->len);

#ifdef NM_NEXTSTEP /* in lieu of target vectory */
 {
   void fork_memcache_put (struct checkpoint *);
   if (cp->pid != 0)
     fork_memcache_put (cp);
 }
#endif
}

/* The count of checkpoints that have been created so far.  */

int checkpoint_count = 1;

/* List of all checkpoints.  */
struct checkpoint *checkpoint_list;
struct checkpoint *last_checkpoint;

/* The current checkpoint is the one corresponding to the actual state of
   the inferior, or to the last point of checkpoint creation.  */

struct checkpoint *current_checkpoint;

/* Latest checkpoint on original line of execution. The program can be continued
   from this one with no ill effects on state of the world.  */

struct checkpoint *original_latest_checkpoint;

/* This is true if we're already in the midst of creating a
   checkpoint, just in case something thinks it wants to make another
   one.  */

int collecting_checkpoint = 0;

int rolling_back = 0;

int rolled_back = 0;

/* This variable is true when the user has asked for a checkpoint to
   be created at each stop.  */

int auto_checkpointing = 0;

/* This is true when we're running the inferior as part of function calls.  */

int inferior_call_checkpoints;

int warned_cpfork = 0;

/* Use inferior's dlopen() to bring in some helper functions.  */

void
load_helpers ()
{
  struct value *dlfn, *args[2], *val;
  long rslt;

  args[0] = value_string (LIBCHECKPOINT_NAME, strlen(LIBCHECKPOINT_NAME) + 1);
  args[0] = value_coerce_array (args[0]);
  args[1] = value_from_longest (builtin_type_int, (LONGEST) RTLD_NOW);
  if (lookup_minimal_symbol ("dlopen", 0, 0)
      && (dlfn = find_function_in_inferior ("dlopen", builtin_type_int)))
    {
      val = call_function_by_hand_expecting_type (dlfn,
						  builtin_type_int, 2, args, 1);
      rslt = value_as_long (val);
      if (rslt == 0)
	warning ("dlopen of checkpoint library returned NULL");
    }
  else
    {
      warning ("dlopen not found, libcheckpoint functions not loaded");
    }
}

/* Manually create a checkpoint.  */

/* Note that multiple checkpoints may be created at the same point; it
   may be that the user has manually touched registers or memory in
   between. Rolling back to identical checkpoints is not interesting,
   but not harmful either.  */

static void
create_checkpoint_command (char *args, int from_tty)
{
  struct checkpoint *cp;

  if (!target_has_execution)
    {
      error ("Cannot create checkpoints before the program has run.");
      return;
    }

  cp = create_checkpoint ();

  if (cp)
    cp->type = manual;

  if (cp)
    printf ("Checkpoint %d created\n", cp->number);
  else
    printf ("Checkpoint creation failed.\n");
}

struct checkpoint *
create_checkpoint ()
{
  struct checkpoint *cp;

  if (!target_has_execution)
    return NULL;

  cp = collect_checkpoint ();

  return finish_checkpoint (cp);
}

/* Create a content-less checkpoint.  */

struct checkpoint *
start_checkpoint ()
{
  struct checkpoint *cp;

  cp = (struct checkpoint *) xmalloc (sizeof (struct checkpoint));
  memset (cp, 0, sizeof (struct checkpoint));

  cp->regs = regcache_xmalloc (current_gdbarch);
  cp->mem = NULL;

  cp->pid = 0;

  return cp;
}

int warned_cg = 0;
static int checkpoint_initialized = 0;

void
checkpoint_clear_inferior ()
{
  checkpoint_initialized = 0;
}

struct checkpoint *
collect_checkpoint ()
{
  struct checkpoint *cp;
  struct value *forkfn;
  struct value *val;
  int retval;

  cp = start_checkpoint ();

  /* Always collect the registers directly.  */
  regcache_cpy (cp->regs, current_regcache);

  if (!checkpoint_initialized)
    {
      load_helpers ();
      signal (SIGTERM, sigterm_handler);
      checkpoint_initialized = 1;
    }

  /* Do subsystems first so their remembered state gets into memcache.  */
  /* This bit does CoreGraphics windows explicitly; in a final version
     subsystems would register themselves as part of target-specific
     checkpoint saving.  */
  if (subsystem_checkpointing) {
    struct value *cgfn, *arg, *val;

    /* (It would seem more logical to use cp->number here, but it hasn't been
       assigned yet - revisit this issue later.)  */
    arg = value_from_longest (builtin_type_int, (LONGEST) checkpoint_count);
    if (lookup_minimal_symbol (CP_CG_SAVE_NAME, 0, 0)
	&& (cgfn = find_function_in_inferior (CP_CG_SAVE_NAME, builtin_type_int)))
      {
	val = call_function_by_hand_expecting_type (cgfn,
						    builtin_type_int, 1, &arg, 1);
      }
    else
      {
	if (!warned_cg)
	  {
	    warning (CP_CG_SAVE_NAME " not found");
	    warned_cg = 1;
	  }
      }
  }
  
  if (forking_checkpoints)
    {
      /* (The following should be target-specific) */
      if (lookup_minimal_symbol(CP_FORK_NAME, 0, 0)
	  && (forkfn = find_function_in_inferior (CP_FORK_NAME, builtin_type_int)))
	{
	  val = call_function_by_hand_expecting_type (forkfn,
						      builtin_type_int, 0, NULL, 1);
  
	  retval = value_as_long (val);

	  /* Keep the pid around, only dig through fork when rolling back.  */
	  cp->pid = retval;
	}
      else
	{
	  if (!warned_cpfork)
	    {
	      warning (CP_FORK_NAME " not found, falling back to memory reads to make checkpoints");
	      warned_cpfork = 1;
	    }
	}
    }

#ifdef NM_NEXTSTEP /* in lieu of target vectory */
  {
    void direct_memcache_get (struct checkpoint *);
    if (cp->pid == 0)
      direct_memcache_get (cp);
  }
#endif

  return cp;
}

struct checkpoint *
finish_checkpoint (struct checkpoint *cp)
{
  cp->number = checkpoint_count++;

  /* Link into the end of the list.  */
  if (last_checkpoint)
    {
      last_checkpoint->next = cp;
      cp->prev = last_checkpoint;
      last_checkpoint = cp;
    }
  else
    {
      checkpoint_list = last_checkpoint = cp;
      cp->prev = NULL;
    }

  cp->next = NULL;

  /* By definition, any newly-created checkpoint is the current one.  */
  if (current_checkpoint)
    current_checkpoint->lnext = cp;
  cp->lprev = current_checkpoint;
  current_checkpoint = cp;

  /* If we're on the original no-rollback line of execution, record
     this checkpoint as the most recent.  */
  if (!rolled_back)
    original_latest_checkpoint = cp;

  prune_checkpoint_list ();

  return cp;
}

/* This function limits the total number of checkpoints in existence
   at any one time. It prefers to retain checkpoints in the direct
   line of execution back from the current state, and then the most
   recently-created checkpoints.  */

/* The algorithm here is not especially efficient, and should be
   redone if we expect to keep hundreds of checkpoints around.  */

void
prune_checkpoint_list ()
{
  int i, n, numcp, numtokeep;
  struct checkpoint *cp2, **todel;

  /* Don't do anything if no limits on checkpointing.  */
  if (max_checkpoints < 0)
    return;

  /* Count the checkpoints we have right now.  */
  numcp = 0;
  for (cp2 = checkpoint_list; cp2 != NULL; cp2 = cp2->next)
    ++numcp;

  if (numcp <= max_checkpoints)
    return;

  for (cp2 = checkpoint_list; cp2 != NULL; cp2 = cp2->next)
    cp2->keep = 0;
  numtokeep = 0;
  /* Keep as many as possible on the straight-line chain back from the
     most recent checkpoint.  */
  for (cp2 = current_checkpoint; cp2 != NULL; cp2 = cp2->lprev)
    {
      if (numtokeep >= max_checkpoints)
	break;
      cp2->keep = 1;
      ++numtokeep;
    }
  /* Now scan last-to-first in general list.  */
  for (cp2 = last_checkpoint; cp2 != NULL; cp2 = cp2->prev)
    {
      if (cp2->keep)
	continue;
      if (numtokeep >= max_checkpoints)
	break;
      cp2->keep = 1;
      ++numtokeep;
    }
  /* Now collect the checkpoints we want to delete. Handle as a
     temporarily-allocated array of checkpoints to delete so we don't
     get confused by all the pointer churn.  */
  n = numcp - numtokeep;
  todel = (struct checkpoint **) xmalloc (n * sizeof (struct checkpoint *));
  i = 0;
  for (cp2 = checkpoint_list; cp2 != NULL; cp2 = cp2->next)
    {
      if (!cp2->keep)
	todel[i++] = cp2;
    }
  for (i = 0; i < n; ++i)
    delete_checkpoint (todel[i]);
  xfree (todel);
}

int
checkpoint_compare (struct checkpoint *cp1, struct checkpoint *cp2)
{
  int regcache_compare (struct regcache *rc1, struct regcache *rc2);

  if (regcache_compare (cp1->regs, cp2->regs) == 0)
    return 0;

  return 1;
}

/* This is called from infrun.c:normal_stop() to auto-generate checkpoints.  */

void
maybe_create_checkpoint ()
{
  struct checkpoint *tmpcp, *lastcp;

  if (!auto_checkpointing)
    return;

  if (collecting_checkpoint || rolling_back)
    return;
  if (inferior_call_checkpoints)
    return;
  /* Oddly, we can end up here even after the program has exited.  */
  if (!target_has_execution)
    return;

  collecting_checkpoint = 1;

  lastcp = current_checkpoint;

  tmpcp = collect_checkpoint ();

#if 0 /* used for re-execution */
  for (cp = checkpoint_list; cp != NULL; cp = cp->next)
    if (checkpoint_compare (cp, tmpcp))
      {
	current_checkpoint = cp;
	if (rx_cp && lastcp)
	  current_checkpoint->immediate_prev = lastcp;
	return;
      }
#endif

  finish_checkpoint (tmpcp);

  tmpcp->type = autogen;

  /*
  if ((rx_cp && lastcp) || step_range_end == 1)
    current_checkpoint->immediate_prev = lastcp;
  */

  collecting_checkpoint = 0;
}

/* Call this pair of routines to enable special handling for
   checkpoints created as part of calls into the inferior.  */
/* Current policy is not to create those checkpoints, although the
   policy needs more thought, for instance about the right thing to do
   when one stops at a breakpoint and then single-steps for a
   while.  */

void
begin_inferior_call_checkpoints()
{
  if (!collecting_checkpoint)
    inferior_call_checkpoints = 1;
}

void
end_inferior_call_checkpoints()
{
  if (!collecting_checkpoint)
    inferior_call_checkpoints = 0;
}

static void
rollback_to_checkpoint_command (char *args, int from_tty)
{
  char *p;
  int num = 1;
  struct checkpoint *cp;

  if (!checkpoint_list)
    {
      error ("No checkpoints to roll back to!\n");
      return;
    }

  if (args)
    {
      p = args;
      num = atoi (p);
    }

  cp = find_checkpoint (num);

  if (cp == NULL)
    {
      error ("checkpoint %d not found\n", num);
      return;
    }

  rollback_to_checkpoint (cp);
}

/* Given a checkpoint, make it the current state.  */

void
rollback_to_checkpoint (struct checkpoint *cp)
{
  regcache_cpy (current_regcache, cp->regs);

  memcache_put (cp);

  /* Prevent a bit of recursion.  */
  rolling_back = 1;

  /* This bit calls a function to roll CoreGraphics windows back to a
     previous state.  */
  if (subsystem_checkpointing) {
    struct value *cgfn, *arg, *val;

    arg = value_from_longest (builtin_type_int, (LONGEST) cp->number);
    if (lookup_minimal_symbol (CP_CG_ROLLBACK_NAME, 0, 0)
	&& (cgfn = find_function_in_inferior (CP_CG_ROLLBACK_NAME, builtin_type_int)))
      {
	val = call_function_by_hand_expecting_type (cgfn,
						    builtin_type_int, 1, &arg, 1);
      }
    else
      {
	if (!warned_cg)
	  {
	    warning (CP_CG_ROLLBACK_NAME " not found");
	    warned_cg = 1;
	  }
      }
  }
  
  current_checkpoint = cp;

  flush_cached_frames ();

  /* Rolling back is a lot like a normal stop, in that we want to tell
     the user where we are, etc, but different because it's not the
     consequence of a resume.  */
  rollback_stop ();

  rolling_back = 0;

  /* Mark that we have been messing around with the program's state.  */
  rolled_back = (cp != original_latest_checkpoint);
}

/* Find a checkpoint given its number.  */

struct checkpoint *
find_checkpoint (int num)
{
  struct checkpoint *cp;

  /* Use a linear search, may need to improve on this when large numbers
     of checkpoints are in use.  */
  for (cp = checkpoint_list; cp != NULL; cp = cp->next)
    {
      if (cp->number == num)
	return cp;
    }
  return NULL;
}

/* static */ void
checkpoints_info (char *args, int from_tty)
{
  struct checkpoint *cp;

  for (cp = checkpoint_list; cp != NULL; cp = cp->next)
    {
      print_checkpoint_info (cp);
    }
}

void
print_checkpoint_info (struct checkpoint *cp)
{
  gdb_byte reg_buf[MAX_REGISTER_SIZE];
  LONGEST pc;
  struct symtab_and_line sal;

  regcache_cooked_read (cp->regs, PC_REGNUM, reg_buf);
  pc = *((LONGEST *) reg_buf);
  sal = find_pc_line (pc, 0);

  if (cp->pid != 0)
    printf("[%d] ", cp->pid);
  else
    {
      struct memcache *mc;
      int blocks = 0, tot = 0;

      for (mc = cp->mem; mc != NULL; mc = mc->next)
	{
	  ++blocks;
	  tot += mc->len;
	}
      printf("[%d/%d] ", tot, blocks);
    }

  printf ("%c%c%c%d: pc=0x%s",
	  cp->type,
	  (current_checkpoint == cp ? '*' : ' '),
	  (original_latest_checkpoint == cp ? '!' : ' '),
	  cp->number, paddr_nz (pc));

  printf (" (");
  if (cp->lprev)
    printf ("%d", cp->lprev->number);
  else
    printf (" ");
  printf ("<-)");
  printf (" (->");
  if (cp->lnext)
    printf ("%d", cp->lnext->number);
  else
    printf (" ");
  printf (")");

  if (sal.symtab)
    {
      printf (" -- ");
      print_source_lines (sal.symtab, sal.line, 1, 0);
    }
  else
    {
      printf ("\n");
    }
}

/* Checkpoint commands.  */

static void
undo_command (char *args, int from_tty)
{
  if (current_checkpoint == NULL)
    error ("No current checkpoint");
  else if (current_checkpoint->lprev == NULL)
    error ("No previous checkpoint to roll back to");
  else
    rollback_to_checkpoint (current_checkpoint->lprev);
}

static void
redo_command (char *args, int from_tty)
{
  if (current_checkpoint == NULL)
    error ("No current checkpoint");
  else if (current_checkpoint->lnext == NULL)
    error ("No next checkpoint to roll forward to");
  else
    rollback_to_checkpoint (current_checkpoint->lnext);
}

static void
now_command (char *args, int from_tty)
{
  if (original_latest_checkpoint == NULL)
    error ("No original latest checkpoint");

  rollback_to_checkpoint (original_latest_checkpoint);
}

/* Given a checkpoint, scrub it out of the system.  */

void
delete_checkpoint (struct checkpoint *cp)
{
  struct checkpoint *cpi;
  struct memcache *mc;

  /* First disentangle all the logical connections.  */
  for (cpi = checkpoint_list; cpi != NULL; cpi = cpi->next)
    {
      if (cpi->lnext == cp)
	cpi->lnext = cp->lnext;
      if (cpi->lprev == cp)
	cpi->lprev = cp->lprev;
    }

  if (current_checkpoint == cp)
    current_checkpoint = NULL;
  /* Note that we don't want to move this to any other checkpoint,
     because this is the only checkpoint that can restore to a
     pre-all-undo state.  */
  if (original_latest_checkpoint == cp)
    original_latest_checkpoint = NULL;

  /* Now disconnect from the main list.  */
  if (cp == checkpoint_list)
    checkpoint_list = cp->next;
  if (cp == last_checkpoint)
    last_checkpoint = cp->prev;
  if (cp->prev)
    cp->prev->next = cp->next;
  if (cp->next)
    cp->next->prev = cp->prev;

  for (mc = cp->mem; mc != NULL; mc = mc->next)
    {
      /* (should dealloc caches) */
    }

  if (cp->pid)
    {
      kill (cp->pid, 9);
    }

  /* flagging for debugging purposes */
  cp->type = 'D';
}

/* Call FUNCTION on each of the checkpoints whose numbers are given in
   ARGS.  */

static void
map_checkpoint_numbers (char *args, void (*function) (struct checkpoint *))
{
  char *p = args;
  char *p1;
  int num;
  struct checkpoint *cpi;
  int match;

  if (p == 0)
    error_no_arg (_("one or more checkpoint numbers"));

  while (*p)
    {
      match = 0;
      p1 = p;

      num = get_number_or_range (&p1);
      if (num == 0)
	{
	  warning (_("bad checkpoint number at or near '%s'"), p);
	}
      else
	{
	  for (cpi = checkpoint_list; cpi != NULL; cpi = cpi->next)
	    if (cpi->number == num)
	      {
		match = 1;
		function (cpi);
	      }
	  if (match == 0)
	    printf_unfiltered (_("No checkpoint number %d.\n"), num);
	}
      p = p1;
    }
}

static void
delete_checkpoint_command (char *args, int from_tty)
{
  dont_repeat ();

  if (args == 0)
    {
      /* Ask user only if there are some breakpoints to delete.  */
      if (!from_tty
	  || (checkpoint_list && query (_("Delete all checkpoints? "))))
	{
	  /* Keep deleting from the front until the list empties,
	     avoid pointer confusion.  */
	  while (checkpoint_list)
	    delete_checkpoint (checkpoint_list);
	}
    }
  else
    map_checkpoint_numbers (args, delete_checkpoint);

}

#if 0 /* comment out for now */
static void
reverse_step_command (char *args, int from_tty)
{
  struct checkpoint *cp, *prev;
  gdb_byte reg_buf[MAX_REGISTER_SIZE];
  LONGEST pc, prev_pc;
  struct symtab_and_line sal, prev_sal;

  if (current_checkpoint == NULL)
    {
      printf ("no cp!\n");
      return;
    }

  if (current_checkpoint->immediate_prev == NULL)
    {
      /* re-execute to find old state */
    }

  if (current_checkpoint->immediate_prev)
    {
      cp = current_checkpoint->immediate_prev;
      prev = cp->immediate_prev;
      while (prev)
	{
	  regcache_cooked_read (cp->regs, PC_REGNUM, reg_buf);
	  pc = *((LONGEST *) reg_buf);
	  sal = find_pc_line (pc, 0);
	  regcache_cooked_read (prev->regs, PC_REGNUM, reg_buf);
	  prev_pc = *((LONGEST *) reg_buf);
	  prev_sal = find_pc_line (prev_pc, 0);
	  if (prev_sal.line != sal.line)
	    {
	      rollback_to_checkpoint (cp);
	      return;
	    }
	  cp = prev;
	  prev = cp->immediate_prev;
	  if (cp == prev)
	    {
	      printf("weird...");
	      return;
	    }
	}
      printf ("no prev line found\n");
    }
  else
    printf("no cp?\n");
}

static void
reverse_stepi_command (char *args, int from_tty)
{
  if (current_checkpoint == NULL)
    {
      printf ("no cp!\n");
      return;
    }

  if (current_checkpoint->immediate_prev == NULL)
    {
      /* re-execute to find old state */
    }

  if (current_checkpoint->immediate_prev)
    rollback_to_checkpoint (current_checkpoint->immediate_prev);
  else
    printf("no cp?\n");
}
#endif

/* Clear out all accumulated checkpoint stuff.  */

void
clear_all_checkpoints ()
{
  struct checkpoint *cp;

  for (cp = checkpoint_list; cp != NULL; cp = cp->next)
    {
      delete_checkpoint (cp);
    }
  checkpoint_list = last_checkpoint = NULL;
  current_checkpoint = NULL;
  checkpoint_count = 1;
  rolled_back = 0;
  collecting_checkpoint = 0;
}

void
set_max_checkpoints (char *args, int from_tty,
		     struct cmd_list_element *c)
{
  prune_checkpoint_list ();
}

/* Catch SIGTERM so we can be sure to get rid of any checkpoint forks that are
   hanging around.  */

static void
sigterm_handler (int signo)
{
  struct checkpoint *cp;

  printf ("Handling sigterm, killing all checkpoint forks.");

  /* Note that we don't need anything more than the kills, because GDB
     as a whole is about to go away.  */
  for (cp = checkpoint_list; cp != NULL; cp = cp->next)
    {
      if (cp->pid)
	kill (cp->pid, 9);
    }
  exit (0);
}

void
_initialize_checkpoint (void)
{


  add_com ("create-checkpoint", class_obscure, create_checkpoint_command,
	   "create a checkpoint");
  add_com_alias ("cc", "create-checkpoint", class_obscure, 1);
  add_com ("rollback", class_obscure, rollback_to_checkpoint_command,
	   "roll back to a checkpoint");
  add_com ("undo", class_obscure, undo_command,
	   "back to last checkpoint");
  add_com ("redo", class_obscure, redo_command,
	   "forward to next checkpoint");
  add_com ("now", class_obscure, now_command,
	   "go to latest original execution line");
#if 0 /* commented out until working on again */
  add_com ("rs", class_obscure, reverse_step_command,
	   "reverse-step by one line");
  add_com ("rsi", class_obscure, reverse_stepi_command,
	   "reverse-step by one instruction");
  add_com ("rx", class_obscure, re_execute_command,
	   "execute up to a given place");
#endif

  add_cmd ("checkpoints", class_obscure, delete_checkpoint_command, _("\
Delete specified checkpoints.\n\
Arguments are checkpoint numbers, separated by spaces.\n\
No argument means delete all checkpoints."),
	   &deletelist);

  add_info ("checkpoints", checkpoints_info, "help");

  add_setshow_boolean_cmd ("checkpointing", class_support, &auto_checkpointing, _("\
Set automatic creation of checkpoints."), _("\
Show automatic creation of checkpoints."), NULL,
			   NULL,
			   NULL,
			   &setlist, &showlist);
  add_setshow_zinteger_cmd ("max-checkpoints", class_support, &max_checkpoints, _("\
Set the maximum number of checkpoints allowed (-1 == unlimited)."), _("\
Show the maximum number of checkpoints allowed (-1 == unlimited)."), NULL,
			   set_max_checkpoints,
			   NULL,
			   &setlist, &showlist);
  add_setshow_boolean_cmd ("subsystem-checkpointing", class_support, &subsystem_checkpointing, _("\
Set checkpointing of subsystems."), _("\
Show checkpointing of subsystems."), NULL,
			   NULL,
			   NULL,
			   &setlist, &showlist);
  add_setshow_boolean_cmd ("forking-checkpoints", class_support, &forking_checkpoints, _("\
Set forking to create checkpoints."), _("\
Show forking to create checkpoints."), NULL,
			   NULL,
			   NULL,
			   &setlist, &showlist);
}
