/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

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

#include "ppc-macosx-regs.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-cfm.h"
#include "macosx-nat-dyld-info.h"

#define TRUE_FALSE_ALREADY_DEFINED

#include "defs.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "symfile.h"
#include "frame.h"
#include "breakpoint.h"
#include "symtab.h"
#include "annotate.h"
#include "target.h"
#include "breakpoint.h"
#include "gdbcore.h"
#include "event-top.h"

#define IS_BC_x(instruction) \
((((instruction) & 0xFC000000) >> 26) == 16)

#define IS_B_x(instruction) \
((((instruction) & 0xFC000000) >> 26) == 18)

#define IS_BCLR_x(instruction) \
(((((instruction) & 0xFC000000) >> 26) == 19) && ((((instruction) & 0x000007FE) >> 1) == 16))

#define IS_BCCTR_x(instruction) \
(((((instruction) & 0xFC000000) >> 26) == 19) && ((((instruction) & 0x000007FE) >> 1) == 528))

#define WILL_LINK(instruction) \
(((instruction) & 0x00000001) != 0)

extern int metrowerks_stepping;
extern CORE_ADDR metrowerks_step_func_start;
extern CORE_ADDR metrowerks_step_func_end;

static void
metrowerks_stepping_cleanup (void *unusued)
{
  metrowerks_stepping = 0;
  metrowerks_step_func_start = 0;
  metrowerks_step_func_end = 0;
}

static void
metrowerks_step (CORE_ADDR range_start, CORE_ADDR range_stop, int step_into)
{
  struct frame_info *frame = NULL;
  CORE_ADDR pc = 0;

  /* When single stepping in assembly, the plugin passes (start + 1)
     as the stop address. Round the stop address up to the next valid
     instruction */

  if ((range_stop & ~0x3) != range_stop)
      range_stop = ((range_stop + 4) & ~0x3);

  pc = read_pc();

  if (range_start >= range_stop)
    error ("invalid step range (the stop address must be greater than the start address)");

  if (pc < range_start)
    error ("invalid step range ($pc is 0x%lx, less than the stop address of 0x%lx)",
	   (unsigned long) pc, (unsigned long) range_start);
  if (pc == range_stop)
    error ("invalid step range ($pc is 0x%lx, equal to the stop address of 0x%lx)",
	   (unsigned long) pc, (unsigned long) range_stop);
  if (pc > range_stop)
    error ("invalid step range ($pc is 0x%lx, greater than the stop address of 0x%lx)", 
	   (unsigned long) pc, (unsigned long) range_stop);

  clear_proceed_status ();
  
  frame = get_current_frame ();
  if (frame == NULL)
    error ("No current frame");
  step_frame_address = FRAME_FP (frame);
  step_sp = read_sp ();
  
  step_range_start = range_start;
  step_range_end = range_stop;
  step_over_calls = step_into ? STEP_OVER_NONE : STEP_OVER_ALL;
  
  step_multi = 0;
  
  metrowerks_stepping = 1;
  proceed ((CORE_ADDR) -1, TARGET_SIGNAL_DEFAULT, 1);
  make_exec_cleanup (metrowerks_stepping_cleanup, NULL);
}

extern int strip_bg_char (char **);

static void
metrowerks_step_command (char *args, int from_tty)
{
  int async_exec = 0;
  CORE_ADDR range_start = 0;
  CORE_ADDR range_stop = 0;
  int step_into = 0;
  int num_args = 0;
  char **argv = NULL;

  if (args != NULL)
    async_exec = strip_bg_char (&args);

  if (event_loop_p && async_exec && !target_can_async_p ())
    error ("Asynchronous execution not supported on this target.");

  /* If we don't get a request of running in the bg, then we need
     to simulate synchronous (fg) execution. */
  if (event_loop_p && !async_exec && target_can_async_p ())
    {
      async_disable_stdin ();
    }

  argv = buildargv (args);
  
  if (argv == NULL)
    {
      num_args = 0;
    } 
  else
    {
      num_args = 0;
      while (argv[num_args] != NULL)
	num_args++;
    }

  if (num_args != 3 && num_args != 5)
    error ("Usage: metrowerks-step <start> <stop> <step-into> ?<func_start> <func_end>?");

  range_start = strtoul (argv[0], NULL, 16);
  range_stop = strtoul (argv[1], NULL, 16);
  step_into = strtoul (argv[2], NULL, 16);

  if (num_args == 5)
    {
      metrowerks_step_func_start = strtoul (argv[3], NULL, 16);
      metrowerks_step_func_end = strtoul (argv[4], NULL, 16);
    }
  else
    {
      metrowerks_step_func_start = 0;
      metrowerks_step_func_end = 0;
    }

  if (!target_has_execution)
    error ("The program is not being run.");

  metrowerks_step (range_start, range_stop, step_into);
}

bfd *FindContainingBFD (CORE_ADDR address)
{
  struct target_stack_item *aTarget;
  bfd *result = NULL;

  for (aTarget = target_stack; (result == NULL) && (aTarget != NULL); aTarget = aTarget->next) {
    
    struct section_table* sectionTable;

    if ((NULL == aTarget->target_ops) || (NULL == aTarget->target_ops->to_sections)) {
      continue;
    }
        
    for (sectionTable = &aTarget->target_ops->to_sections[0];
	 (result == NULL) && (sectionTable < aTarget->target_ops->to_sections_end);
	 sectionTable++)
      {
	if ((address >= sectionTable->addr) && (address < sectionTable->endaddr)) {
	  result = sectionTable->bfd;
	}
      }
  }
  
  return result;
}

static void
metrowerks_address_to_name_command (char* args, int from_tty)
{
  CORE_ADDR address;
  bfd *aBFD;

  address = strtoul (args, NULL, 16);

  if (annotation_level > 1) {
    printf("\n\032\032fragment-name ");
  }

  aBFD = FindContainingBFD (address);
  if (aBFD != NULL) {
    printf_unfiltered ("%s\n", aBFD->filename);
    return;
  }

  printf_unfiltered ("[unknown]\n");
}

void
_initialize_metrowerks(void)
{
    add_com ("metrowerks-step", class_obscure, metrowerks_step_command,
	     "GDB as MetroNub command");

    add_com ("metrowerks-address-to-name", class_obscure, metrowerks_address_to_name_command,
	     "GDB as MetroNub command");
}
