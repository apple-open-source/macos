/* Register coalescing code for GNU compiler.
   Contributed by Jan Hubicka
   Copyright (C) 2002 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "input.h"
#include "tree.h"
#include "rtl.h"
#include "toplev.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "flags.h"
#include "basic-block.h"
#include "sbitmap.h"
#include "output.h"

typedef sbitmap *cgraph;
static void mark_conflict	PARAMS ((rtx, rtx, void *));
static cgraph build_cgraph	PARAMS ((void));
static int replace		PARAMS ((rtx *, void *));

struct mark_conflict_data
{
  cgraph graph;
  regset live;
};

/* Mark conflict betwen X and live registers.  */

static void
mark_conflict (x, set, data)
     rtx x;
     rtx set;
     void *data;
{
  struct mark_conflict_data *d = (struct mark_conflict_data *)data;
  regset live = d->live;
  cgraph graph = (cgraph) d->graph;
  int skip = -1;

  /* In the case of reg-reg copy, we may safely omit the conflict
     between source and destination and coalesce both operands into
     single register.  */
  if (GET_CODE (set) == SET
      && REG_P (XEXP (set, 0))
      && REG_P (XEXP (set, 1)))
    skip = REGNO (SET_SRC (set));

  if (GET_CODE (x) == SUBREG)
    x = SUBREG_REG (x);
  if (REG_P (x))
    {
      int regno = REGNO (x);
      int regno2;
      EXECUTE_IF_SET_IN_BITMAP (live, 0, regno2,
				if (regno2 != skip)
				  {
				    SET_BIT (graph[regno], regno2);
				    SET_BIT (graph[regno2], regno);
				  });
    }
}

/* Build conflict graph.  The graph is accurate only for pseudo registers, but
   that is OK for us.  */

static cgraph
build_cgraph ()
{
  cgraph graph = sbitmap_vector_alloc (max_regno + 1, max_regno + 1);
  regset live;
  regset_head live_head;
  basic_block bb;

  live = INITIALIZE_REG_SET (live_head);

  life_analysis (get_insns (), rtl_dump_file,
		 PROP_DEATH_NOTES | PROP_EQUAL_NOTES
		 | PROP_SCAN_DEAD_CODE | PROP_KILL_DEAD_CODE);
  graph = sbitmap_vector_alloc (max_regno + 1, max_regno + 1);
  sbitmap_vector_zero (graph, max_regno + 1);
  FOR_EACH_BB (bb)
    {
      rtx insn;
      struct propagate_block_info *pbi;

      COPY_REG_SET (live, bb->global_live_at_end);
      pbi = init_propagate_block_info (bb, live, NULL, NULL, PROP_EQUAL_NOTES);

      for (insn = bb->end; insn != bb->head; insn = PREV_INSN (insn))
	{
	  if (INSN_P (insn))
	    {
	      struct mark_conflict_data d;

	      d.graph = graph;
	      d.live = live;
	      note_stores (PATTERN (insn), mark_conflict, &d);
	      propagate_one_insn (pbi, insn);
	    }
	}
      free_propagate_block_info (pbi);
    }
  FREE_REG_SET (live);
  count_or_remove_death_notes (NULL, 1);
  return graph;
}

/* Replace registers in X using map passed as DATA.  */

static int
replace (x, data)
     rtx *x;
     void *data;
{
  rtx *map = (rtx *) data;
  if (*x && REG_P (*x) && map[REGNO (*x)])
    *x = map[REGNO (*x)];
  return 0;
}

/* Do stand alone register coalescing pass.  */

void
coalesce ()
{
  cgraph graph = build_cgraph ();
  rtx *replace_reg_by = xcalloc (sizeof (rtx *), max_regno + 1);
  basic_block bb;

#if 0
  if (rtl_dump_file)
    dump_sbitmap_vector (rtl_dump_file, "Conflict graph", "reg", graph,
			 max_regno + 1);
#endif

  FOR_EACH_BB (bb)
    {
      rtx insn;

      for (insn = bb->head; insn != NEXT_INSN (bb->end);
	   insn = NEXT_INSN (insn))
	if (INSN_P (insn))
	  {
	    rtx set = single_set (insn);
	    rtx src, dest;
	    int regno1, regno2, regno3;
	    int i;

	    if (!set || GET_CODE (set) != SET)
	      continue;
	    src = SET_SRC (set);
	    dest = SET_DEST (set);
	    if (GET_CODE (src) == SUBREG
		&& subreg_lowpart_offset (GET_MODE (src),
					  GET_MODE (SUBREG_REG (src))) ==
		SUBREG_BYTE (src))
	      src = SUBREG_REG (src);
	    if (GET_CODE (dest) == SUBREG
		&& subreg_lowpart_offset (GET_MODE (dest),
					  GET_MODE (SUBREG_REG (dest))) ==
		SUBREG_BYTE (dest))
	      dest = SUBREG_REG (dest);
	    if (!REG_P (src) || !REG_P (dest)
		|| GET_MODE (src) != GET_MODE (dest))
	      continue;
	    regno1 = REGNO (src);
	    regno2 = REGNO (dest);
	    if (replace_reg_by[regno1])
	      src = replace_reg_by[regno1], regno1 = REGNO (src);
	    if (replace_reg_by[regno2])
	      dest = replace_reg_by[regno2], regno1 = REGNO (dest);
	    if (regno1 < FIRST_PSEUDO_REGISTER
		|| regno2 < FIRST_PSEUDO_REGISTER
		|| TEST_BIT (graph[regno1], regno2) || regno1 == regno2)
	      continue;

	    EXECUTE_IF_SET_IN_SBITMAP (graph[regno2], 0, regno3,
				       SET_BIT (graph[regno1], regno3);
				       SET_BIT (graph[regno3], regno1);
	      );
	    for (i = 0; i <= max_regno; i++)
	      if (replace_reg_by [i] == dest)
		replace_reg_by[i] = src;
	    replace_reg_by [regno2] = src;
	    if (rtl_dump_file)
	      fprintf (rtl_dump_file, "Coalescing %i and %i for insn %i\n",
		       regno1, regno2, INSN_UID (insn));
	  }
    }
  FOR_EACH_BB (bb)
    {
      rtx insn;

      for (insn = bb->head; insn != NEXT_INSN (bb->end);
	   insn = NEXT_INSN (insn))
	if (INSN_P (insn))
	  for_each_rtx (&insn, replace, replace_reg_by);
    }

  sbitmap_vector_free (graph);
  delete_trivially_dead_insns (get_insns (), max_reg_num ());
  free (replace_reg_by);
}
