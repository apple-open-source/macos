/* Basic block reordering routines for the GNU compiler.
   Copyright (C) 2000, 2002 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* References:

   "Profile Guided Code Positioning"
   Pettis and Hanson; PLDI '90.

   TODO:

   (1) Consider:

		if (p) goto A;		// predict taken
		foo ();
	      A:
		if (q) goto B;		// predict taken
		bar ();
	      B:
		baz ();
		return;

       We'll currently reorder this as

		if (!p) goto C;
	      A:
		if (!q) goto D;
	      B:
		baz ();
		return;
	      D:
		bar ();
		goto B;
	      C:
		foo ();
		goto A;

       A better ordering is

		if (!p) goto C;
		if (!q) goto D;
	      B:
		baz ();
		return;
	      C:
		foo ();
		if (q) goto B;
	      D:
		bar ();
		goto B;

       This requires that we be able to duplicate the jump at A, and
       adjust the graph traversal such that greedy placement doesn't
       fix D before C is considered.

   (2) Coordinate with shorten_branches to minimize the number of
       long branches.

   (3) Invent a method by which sufficiently non-predicted code can
       be moved to either the end of the section or another section
       entirely.  Some sort of NOTE_INSN note would work fine.

       This completely scroggs all debugging formats, so the user
       would have to explicitly ask for it.
*/

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "flags.h"
#include "output.h"
#include "cfglayout.h"
#include "target.h"
/* APPLE LOCAL begin - rarely executed bb optimization */
#include "langhooks.h" 
/* APPLE LOCAL end - rarely executed bb optimization */

/* Local function prototypes.  */
static void make_reorder_chain		PARAMS ((void));
static basic_block make_reorder_chain_1	PARAMS ((basic_block, basic_block));
/* APPLE LOCAL begin - rarely executed bb optimization */
static void find_rarely_executed_basic_blocks PARAMS ((void));
static void mark_bb_for_unlikely_executed_section  PARAMS((basic_block));
static void fix_branches_for_unexecuted_code PARAMS ((void));
/* APPLE LOCAL end - rarely executed bb optimization */


/* Compute an ordering for a subgraph beginning with block BB.  Record the
   ordering in RBI()->index and chained through RBI()->next.  */

static void
make_reorder_chain ()
{
  basic_block prev = NULL;
  basic_block next, bb;

  /* Loop until we've placed every block.  */
  do
    {
      next = NULL;

      /* Find the next unplaced block.  */
      /* ??? Get rid of this loop, and track which blocks are not yet
	 placed more directly, so as to avoid the O(N^2) worst case.
	 Perhaps keep a doubly-linked list of all to-be-placed blocks;
	 remove from the list as we place.  The head of that list is
	 what we're looking for here.  */

      FOR_EACH_BB (bb)
	if (! RBI (bb)->visited)
	  {
	    next = bb;
	    break;
	  }

      if (next)
	prev = make_reorder_chain_1 (next, prev);
    }
  while (next);
  RBI (prev)->next = NULL;
}

/* A helper function for make_reorder_chain.

   We do not follow EH edges, or non-fallthru edges to noreturn blocks.
   These are assumed to be the error condition and we wish to cluster
   all of them at the very end of the function for the benefit of cache
   locality for the rest of the function.

   ??? We could do slightly better by noticing earlier that some subgraph
   has all paths leading to noreturn functions, but for there to be more
   than one block in such a subgraph is rare.  */

static basic_block
make_reorder_chain_1 (bb, prev)
     basic_block bb;
     basic_block prev;
{
  edge e;
  basic_block next;
  rtx note;

  /* Mark this block visited.  */
  if (prev)
    {
 restart:
      RBI (prev)->next = bb;

      if (rtl_dump_file && prev->next_bb != bb)
	fprintf (rtl_dump_file, "Reordering block %d after %d\n",
		 bb->index, prev->index);
    }
  else
    {
      if (bb->prev_bb != ENTRY_BLOCK_PTR)
	abort ();
    }
  RBI (bb)->visited = 1;
  prev = bb;

  if (bb->succ == NULL)
    return prev;

  /* Find the most probable block.  */

  next = NULL;
  if (any_condjump_p (bb->end)
      && (note = find_reg_note (bb->end, REG_BR_PROB, 0)) != NULL)
    {
      int taken, probability;
      edge e_taken, e_fall;

      probability = INTVAL (XEXP (note, 0));
      taken = probability > REG_BR_PROB_BASE / 2;

      /* Find the normal taken edge and the normal fallthru edge.

	 Note, conditional jumps with other side effects may not
	 be fully optimized.  In this case it is possible for
	 the conditional jump to branch to the same location as
	 the fallthru path.

	 We should probably work to improve optimization of that
	 case; however, it seems silly not to also deal with such
	 problems here if they happen to occur.  */

      e_taken = e_fall = NULL;
      for (e = bb->succ; e ; e = e->succ_next)
	{
	  if (e->flags & EDGE_FALLTHRU)
	    e_fall = e;
	  else if (! (e->flags & EDGE_EH))
	    e_taken = e;
	}

      next = ((taken && e_taken) ? e_taken : e_fall)->dest;
    }

  /* In the absence of a prediction, disturb things as little as possible
     by selecting the old "next" block from the list of successors.  If
     there had been a fallthru edge, that will be the one.  */
  /* Note that the fallthru block may not be next any time we eliminate
     forwarder blocks.  */
  if (! next)
    {
      for (e = bb->succ; e ; e = e->succ_next)
	if (e->flags & EDGE_FALLTHRU)
	  {
	    next = e->dest;
	    break;
	  }
	else if (e->dest == bb->next_bb)
	  {
	    if (! (e->flags & (EDGE_ABNORMAL_CALL | EDGE_EH)))
	      next = e->dest;
	  }
    }

  /* Make sure we didn't select a silly next block.  */
  if (! next || next == EXIT_BLOCK_PTR || RBI (next)->visited)
    next = NULL;

  /* Recurse on the successors.  Unroll the last call, as the normal
     case is exactly one or two edges, and we can tail recurse.  */
  for (e = bb->succ; e; e = e->succ_next)
    if (e->dest != EXIT_BLOCK_PTR
	&& ! RBI (e->dest)->visited
	&& e->dest->succ
	&& ! (e->flags & (EDGE_ABNORMAL_CALL | EDGE_EH)))
      {
	if (next)
	  {
	    prev = make_reorder_chain_1 (next, prev);
	    next = RBI (e->dest)->visited ? NULL : e->dest;
	  }
	else
	  next = e->dest;
      }
  if (next)
    {
      bb = next;
      goto restart;
    }

  return prev;
}

/* APPLE LOCAL begin - rarely executed bb optimization */

/* Find the basic blocks that are rarely executed and need to be moved to
   a separate section of the .o file (to cut down on paging and improve
   cache locality) */

static void
find_rarely_executed_basic_blocks ()
{
  basic_block bb;

  FOR_EACH_BB (bb)
    if (probably_never_executed_bb_p (bb))
      mark_bb_for_unlikely_executed_section (bb);
}
 
/* Add NOTE_INSN_UNLIKELY_EXECUTED_CODE to top of basic block */
  
static void
mark_bb_for_unlikely_executed_section (bb) 
     basic_block bb;
{
  rtx cur_insn;
  rtx insert_insn = NULL;
  rtx new_note;
  
  /* Find first non-note instruction and insert new NOTE before it (as
     long as new NOTE is not first instruction in basic block) */
  
  for (cur_insn = bb->head; cur_insn != bb->end; cur_insn = NEXT_INSN (cur_insn))
    {
      if ((GET_CODE (cur_insn) != NOTE)  
	  && (GET_CODE (cur_insn) != CODE_LABEL))
	{
	  insert_insn = cur_insn;
	  break;
	}
    }
  
  /* See if it is appropriate to insert note before final instruction in BB */
  
  if ((!insert_insn) 
      && (GET_CODE (cur_insn) != NOTE) 
      && (GET_CODE (cur_insn) != CODE_LABEL)) 
    insert_insn = cur_insn;
  
  /* Insert note and assign basic block number to it */
  
  if (insert_insn) 
    {
      new_note = emit_note_before (NOTE_INSN_UNLIKELY_EXECUTED_CODE, insert_insn);
      NOTE_BASIC_BLOCK (new_note) = bb;
    }
}

static basic_block
find_jump_block  (old_label, block_list, max_idx)
     rtx old_label;
     basic_block *block_list;
     int max_idx;
{
  int i;
  rtx set_src;
  rtx cur_insn;
  rtx jump_insn;
  basic_block jump_block = NULL;
  basic_block cur_bb;

  if (GET_CODE (old_label) == LABEL_REF)
    {
      for (i = 0; i < max_idx; i++)
	{
	  cur_bb = block_list[i];
	  jump_insn = NULL;
	  for (cur_insn = cur_bb->head; cur_insn != cur_bb->end;
	       cur_insn = NEXT_INSN (cur_insn))
	    {
	      if (GET_CODE (cur_insn) == JUMP_INSN)
		{
		  jump_insn = cur_insn;
		  break;
		}
	    }
	  
	  if ((!jump_insn) && (GET_CODE (cur_insn) == JUMP_INSN))
	    jump_insn = cur_insn;

	  if (jump_insn)
	    {
	      if (GET_CODE (PATTERN (jump_insn)) == SET)
		{
		  set_src = SET_SRC (PATTERN (jump_insn));
		  if ((GET_CODE (set_src) == LABEL_REF)
		      && (XEXP (set_src, 0) == XEXP (old_label, 0)))
		    {
		      jump_block = cur_bb;
		      break;
		    }
		}
	    }
	}
    }

  return jump_block;
}

/* Basic blocks containing NOTE_INSN_UNLIKELY_EXECUTED_CODE will be
   put in a separate section of the .o file, to reduce paging and
   improve cache performance (hopefully).  Therefore conditional
   branches between 'hot' & 'cold' basic blocks need a level of
   indirection inserted, to make sure they can cover the distance
   (i.e. a (short) conditional branch to a (long) unconditional
   branch).  */

static void
fix_branches_for_unexecuted_code ()
{
  rtx cur_insn, old_jump, new_jump, jump_insn, old_label, new_label, label;
  rtx new_note, new_note2, set_src, barrier, fall_thru_label;
  edge cur_edge, crossing_edge, fall_thru, succ1, succ2, new_edge, e, cond_jump;
  basic_block cur_bb, new_bb, dest, src, last_bb, last_hot_bb;

  int *bb_colors;
  int *bb_has_label;
  int *bb_has_jump;
  edge *crossing_edges;
  basic_block *block_list;

  int i;
  int block_list_idx = 0;
  int n_crossing_edges;
  int found;
  int max_array_size;
  int invert_worked;
  bool cond_jump_crosses;
  
  bb_colors = xcalloc (2*last_basic_block, sizeof (int));
  bb_has_label = xcalloc (2*last_basic_block, sizeof (int));
  bb_has_jump = xcalloc (2*last_basic_block, sizeof (int));
  crossing_edges = xcalloc (2*last_basic_block, sizeof (edge));
  block_list = xcalloc (4*last_basic_block, sizeof (basic_block));
  
  max_array_size = 2*last_basic_block;

  /* Color basic blocks: red(0) => executed lots; black(1) => unlikely to be
     executed (the appropriate NOTE instructions were added during 
     mark_bb_for_unlikely_executed_section).  Also find out which basic blocks
     already have labels (may need to add some later) and jumps. */
  
  found = 0;
  FOR_EACH_BB (cur_bb) 
    {
      for (cur_insn = cur_bb->head; cur_insn != cur_bb->end; 
 	   cur_insn = NEXT_INSN (cur_insn)) 
 	{
 	  if ((GET_CODE (cur_insn) == NOTE) 
	      && (NOTE_LINE_NUMBER (cur_insn) == NOTE_INSN_UNLIKELY_EXECUTED_CODE))
 	    {
 	      bb_colors[cur_bb->index] = 1;
 	      found++;
 	    }
 	  else if (GET_CODE (cur_insn) == CODE_LABEL)

	    bb_has_label[cur_bb->index] = 1;

 	  else if (GET_CODE (cur_insn) == JUMP_INSN)

	    bb_has_jump[cur_bb->index] = 1;
	  
 	} /* for each instruction */
      
      if (cur_insn == cur_bb->end) 
 	{
 	  if ((GET_CODE (cur_insn) == NOTE) 
	      && (NOTE_LINE_NUMBER (cur_insn) == NOTE_INSN_UNLIKELY_EXECUTED_CODE))
 	    {
 	      bb_colors[cur_bb->index] = 1;
 	      found++;
 	    }
 	  else if (GET_CODE (cur_insn) == CODE_LABEL)

	    bb_has_label[cur_bb->index] = 1;

 	  else if (GET_CODE (cur_insn) == JUMP_INSN)

	    bb_has_jump[cur_bb->index] = 1;

 	}
      
    } /* FOR_EACH_BB */
  
  /* Find all edges that cross a color boundary */
  
  i=0;
  FOR_EACH_BB (cur_bb) 
    {
      for (cur_edge = cur_bb->succ; cur_edge; cur_edge = cur_edge->succ_next) 
 	{
 	  if ((cur_edge->src) && (cur_edge->dest))
 	    {
 	      if ((cur_edge->src->index >= 0)
		  && (cur_edge->dest->index >= 0)
		  && (bb_colors[cur_edge->src->index] !=
		      bb_colors[cur_edge->dest->index]))
 		{
 		  crossing_edges[i] = cur_edge;
 		  i++;
 		}
 	    }
 	}
    } /* FOR_EACH_BB */
  
  n_crossing_edges = i;
  
  /* If any destination of a crossing edge does not have a label, add label;
     Convert any fall-through crossing edges (for blocks that do not contain
     a jump) to unconditional jumps.
  */
  
  for (i=0; i < n_crossing_edges; i++) 
    {
      if (crossing_edges[i]) 
 	{
 	  src = crossing_edges[i]->src; 
 	  dest = crossing_edges[i]->dest;
	  
 	  /* Make sure dest has a label */
 	  
 	  if (dest && (dest->index != -2)) /* Not EXIT BLOCK */
 	    {
 	      if (!(bb_has_label[dest->index])) 
 		{
 		  rtx new_label = gen_label_rtx ();
 		  emit_label_before (new_label, dest->head);
		  dest->head = new_label;
		  bb_has_label[dest->index] = 1;
 		}
	  
	      /* Make sure source block ends with a jump */
	  
	      if (src && (src->index != -1)) /* Not ENTRY BLOCK */ 
		{
		  if (!(bb_has_jump[src->index])) /* bb just falls through */
		    {
		      /* make sure there's only one successor */
		      if ((src->succ) && (src->succ->succ_next == NULL))
			{
			  /* find label in dest block */
			  cur_insn = dest->head;
			  found = 0;
			  label = NULL;
			  while ((cur_insn != dest->end) && !found)
			    {
			      if (GET_CODE(cur_insn) == CODE_LABEL)
				{
				  label= cur_insn;
				  found = 1;
				} 
			      else
				cur_insn = NEXT_INSN (cur_insn);
			    }
			  
			  /* check last insn in bb (not checked in loop above) */
			  
			  if (cur_insn == dest->end)
			    if (GET_CODE(cur_insn) == CODE_LABEL)
			      {
				label= cur_insn;
				found = 1;
			      }
			  
			  if (found) 
			    {
			      new_jump = emit_jump_insn_after (gen_jump (label), 
							       src->end);
			      barrier = emit_barrier_after (new_jump);
			      JUMP_LABEL (new_jump) = label;
			      LABEL_NUSES (label) += 1;
			      RBI (src)->footer = unlink_insn_chain (barrier, 
								     barrier);
			    }
			}
		      else
			{ 
			  /* basic block has two successors, but doesn't end in
			     a jump; something is wrong here! */
			  abort();
			}
		    }
		}
 	    }
 	}
    }
  
  /* Find all crossing edges that are result of unconditional jumps:
     mark the unconditional jumps as do-not-shorten                         */
  
  for (i = 0; i < n_crossing_edges; i++)
    {
      e = crossing_edges[i];
      
      if (e && e->src)
 	{
 	  src = e->src;
 	  jump_insn = src->end;
	  
 	  if (GET_CODE(jump_insn) == JUMP_INSN) 
 	    {
 	      /* if only one successor must be unconditional jump */
 	      if ((src->succ) && (src->succ->succ_next == NULL))
 		{
 		  new_note = emit_note_before (NOTE_INSN_DONT_SHORTEN_BRANCH, 
 					       jump_insn);
 		  NOTE_BASIC_BLOCK (new_note) = src;
 		}
 	    }
 	}
      
    }
  
  /* ======================================================================= */
  
  /* Find any bb's where fall-through edge is a crossing edge (note that
     these bb's must also contain a conditional jump; we've already dealt
     with fall-through edges for blocks that didn't have a conditional jump); 
     convert the fall-through edge to non-crossing edge by inserting a new bb to
     fall-through into.  The new bb will contain an unconditional jump 
     (crossing edge) to the original fall through destination.
  */
  
  FOR_EACH_BB (cur_bb)
    {
      fall_thru = NULL;
      succ1 = cur_bb->succ;
      if (succ1)
 	succ2 = succ1->succ_next;
      else
 	succ2 = NULL;
      
      /* find fall-through edge */
      
      if ((succ1) 
	  && (succ1->flags & EDGE_FALLTHRU))
	{
	  fall_thru = succ1;
	  cond_jump = succ2;
	}
      else if ((succ2) 
	       && (succ2->flags & EDGE_FALLTHRU))
	{
	  fall_thru = succ2;
	  cond_jump = succ1;
	}
      
      
      if (fall_thru)
 	{
 	  /* check to see if fall-thru edge is a crossing edge */

	  if (scan_ahead_for_unlikely_executed_note (fall_thru->src->head) !=
	      scan_ahead_for_unlikely_executed_note (fall_thru->dest->head))
 	    {
	      basic_block temp_bb;

	      /* fall_thru edge crosses; now check cond jump edge */

	      cond_jump_crosses = true;
	      invert_worked  = 0;

	      if (cond_jump)
		{
		  if (scan_ahead_for_unlikely_executed_note (cond_jump->src->head)
		      == scan_ahead_for_unlikely_executed_note 
		                                          (cond_jump->dest->head))
		    cond_jump_crosses = false;
	      
		  /* We know the fall-thru edge crosses; if the cond jump edge
		     does NOT cross, AND the next bb in the bb sequence chain
		     would normally be the destination of the cond jump,
		     invert the jump */
		  
		  temp_bb = RBI (cur_bb)->next;

		  if ((!(cond_jump_crosses))
		      && (temp_bb == cond_jump->dest))
		    {
		      
		      /* find cond jump instruction */
		      
		      old_jump = NULL;
		      for (cur_insn = cur_bb->head; cur_insn != cur_bb->end;
			   cur_insn = NEXT_INSN (cur_insn))
			if (GET_CODE (cur_insn) == JUMP_INSN)
			  {
			    old_jump = cur_insn;
			    break;
			  }
		      
		      if ((!old_jump) && (GET_CODE (cur_insn) == JUMP_INSN))
			old_jump = cur_insn;

		      /* find label in fall_thru block */
		      
		      fall_thru_label = NULL;
		      for (cur_insn = fall_thru->dest->head;
			   cur_insn != fall_thru->dest->end;
			   cur_insn = NEXT_INSN (cur_insn))
			if (GET_CODE (cur_insn) == CODE_LABEL)
			  {
			    fall_thru_label = cur_insn;
			    break;
			  }
		      
		      if ((!fall_thru_label) 
			  && (GET_CODE (cur_insn) == CODE_LABEL))
			fall_thru_label = cur_insn;
		      
		      if (old_jump && fall_thru_label)
			invert_worked = invert_jump (old_jump, fall_thru_label,0);
		      
		      if (invert_worked)
			{
			  fall_thru->flags &= ~EDGE_FALLTHRU;
			  cond_jump->flags |= EDGE_FALLTHRU;
			  update_br_prob_note (cur_bb);
			  e = fall_thru;
			  fall_thru = cond_jump;
			  cond_jump = e;
			}
		    }
		}
	      

	      if (cond_jump_crosses || (!invert_worked))
		{

		  dest = fall_thru->dest;
	      
		  /* Make sure original fall-through dest has a label */
	      
		  if (dest->index > max_array_size)
		    abort ();
		  else if (!(bb_has_label[dest->index]))
		    {
		      new_label = gen_label_rtx();
		      emit_label_before (new_label, dest->head);
		      dest->head = new_label;
		      bb_has_label[dest->index] = 1;
		    }
		  else
		    /* find the fall-through dest label (for later use) */
		    {
		      found = 0;
		      new_label = NULL;
		      for (cur_insn = dest->head; 
			   (cur_insn != dest->end) && !found; 
			   cur_insn = NEXT_INSN (cur_insn)) 
			{
			  if (GET_CODE(cur_insn) == CODE_LABEL) 
			    {
			      new_label = cur_insn;
			      found = 1;
			    }
			}
		      
		      /* check last instruction in bb (loop above didnt't) */
		      
		      if (cur_insn == dest->end)
			if (GET_CODE(cur_insn) == CODE_LABEL) 
			  new_label = cur_insn;
		    }
		  
		  
		  /* create new bb immediately after cur_bb */
		  
		  new_bb = create_basic_block (NULL, NULL, cur_bb);
		  
		  alloc_aux_for_block (new_bb, sizeof (struct reorder_block_def));
		  RBI (new_bb)->next = RBI (cur_bb)->next;
		  RBI (cur_bb)->next = new_bb;
		  
		  /* put appropriate instructions in new_bb */
		  
		  new_jump = emit_jump_insn_after (gen_jump (new_label),
						   new_bb->head);
		  barrier = emit_barrier_after (new_jump);
		  JUMP_LABEL (new_jump) = new_label;
		  LABEL_NUSES (new_label) += 1;
		  RBI (new_bb)->footer = unlink_insn_chain (barrier, barrier);
		  
		  new_note = emit_note_before (NOTE_INSN_DONT_SHORTEN_BRANCH,
					       new_jump);
		  NOTE_BASIC_BLOCK (new_note) = new_bb;
		  
		  if (new_bb->index < max_array_size)
		    bb_has_jump[new_bb->index] = 1;
		  
		  /* make sure new fall-through bb is same "color" as
		     bb it's falling through from */
		  
		  if (scan_ahead_for_unlikely_executed_note (cur_bb->head))
		    {
		      new_note2 = emit_note_before 
			                        (NOTE_INSN_UNLIKELY_EXECUTED_CODE,
						 new_note);
		      NOTE_BASIC_BLOCK (new_note2) = new_bb;
		    }
		  
		  /* Add don't shorten label to conditional jump in cur_bb;
		     sometimes optimizer swaps jump & fall-thru edges, which
		     could mess this stuff up */
		  
		  for (old_jump = cur_bb->head; old_jump != cur_bb->end;
		       old_jump = NEXT_INSN (old_jump))
		    if (GET_CODE (old_jump) == JUMP_INSN)
		      break;
		  
		  if (GET_CODE (old_jump) == JUMP_INSN)
		    {
		      new_note = emit_note_before (NOTE_INSN_DONT_SHORTEN_BRANCH,
						   old_jump);
		      NOTE_BASIC_BLOCK (new_note) = cur_bb;
		    }
		  
		  /* Remove fall_thru as predecessor of 'dest' */
		  
		  dest = fall_thru->dest; 
		  
		  redirect_edge_succ (fall_thru, new_bb);
		  
		  /* Create a new (crossing) edge from new_bb to old fall thru 
		     dest */
		  
		  new_edge = make_edge (new_bb, dest, 0);
		  
		}
 	    }
 	}
    }
  
  /* Find last & last hot  basic blocks.  These will be used as insertion
     points for new bb's containing unconditional jumps (to cross section
     boundaries
  */

  last_hot_bb = NULL;
  FOR_EACH_BB (cur_bb) 
    {
      last_bb = cur_bb;
      if (!(scan_ahead_for_unlikely_executed_note (cur_bb->head)))
	last_hot_bb = cur_bb;
    }

  if (!(last_hot_bb))
    last_hot_bb = last_bb;

  /* ======================================================================= */
  
  /* Find all BB's with conditional jumps that are crossing edges;
     insert a new bb and make the conditional jump branch to the new
     bb instead (make the new bb same color so conditional branch won't
     be a 'crossing' edge).  Insert an unconditional jump from the
     new bb to the original destination of the conditional jump */
  
  FOR_EACH_BB (cur_bb)
    {
      crossing_edge = NULL;
      succ1 = cur_bb->succ;
      if (succ1)
 	succ2 = succ1->succ_next;
      else
 	succ2 = NULL;

      /* We already took care of fall-through edges, so only one successor
	 can be a crossing edge. */
      
      if ((succ1) 
	  && (scan_ahead_for_unlikely_executed_note (succ1->src->head) !=
	      scan_ahead_for_unlikely_executed_note (succ1->dest->head)))
	crossing_edge = succ1;
      else if ((succ2) 
	       && (scan_ahead_for_unlikely_executed_note (succ2->src->head) !=
		   scan_ahead_for_unlikely_executed_note (succ2->dest->head)))
 	crossing_edge = succ2;
      
      if (crossing_edge) 
 	{
	  if (cur_bb->index >= max_array_size)
	    abort ();
 	  else if (bb_has_jump[cur_bb->index])
 	    {
 	      /* Find the jump insn in cur_bb. */
	      
 	      found = 0;
 	      old_jump = NULL;
 	      for (cur_insn = cur_bb->head; (cur_insn != cur_bb->end) && !found; 
 		   cur_insn = NEXT_INSN (cur_insn))
 		{
 		  if (GET_CODE (cur_insn) == JUMP_INSN)
 		    {
 		      found = 1;
 		      old_jump = cur_insn;
 		    }
 		}

	      /* Check last insn in cur_bb to see if it is the jump insn  
		 (previous loop didn't do that). */

 	      if (cur_insn == cur_bb->end)
 		if (GET_CODE (cur_insn) == JUMP_INSN)
 		  old_jump = cur_insn;
	      
 	      /* Check to make sure the jump instruction is a conditional jump. */
	      
	      /*
		set_src = pc_set;
		
		if (set_src)
		set_src = SET_SRC (set_src);
		
		if (set_src && (GET_CODE(set_src) == IF_THEN_ELSE))
	      */


	      set_src = NULL_RTX;

	      if (GET_CODE (old_jump) == JUMP_INSN)
		{
		  if (GET_CODE (PATTERN (old_jump)) == SET)
		    set_src = SET_SRC (PATTERN (old_jump));
		  else if (GET_CODE (PATTERN (old_jump)) == PARALLEL)
		    {
		      set_src = XVECEXP (PATTERN (old_jump), 0,0);
		      if (GET_CODE (set_src) == SET)
			set_src = SET_SRC (set_src);
		      else
			set_src = NULL_RTX;
		    }
		}


	      if (set_src && (GET_CODE (set_src) == IF_THEN_ELSE))

 		{

		  if (GET_CODE (XEXP (set_src, 1)) == PC)
		    old_label = XEXP (set_src, 2);
		  else if (GET_CODE (XEXP (set_src, 2)) == PC)
		    old_label = XEXP (set_src, 1);

 		  /* mark jump do-not-shorten */
		  
 		  new_note = emit_note_before (NOTE_INSN_DONT_SHORTEN_BRANCH,
 					       old_jump);
 		  NOTE_BASIC_BLOCK (new_note) = cur_bb;

		  /* check to see if new bb for jumping to that dest has
		     already been created; if so, use it; if not, create
		     a new one */
		  
		  new_bb = find_jump_block (old_label, block_list, 
					    block_list_idx);

		  if (new_bb)
		    {
		      new_label = NULL;
		      for (cur_insn = new_bb->head; cur_insn != new_bb->end;
			   cur_insn = NEXT_INSN (cur_insn))
			if (GET_CODE (cur_insn) == CODE_LABEL)
			  new_label = cur_insn;
		    }
		  else
		    {
		      /* create new basic block to be dest for conditional jump */

		      if (scan_ahead_for_unlikely_executed_note (cur_bb->head))
			{
			  new_bb = create_basic_block (NULL, NULL, last_bb);
			  alloc_aux_for_block (new_bb, 
					       sizeof (struct reorder_block_def));
			  RBI (new_bb)->next = RBI (last_bb)->next;
			  RBI (last_bb)->next = new_bb;
			  last_bb = new_bb;
			}
		      else
			{
			  new_bb = create_basic_block (NULL, NULL, last_hot_bb);
			  alloc_aux_for_block ( new_bb,
						sizeof (struct reorder_block_def));
			  RBI (new_bb)->next = RBI (last_hot_bb)->next;
			  RBI (last_hot_bb)->next = new_bb;
			  last_hot_bb = new_bb;
			}
		      
		      /* put appropriate instructions in new bb */
		      
		      new_label = gen_label_rtx ();
		      emit_label_before (new_label, new_bb->head);
		      new_bb->head = new_label;
		      if (new_bb->index < max_array_size)
			bb_has_label[new_bb->index] = 1;
		      
		      if (GET_CODE (old_label) == LABEL_REF)
			{
			  old_label = JUMP_LABEL (old_jump);
			  new_jump = emit_jump_insn_after (gen_jump (old_label), 
							   new_bb->end);
			}
		      else if (GET_CODE (old_label) == RETURN)
			new_jump = emit_jump_insn_after (gen_return (), new_bb->end);
		      else
			abort ();
		      
		      barrier = emit_barrier_after (new_jump);
		      JUMP_LABEL (new_jump) = old_label;
		      RBI (new_bb)->footer = unlink_insn_chain (barrier, barrier);
		      
		      new_note = emit_note_before (NOTE_INSN_DONT_SHORTEN_BRANCH,
						   new_jump);
		      NOTE_BASIC_BLOCK (new_note) = new_bb;
		      
		      if (new_bb->index < max_array_size)
			bb_has_jump [new_bb->index] = 1;
		  
		      /* make sure new bb has same 'color' as source of conditional
			 branch */
		  
		      if (scan_ahead_for_unlikely_executed_note (cur_bb->head))
			{
			  new_note2 = emit_note_before (NOTE_INSN_UNLIKELY_EXECUTED_CODE,
							new_note);
			  NOTE_BASIC_BLOCK (new_note2) = new_bb;
			  /* bb_colors[new_bb->index] = 1; */
			}
		  
		      if (block_list_idx < 4*last_basic_block)
			{
			  block_list[block_list_idx] = new_bb;
			  block_list_idx++;
			}

		    }

 		  /* make old jump branch to new bb */
		  
 		  redirect_jump (old_jump, new_label, 0);
		  
 		  /* Remove crossing_edge as predecessor of 'dest' */

      		  dest = crossing_edge->dest;

		  redirect_edge_succ (crossing_edge, new_bb);

 		  /* make a new edge from new_bb to old dest; new
 		     edge will be a successor for new_bb and a predecessor for
 		     'dest' */
		  
 		  new_edge = make_edge (new_bb, dest, 0);
		  
 		} /* if it's a conditional branch */
 	    } /* if bb contains a branch */
 	} /* if bb has a crossing edge */
    } /* for each bb */
  
  free (bb_colors);
  free (bb_has_label);
  free (bb_has_jump);
  free (crossing_edges);
  free (block_list);

}

/* APPLE LOCAL end - rarely executed bb optimization */

/* Reorder basic blocks.  The main entry point to this file.  */

void
reorder_basic_blocks (partition_flag)
     int partition_flag;
{
  if (n_basic_blocks <= 1)
    return;

  if ((* targetm.cannot_modify_jumps_p) ())
    return;

  cfg_layout_initialize ();

  /* APPLE LOCAL begin - rarely executed bb optimization */

  if ((partition_flag)
      && (!flag_exceptions) 
      && (strcmp (lang_hooks.name, "GNU C++") != 0))
    find_rarely_executed_basic_blocks();
  /* APPLE LOCAL end - rarely executed bb optimization */

  make_reorder_chain ();

  if (rtl_dump_file)
    dump_flow_info (rtl_dump_file);

  /* APPLE LOCAL begin - rarely executed bb optimization */
  if ((partition_flag)
      && (!flag_exceptions) 
      && (strcmp (lang_hooks.name, "GNU C++") != 0))
    fix_branches_for_unexecuted_code ();
  /* APPLE LOCAL end - rarely executed bb optimization */

  cfg_layout_finalize ();
}
