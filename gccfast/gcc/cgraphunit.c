/* Callgraph handling code.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Contributed by Jan Hubicka

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

#include <float.h>
#include "config.h"
#include "system.h"
/* APPLE LOCAL begin callgraph inlining */
/* #include "coretypes.h" */
/* #include "tm.h" */
/* APPLE LOCAL end callgraph inlining */
#include "tree.h"
#include "c-common.h"
#include "tree-inline.h"
#include "langhooks.h"
#include "hashtab.h"
#include "toplev.h"
#include "flags.h"
#include "ggc.h"
#include "debug.h"
#include "target.h"
#include "varray.h"
#include "cgraph.h"
#include "diagnostic.h"
#include "params.h"
#include "feedback.h"

/* Plagaraized from tree-inline.c .  */
#define INSNS_PER_STMT (10)

static void cgraph_expand_functions PARAMS ((void));
static tree cgraph_find_calls_r PARAMS ((tree *, int *, void *));
static struct cgraph_edge *cgraph_inlining_callee_list PARAMS ((struct cgraph_node *, tree *));
static void cgraph_mark_functions_to_output PARAMS ((void));
static void cgraph_directed_inlining_r PARAMS ((struct cgraph_edge *, varray_type *));
static void cgraph_expand_function PARAMS ((struct cgraph_node *));
static tree record_call_1 PARAMS ((tree *, int *, void *));
static void cgraph_mark_local_functions PARAMS ((void));
static void cgraph_mark_functions_to_inline_once PARAMS ((void));
static bool cgraph_inlinable_function_p PARAMS ((tree));
static inline double cgraph_call_body_desirability PARAMS ((struct cgraph_node *));
static inline struct cgraph_edge *cgraph_most_desirable_edge PARAMS ((struct cgraph_edge *));
static void cgraph_make_inlining_choices PARAMS ((void));
static void cgraph_clone_parms PARAMS ((tree, tree, splay_tree));
static void cgraph_display_inlining_step_r PARAMS ((struct cgraph_edge *));
static void cgraph_display_inlining_step PARAMS ((struct cgraph_edge *, const char *));
static tree cgraph_directed_inlining PARAMS ((struct cgraph_node *));
static void cgraph_optimize_function PARAMS ((struct cgraph_node *));
static inline void revise_desirabilities_r PARAMS ((struct cgraph_edge *edge));
static inline HOST_WIDEST_INT scaled_execution_count PARAMS ((struct cgraph_edge *edge));
static inline struct cgraph_edge *cgraph_most_desirable_edge PARAMS ((struct cgraph_edge *));

/* True if we made some inlining decisions; left FALSE if generating profile-generation code.  */
static bool choices_made = FALSE;

/* Analyze function once it is parsed.  Set up the local information
   available - create cgraph edges for function calles via BODY.  */

void
cgraph_finalize_function (decl, body)
     tree decl;
     tree body ATTRIBUTE_UNUSED;
{
  struct cgraph_node *node = cgraph_node (decl);

  node->decl = decl;

  node->local.can_inline_once = tree_inlinable_function_p (decl, /* nolimit: */1);
  if (flag_inline_trees)
    node->local.inline_many = tree_inlinable_function_p (decl, /* nolimit: */0);
  else
    node->local.inline_many = 0;

  (*debug_hooks->deferred_inline_function) (decl);
}

static GTY(()) struct cgraph_node *queue = NULL;

/* Notify finalize_compilation_unit that given node is reachable
   or needed.  */
void
cgraph_mark_needed_node (node, needed)
     struct cgraph_node *node;
     int needed;
{
  if (needed)
    {
      if (DECL_SAVED_TREE (node->decl))
        announce_function (node->decl);
      node->needed = 1;
    }
  if (!node->reachable)
    {
      node->reachable = 1;
      if (DECL_SAVED_TREE (node->decl))
	{
	  node->aux = queue;
	  queue = node;
        }
    }
}

/* Walk tree and record all calls.  Called via walk_tree_without_duplicates.  */
/* Someday, when -funit-at-a-time is so wonderful that it becomes the
   default ;-), this should be folded into the tree-creation/tree-copying
   routines.  Doing this work with a separate tree walk is slow when the
   amount of inlining is large.  */   
static tree
record_call_1 (tp, walk_subtrees, data)
     tree *tp;
     int *walk_subtrees ATTRIBUTE_UNUSED;
     void *data;
{
  /* Record dereferences to the functions.  This makes the functions
     reachable unconditionally.  */
  if (TREE_CODE (*tp) == ADDR_EXPR)
    {
      tree decl = TREE_OPERAND (*tp, 0);
      if (TREE_CODE (decl) == FUNCTION_DECL)
        cgraph_mark_needed_node (cgraph_node (decl), 1);
    }
  else if (TREE_CODE (*tp) == CALL_EXPR)
    {
      tree decl = TREE_OPERAND (*tp, 0);
      if (TREE_CODE (decl) == ADDR_EXPR)
	decl = TREE_OPERAND (decl, 0);
      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  if (DECL_BUILT_IN (decl))
	    return NULL;
	  /* FIXME: Need profiling info, or at least, "loop-nest heuristic" instead of "1" here.  */
	  cgraph_record_call (data, decl, tp, (HOST_WIDEST_INT)1);
	  /* walk_tree (&TREE_OPERAND (*tp, 1), record_call_1, data, %%%%%%%); */
	  /* *walk_subtrees = 0; */
	}
    }
  return NULL;
}

/* Create cgraph edges for function calls via BODY.  */
void
cgraph_create_edges (decl, body)
     tree decl;
     tree body;
{
  walk_tree_without_duplicates (&body, record_call_1, decl);
}

/* If we stumbled upon a CALL_EXPR, remember where it lives in case we
   decide to inline it.  Invoked only by
   walk_tree_without_duplicates.  */
static tree
cgraph_find_calls_r (tp, walk_subtrees, data)
     tree *tp;
     int *walk_subtrees ATTRIBUTE_UNUSED;
     void *data;
{
  if (TYPE_P (*tp))
    /* Because types were not copied in copy_body, CALL_EXPRs beneath
       them should not be expanded.  This can happen if the type is a
       dynamic array type, for example.  */
    *walk_subtrees = 0;
  else if (TREE_CODE (*tp) == CALL_EXPR)
    {
      varray_type *callee_varray = (varray_type *)data;
      tree decl = TREE_OPERAND (*tp, 0);
      if (TREE_CODE (decl) == ADDR_EXPR)
	decl = TREE_OPERAND (decl, 0);
      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  /* FIXME: ask if this function is legal/moral to inline.
	     calls alloca? setjmp?
	     look at langhooks.c:lhd_tree_inlining_add_pending_fn_decls()  */
	  /* see cp/optimize.c:calls_setjmp_r() */
	  if (DECL_BUILT_IN (decl))
	    return NULL;
	  /* Remember the location of this CALL_EXPR.  */
	  VARRAY_PUSH_GENERIC_PTR (*callee_varray, tp);
	}
    }
  return NULL;
}

/* Return a varray of addresses of pointers to CALL_EXPRs.  Ideally
 this functionality could be folded into copy_body, avoiding the need
 for this tree walk.  */

varray_type
cgraph_find_calls (body)
     tree *body;
{
  varray_type callee_array;
  VARRAY_GENERIC_PTR_INIT (callee_array, 10, "list of ptrs to CALL_EXPRs");
  walk_tree_without_duplicates (body, (walk_tree_fn)cgraph_find_calls_r, (void *)&callee_array);
  return callee_array;
}

/* Create a list of edges, one per callee, using the same algorithm as
   cgraph_find_calls().  It's crucial that we discover the same calls
   in the same order every time we walk over a tree or its clone.  */
static struct cgraph_edge *
cgraph_inlining_callee_list (caller, tp)
     struct cgraph_node *caller ATTRIBUTE_UNUSED;
     tree *tp;
{
  struct cgraph_edge *first, **step;
  tree *callp, decl;
  varray_type callee_varray = cgraph_find_calls (tp);
  unsigned int i, limit = VARRAY_ACTIVE_SIZE (callee_varray);

  first = (struct cgraph_edge *)NULL;
  step = &first;
  for (i=0; i<limit; i++)
    {
      callp = VARRAY_GENERIC_PTR (callee_varray, i);
      decl = TREE_OPERAND (*callp, 0);
      if (TREE_CODE (decl) == ADDR_EXPR)
	decl = TREE_OPERAND (decl, 0);
      if (TREE_CODE (decl) == FUNCTION_DECL)
	{
	  *step = create_edge ((struct cgraph_node *)NULL, (struct cgraph_node *)NULL, callp);
	  (*step)->caller = caller;
	  (*step)->callee = cgraph_node (decl);
	  step = &((*step)->next_callee);
	}
      else
	abort();
    }
  *step = (struct cgraph_edge *)NULL;
  return first;
}

/* Analyze the whole compilation unit once it is parsed completely.  */

void
cgraph_finalize_compilation_unit ()
{
  struct cgraph_node *node;
  struct cgraph_edge *edge;

  /* Collect entry points to the unit.  */

  if (!quiet_flag)
    fprintf (stderr, "\n\nUnit entry points:");

  for (node = cgraph_nodes; node; node = node->next)
    {
      tree decl = node->decl;

      if (!DECL_SAVED_TREE (decl))
	continue;
      if ((TREE_PUBLIC (decl) && !DECL_COMDAT (decl) && !DECL_EXTERNAL (decl))
	  || (DECL_ASSEMBLER_NAME_SET_P (decl)
	      && TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (decl))))
	{
	  if (!quiet_flag)
	    fprintf (stderr, " 0x%p", (void *)node);
          cgraph_mark_needed_node (node, 1);
	}
    }

  /*  Propagate reachability flag and lower representation of all reachable
      functions.  In the future, lowering will introduce new functions and
      new entry points on the way (by template instantiation and virtual
      method table generation for instance).  */
  while (queue)
    {
      tree decl = queue->decl;

      node = queue;
      queue = queue->aux;
      if (node->lowered || !node->reachable || !DECL_SAVED_TREE (decl))
	abort ();

      /* At the moment frontend automatically emits all nested functions.  */
      if (node->nested)
	{
	  struct cgraph_node *node2;

	  for (node2 = node->nested; node2; node2 = node2->next_nested)
	    if (!node2->reachable)
	      cgraph_mark_needed_node (node2, 0);
	}

      if (lang_hooks.callgraph.lower_function)
	(*lang_hooks.callgraph.lower_function) (decl);
      /* First kill forward declaration so reverse inlining works properly.  */
      cgraph_create_edges (decl, DECL_SAVED_TREE (decl));

      for (edge = node->callees; edge; edge = edge->next_callee)
	{
	  if (!edge->callee->reachable)
            cgraph_mark_needed_node (edge->callee, 0);
	}
      node->lowered = true;
    }
  if (!quiet_flag)
    fprintf (stderr, "\n\nReclaiming functions:");

  for (node = cgraph_nodes; node; node = node->next)
    {
      tree decl = node->decl;

      if (!node->reachable && DECL_SAVED_TREE (decl))
	{
	  cgraph_remove_node (node);
	  announce_function (decl);
	}
    }
  ggc_collect ();
}

/* Figure out what functions we want to assemble.  */

static void
cgraph_mark_functions_to_output ()
{
  struct cgraph_node *node;

  /* Figure out functions we want to assemble.  */
  for (node = cgraph_nodes; node; node = node->next)
    {
      tree decl = node->decl;

      if (DECL_SAVED_TREE (decl)
	  && (node->needed
	      || (!node->local.inline_many && !node->global.inline_once
		  && node->reachable)
	      || TREE_SYMBOL_REFERENCED (DECL_ASSEMBLER_NAME (decl)))
	  && !TREE_ASM_WRITTEN (decl) && !node->origin
	  && !DECL_EXTERNAL (decl))
	node->output = 1;
    }
}

/* Tell user what we're doing.  For debugging.  */
static void
cgraph_display_inlining_step_r (edge)
     struct cgraph_edge *edge;
{
  if (edge->inliner.uplink)
    {
      cgraph_display_inlining_step_r (edge->inliner.uplink);
      fprintf (stderr, "->");
    }
  fprintf (stderr, "%s",
	   IDENTIFIER_POINTER (DECL_NAME (edge->caller->decl)));
}

/* Tell user what we're doing.  For debugging.  */
static void
cgraph_display_inlining_step (edge, verb)
     struct cgraph_edge *edge;
     const char *verb;
{
  fprintf (stderr, "\n[0x%p ", (void *)edge);
  cgraph_display_inlining_step_r (edge);
  /* [addr fnbody->callee1->... 'verb' calleeN
     edge-exec-count calleeN-exec-count edge-desirability] */
  fprintf (stderr, " %s %s edge %llu callee %llu - %llur %e]", verb,
	   IDENTIFIER_POINTER (DECL_NAME (edge->callee->decl)),
	   edge->inliner.execution_count,
	   edge->callee->inliner.execution_count,
	   edge->callee->inliner.removed_execution_count,
	   edge->inliner.desirability);
}

/* Display all the edges, inlined or not.  */
void
cgraph_display_function_callgraph (edge, verb)
     struct cgraph_edge *edge;
     const char *verb;
{
  struct cgraph_edge *step;

  for (step=edge; step; step=step->next_callee)
    {
      if (step->inliner.inline_this)
	{
	  cgraph_display_inlining_step (step, verb);
	}
      else
	{
	  cgraph_display_inlining_step (step, "calls");
	  cgraph_display_function_callgraph (step->inliner.callees, verb);
	}
    }
}

/* Walk the graph of callees in node, and inline the chosen calls.  */
static void
cgraph_directed_inlining_r (edge, inlinee_list)
     struct cgraph_edge *edge;
     varray_type *inlinee_list;
{
  struct cgraph_edge *prev, *step;
  varray_type callee_array = (varray_type)NULL;
  tree *p_2_body, *tp;
  int i;
  double scale;
  HOST_WIDEST_INT scaled_edge_exec_count;

  /* If this edge is marked to be inlined, do so.  */
  if (edge->inliner.inline_this)
    {
      scaled_edge_exec_count = edge->inliner.execution_count;
      if (edge->callee->inliner.execution_count == 0)
	scale = 0.0;
      else
        scale = (double)edge->inliner.execution_count 
	  / (double)edge->callee->inliner.execution_count;
      /* It's necessary for scale * edge->callee->inliner.execution_count
	 to be == edge->inliner.execution.count.  If this is not the case
	 tweak scale until it is.  */
      {
	HOST_WIDEST_INT tweak = 1;
        while ((HOST_WIDEST_INT)(scale * edge->callee->inliner.execution_count)
	       < edge->inliner.execution_count)
	  scale = (double)edge->inliner.execution_count
	    / ((double)edge->callee->inliner.execution_count - tweak++);
        tweak = 1;
        while ((HOST_WIDEST_INT)(scale * edge->callee->inliner.execution_count)
	       > edge->inliner.execution_count)
	  scale = (double)edge->inliner.execution_count
	    / ((double)edge->callee->inliner.execution_count + tweak++);
      }
      if (!quiet_flag)
	{
	  /* cgraph_display_function_callgraph (edge, "inlines"); */
	  fprintf (stderr, "[scale: %lld/%lld=%f", scaled_edge_exec_count,
		   edge->callee->inliner.execution_count, scale);
	  if ((scale > 1.0) || (scale <= 0.0))
	    fprintf (stderr, " ***error***");
	  fprintf (stderr, "]");
	}
      tp = edge->inliner.call_expr;
      p_2_body = inline_this_call(tp, edge->callee->decl, scale);
      /* We just inlined the only call to this callee; discard the body.  */
      if (edge->callee->global.inline_once)
	DECL_SAVED_TREE (edge->callee->decl) = NULL;
      /* FIXME: loop and check if any new callees will be inlined; if not, we're done.  */
      /* We just duplicated a function body; find the new CALL_EXPRs therein.  */
      if (edge->inliner.callees)
	/* Freed by the garbage collector.  */
	callee_array = cgraph_find_calls (p_2_body);
      /* If the outermost function wants to know what got inlined into
	 its body, add the current inlinee to the list.  */
      if (inlinee_list)
	VARRAY_PUSH_GENERIC_PTR (*inlinee_list, edge->callee->decl);
      /* If this callee contained any calls, examine their edges and
	 inline any marked for inlining.  (Leaf functions will skip
	 this loop.)  */
      for (prev=(struct cgraph_edge *)NULL, i=0, step=edge->inliner.callees;
	   step;
	   i++, step=step->next_callee)
	{
	  if (/* disabled */ 0 && flag_use_feedback)
	    set_times_call_executed (*(tree *)VARRAY_GENERIC_PTR (callee_array, i), step->inliner.execution_count);
	  step->inliner.prev = prev;
	  prev = step;
	}
      for (i--, step=prev; step; i--, step=step->inliner.prev)
	{
	  if (step->inliner.inline_this)
	    {
	      /* Locate the pointer-to-the-CALL_EXPR.  */
	      step->inliner.call_expr = VARRAY_GENERIC_PTR (callee_array, i);
	      cgraph_directed_inlining_r (step, inlinee_list);
	    }
	}
    }
}

/* Clone the DECL_ARGUMENT list of fn into clone, creating a splay
   tree to map fn-parameters onto clone-parameters.  Also duplicate
   and en-splay the DECL_RESULT.  */
static void
cgraph_clone_parms (clone, fn, arg_map)
     tree clone, fn;
     splay_tree arg_map;
{
  tree *clone_parm = &DECL_ARGUMENTS (clone);
  tree fn_parm = DECL_ARGUMENTS (fn);
  for ( ; fn_parm; clone_parm = &TREE_CHAIN (*clone_parm), fn_parm = TREE_CHAIN (fn_parm))
    {
      *clone_parm = copy_node (fn_parm);
      DECL_CONTEXT (*clone_parm) = clone;
      (void) splay_tree_insert (arg_map,
				(splay_tree_key) fn_parm,
				(splay_tree_value) *clone_parm);
    }
  *clone_parm = NULL_TREE;
  if (DECL_RESULT (clone))
    {
      DECL_RESULT (clone) = copy_node (DECL_RESULT (clone));
      DECL_CONTEXT (DECL_RESULT (clone)) = clone;
      (void) splay_tree_insert (arg_map,
				(splay_tree_key) DECL_RESULT (fn),
				(splay_tree_value) DECL_RESULT (clone));
    }
}

/* For debugging.  */

splay_tree_foreach_fn debug_splay_tree_node PARAMS ((struct splay_tree_node_s *, void *));
void debug_splay_tree PARAMS ((splay_tree));

/* For debugging.  */

splay_tree_foreach_fn
debug_splay_tree_node (sp, v)
     struct splay_tree_node_s *sp;
     void *v ATTRIBUTE_UNUSED;
{
  fprintf (stderr, "key: "); debug_tree ((tree)sp->key);
  fprintf (stderr, "val: "); debug_tree ((tree)sp->value);
  return 0;
}

/* For debugging.  */

void
debug_splay_tree (sp)
     splay_tree sp;
{
  splay_tree_foreach (sp, (splay_tree_foreach_fn)debug_splay_tree_node, (void *)NULL);
}

/* Walk the graph of callees in node, and inline the chosen calls.  If
   any inlining is done, returns a duplicate of the original tree.  If
   no inlining is done, returns the original tree.  */
static tree
cgraph_directed_inlining (node)
     struct cgraph_node *node;
{
  struct cgraph_edge *prev, *step;
  varray_type inlinee_list = (varray_type)NULL;
  varray_type call_expr_array;
  tree clone = node->decl;
  int i;
  double scale;

  if (DECL_LANG_SPECIFIC (node->decl))
    VARRAY_GENERIC_PTR_INIT (inlinee_list, 10, "list of inlined functions");

  if (!quiet_flag && node->inliner.top_edge && node->inliner.top_edge->inliner.callees)
    cgraph_display_function_callgraph (node->inliner.top_edge->inliner.callees, "inlines");

  /* Did we decided to inline anything into this function?  */
  for (step=node->inliner.top_edge->inliner.callees; step; step=step->next_callee)
    if (step->inliner.inline_this)
      break;

  /* Yes, we want to inline something.  */
  if (step)
    {
      /* Create empty splay tree; remap_decls() will duplicate PARM_DECLs for us.  */
      splay_tree arg_map = splay_tree_new (splay_tree_compare_pointers, NULL, NULL);
      /* Scale the execution counts in this body.  */
      scale = (((double)node->inliner.execution_count) - ((double)node->inliner.removed_execution_count))
	/ ((double)node->inliner.execution_count);
      /* Always duplicate the function body to be modified, so that
	 the original is available for subsequent inlining elsewhere.
	 Copy the topmost FUNCTION_DECL and parameters right here,
	 since copy_tree will decline to do so.  */
      clone = copy_node (node->decl);
      /* Propagate the (unscaled) execution count for the FUNCTION_DECL node.  */
      current_function_decl = node->decl;
      clone_rtx_feedback_counter (node->decl, clone);
      /* set_times_arc_executed (clone, 0, (HOST_WIDEST_INT)(scale * times_arc_executed (node->decl, 0))); */
      /* set_times_arc_executed (clone, 1, (HOST_WIDEST_INT)(scale * times_arc_executed (node->decl, 1))); */
      /* copy_tree will look here to get the DECL_CONTEXT of variables.  */
      current_function_decl = clone;
      cgraph_clone_parms (clone, node->decl, arg_map);
      /* Conservatively duplicate the top node, as clone_body() will
	 clobber its TREE_CHAIN link.  */
      DECL_SAVED_TREE (clone) = copy_node (DECL_SAVED_TREE (node->decl));
      clone_body_scaled (clone, node->decl, arg_map, /*** disabled *** scale */1.0);
      /* The body returned by clone_body() is chained onto the
	 saved_tree.  */
      DECL_SAVED_TREE (clone) = TREE_CHAIN (DECL_SAVED_TREE (clone));
      DECL_INITIAL (clone) = clone_tree_into_context (clone, DECL_INITIAL (clone), arg_map);
      BLOCK_SUPERCONTEXT (DECL_INITIAL (clone)) = clone;

      /* We currently locate the CALL_EXPRs by walking the tree
	 ourselves.  In the future, it would be more efficient to
	 collect these addresses while copying ("cloning") trees, but
	 it may be difficult to discover and remember them in the same
	 order during every walk.  */
      call_expr_array = cgraph_find_calls (&DECL_SAVED_TREE (clone));
      /* Find the last one and set the backlinks.  */
      for (prev=(struct cgraph_edge *)NULL, i=0, step=node->inliner.top_edge->inliner.callees;
	   step;
	   i++, step=step->next_callee)
	{
	  step->inliner.prev = prev;
	  prev = step;
	}
      /* From last(deepest) to first(highest), inline the CALL_EXPRs.  */
      for (i--, step=prev; step; i--, step=step->inliner.prev)
	{
	  if (step->inliner.inline_this)
	    {
	      step->inliner.call_expr = VARRAY_GENERIC_PTR (call_expr_array, i);
	      /* Record our scaled (estimated) execution counts for these new CALL_EXPRs.  */
	      if (/* disabled */ 0 && flag_use_feedback)
		set_times_call_executed (*step->inliner.call_expr, step->inliner.execution_count);
	      /* If the topmost node of the clone is a
		 CALL_EXPR, and we decide to inline it, then the
		 contents of our local variable "clone" will be
		 altered here.  Scary, but it should work.  */
	      cgraph_directed_inlining_r (step, &inlinee_list);
	    }
	}
      /* Make a vector of functions inlined into this one.  */
      if (DECL_LANG_SPECIFIC (clone))
	{
	  tree ifn = make_tree_vec (VARRAY_ACTIVE_SIZE (inlinee_list));

	  if (VARRAY_ACTIVE_SIZE (inlinee_list))
	    memcpy (&TREE_VEC_ELT (ifn, 0), &VARRAY_TREE (inlinee_list, 0),
		    VARRAY_ACTIVE_SIZE (inlinee_list) * sizeof (tree));
	  DECL_INLINED_FNS (clone) = ifn;
	}
    }
  else /* We didn't choose to inline anything into this function.  */
    {
      if (DECL_LANG_SPECIFIC (clone))
	DECL_INLINED_FNS (clone) = make_tree_vec (0);
    }
  current_function_decl = NULL_TREE;
  return clone;
}

/* Optimize the function before expansion.  */
static void
cgraph_optimize_function (node)
     struct cgraph_node *node;
{
  if (choices_made)
    node->inliner.fully_inlined_decl = cgraph_directed_inlining (node);
  else
    node->inliner.fully_inlined_decl = node->decl;

  if (node->nested)
    {
      for (node = node->nested; node; node = node->next_nested)
	{
	  /* FIXME: Correct, or use enclosing function? */
	  cgraph_optimize_function (node);
	  /* FIXME: inlining on nested functions.  */
	}
    }
}

/* Expand function specified by NODE.  */
static void
cgraph_expand_function (node)
     struct cgraph_node *node;
{
  tree decl = node->decl;

  announce_function (decl);

  cgraph_optimize_function (node);

  /* Avoid RTL inlining from taking place.  */
  (*lang_hooks.callgraph.expand_function) (node->inliner.fully_inlined_decl);

  /* When we decided to inline the function once, we never ever should need to
     output it separately.  */
  if (node->global.inline_once)
    abort ();
  /* FIXME: Need callgraph-directed-inlining smarts here.  */
  /* if (!node->local.inline_many
     || !node->callers)
     DECL_SAVED_TREE (decl) = NULL; */
  node->inliner.fully_inlined_decl = NULL_TREE;
  current_function_decl = NULL;
  TREE_ASM_WRITTEN (node->decl) = TRUE;
}


/* Expand all functions that must be output. 
  
   Attempt to topologically sort the nodes so function is output when
   all called functions are already assembled to allow data to be propagated
   accross the callgraph.  Use stack to get smaller distance between function
   and its callees (later we may use more sophisticated algorithm for
   function reordering, we will likely want to use subsections to make output
   functions to appear in top-down order, not bottom-up they are assembled).  */

static void
cgraph_expand_functions ()
{
  struct cgraph_node *node, *node2;
  struct cgraph_node **stack =
    xcalloc (sizeof (struct cgraph_node *), cgraph_n_nodes);
  struct cgraph_node **order =
    xcalloc (sizeof (struct cgraph_node *), cgraph_n_nodes);
  int stack_size = 0;
  int order_pos = 0;
  struct cgraph_edge *edge, last;
  int i;

  cgraph_mark_functions_to_output ();

  /*  We have to deal with cycles nicely, so use depth first traversal
      algorithm.  Ignore the fact that some functions won't need to be output
      and put them into order as well, so we get dependencies right trought inlined
      functions.  */
  for (node = cgraph_nodes; node; node = node->next)
    node->aux = NULL;
  for (node = cgraph_nodes; node; node = node->next)
    if (node->output && !node->aux)
      {
	node2 = node;
	if (!node->callers)
	  node->aux = &last;
	else
	  node->aux = node->callers;
	while (node2)
	  {
	    while (node2->aux != &last)
	      {
		edge = node2->aux;
		if (edge->next_caller)
		  node2->aux = edge->next_caller;
		else
		  node2->aux = &last;
		if (!edge->caller->aux)
		  {
		    if (!edge->caller->callers)
		      edge->caller->aux = &last;
		    else
		      edge->caller->aux = edge->caller->callers;
		    stack[stack_size++] = node2;
		    node2 = edge->caller;
		    break;
		  }
	      }
	    if (node2->aux == &last)
	      {
		order[order_pos++] = node2;
		if (stack_size)
		  node2 = stack[--stack_size];
		else
		  node2 = NULL;
	      }
	  }
      }
  for (i = order_pos - 1; i >= 0; i--)
    {
      node = order[i];
      if (node->output)
	{
	  if (!node->reachable)
	    abort ();
	  node->output = 0;
	  cgraph_expand_function (node);
	}
    }
  free (stack);
  free (order);
}

/* Mark all local functions.
   We can not use node->needed directly as it is modified during
   execution of cgraph_optimize.  */

static inline void
cgraph_mark_local_functions ()
{
  struct cgraph_node *node;

  if (!quiet_flag)
    fprintf (stderr, "\n\nMarking local functions:");

  /* Figure out functions we want to assemble.  */
  for (node = cgraph_nodes; node; node = node->next)
    {
      node->local.local = (!node->needed
		           && DECL_SAVED_TREE (node->decl)
		           && !TREE_PUBLIC (node->decl));
      if (node->local.local)
	announce_function (node->decl);
    }
}

/*  Decide what function should be inlined because they are invoked once
    (so inlining won't result in duplication of the code).  */

static inline void
cgraph_mark_functions_to_inline_once ()
{
  struct cgraph_node *node, *node1;

  if (!quiet_flag)
    fprintf (stderr, "\n\nMarking functions to inline once:");

  /* Now look for function called only once and mark them to inline.  From this
     point number of calls to given function won't grow.  */
  for (node = cgraph_nodes; node; node = node->next)
    {
      if (node->callers && !node->callers->next_caller && !node->needed
	  && node->local.can_inline_once)
	{
	  bool ok = true;

	  /* Verify that we won't duplicate the caller.  */
	  for (node1 = node->callers->caller;
	       node1->local.inline_many
	       && node1->callers
	       && ok;
	       node1 = node1->callers->caller)
	    if (node1->callers->next_caller || node1->needed)
	      ok = false;
	  if (ok)
	    {
	      node->global.inline_once = true;
	      announce_function (node->decl);
	    }
	}
    }
}

/* TRUE if the argument FUNCTION_DECL is inlinable.  Reasons to be
   un-inlinable include: no DECL_SAVED_TREE, variable arguments, calls
   setjmp or alloca.  Not aware of call-point issues (e.g. recursion:
   me() { me(); }).  Note the result of this function has nothing to
   do with the "desirability" of a function for inlining.  */
static bool
cgraph_inlinable_function_p (fn)
     tree fn;
{
  tree fntype;
  if (!DECL_SAVED_TREE (fn))
    return FALSE;
  fntype = TREE_TYPE (fn);
  /* If this function accepts variable arguments, we can't inline it.  */
  if ((TYPE_ARG_TYPES (fntype) != 0)
      && (TREE_VALUE (tree_last (TYPE_ARG_TYPES (fntype)))
	  != void_type_node))
    return FALSE;
  if (find_builtin_longjmp_call (DECL_SAVED_TREE (fn)))
    return FALSE;
  /* Refuse to inline alloca call unless user explicitly forced so as this may
     change program's memory overhead drastically when the function using alloca
     is called in loop.  In GCC present in SPEC2000 inlining into schedule_block
     cause it to require 2GB of ram instead of 256MB.  */
  if (lookup_attribute ("always_inline", DECL_ATTRIBUTES (fn)) == NULL
	   && find_alloca_call (DECL_SAVED_TREE (fn)))
    return FALSE;
  if (lookup_attribute ("noinline", DECL_ATTRIBUTES (fn)))
    return FALSE;
  if ((*lang_hooks.tree_inlining.cannot_inline_tree_fn) (&fn))
    return FALSE;
  return TRUE;
}

/* Heuristically compute the "desirability" of a function body for
   inlining.  This is a metric for everything known about a callee
   independent of a call site.  The real desirability values are
   computed using this metric, and some information gleaned from the
   context of each call.  This is a pure, stateless function, and it
   may be invoked from the debugger.  */
static inline double
cgraph_call_body_desirability (struct cgraph_node *node)
{
  double desirability=0.0;
  unsigned long stmts;

  if (node && cgraph_inlinable_function_p (node->decl))
    {
      desirability = 1.0;

      /* Use the number of statements in the source as our size estimate.  */
      stmts = DECL_NUM_STMTS (node->decl);
      if (stmts == 0)
	stmts = 1;

      /* If this is the only call to this node, we must inline it.  */
      if (node->global.inline_once)
	/* Caution: DBL_MAX may not be portable across all GCC hosts.  */
	desirability = DBL_MAX;
/* haifa begin */
      else if (flag_callgraph_inline_small
	       && (stmts <= flag_callgraph_inline_small))
	desirability = DBL_MAX;
/* haifa end */
      /* If node represents a leaf function, and it's inlinable.  */
      else if (/* !node->callees && */ node->local.inline_many)
	desirability = 100.0 / stmts;
    }
  return desirability;
}

/* Heuristically compute the "desirability" of inlining the given call.
   This is a pure, stateless function, and it may be invoked from the debugger.
   Caller must set edge->inliner.execution_count if flag_use_feedback is set.  */
double
cgraph_call_desirability (struct cgraph_edge *edge)
{
  double desirability = 0.0, scale;
  struct cgraph_node *callee, *caller, *top_node;
  struct cgraph_edge *up, *top_edge;

  if (edge->callee && edge->callee->inliner.body_desirability > 0.0)
    {
      callee = edge->callee;
      caller = edge->caller;
      desirability = callee->inliner.body_desirability;

      /* Want to look at 1) # of const parameters,
	 2) are we in a loop, 3) is callee a leaf, 4) size of callee,
	 5) is this the only call to callee, 6) is this a recursive call,
	 7) are we under an if-then-else.  */

      /* Profile-directed inlining.  */
      if (flag_use_feedback)
	{
	  /* If b() inlined c() and d(), and then a() inlined half of
	     the invocations of b(), account for these "removed"
	     executions.  We don't yet un-do previous inlining
	     decisions, but we might, someday.  'scale' will always be
	     between 0.0 and 1.0 .  */
	  scale = (caller->inliner.execution_count - caller->inliner.removed_execution_count) / caller->inliner.execution_count;
	  desirability *= (edge->inliner.execution_count * scale);

/* haifa begin */
	  if (flag_callgraph_inlining_callee_ratio)
	    {
	      if ((100 * edge->inliner.execution_count) <
		  (flag_callgraph_inlining_callee_ratio * edge->callee->inliner.execution_count))
		desirability = 0.0;
	    }
	  if (flag_callgraph_inlining_caller_ratio)
	    {
	      if ((100 * edge->inliner.execution_count) <
		  (flag_callgraph_inlining_caller_ratio * edge->caller->inliner.execution_count))
		desirability = 0.0;
	    }
/* haifa end */
	}

      /* If this is the only call to this callee, or if the callee is
	 marked "always_inline", we must inline it.  */
      if (callee->global.inline_once
	  || lookup_attribute ("always_inline", DECL_ATTRIBUTES (callee->decl)))
	/* FIXME: Is DBL_MAX portable across all GCC hosts?  */
	desirability = DBL_MAX;
/* haifa begin */
      else if (flag_callgraph_inline_small
	       && (DECL_NUM_STMTS (callee->decl) <= flag_callgraph_inline_small))
	desirability = DBL_MAX;
/* haifa end */

      /* Walk the uplinks and compare all callers with current callee.
	 If any recursion is detected, mark this edge
	 un-desirable.  */
      for (top_edge=up=edge; up->inliner.uplink; top_edge=up, up=up->inliner.uplink)
	if (up->caller == callee)
	  desirability = 0.0;

      top_node = top_edge->caller;

      /* When "a(){ b(); } b(){ c(); d(); }", and b() is an
	 "inline_once" function, avoid inlining c() and d() into the
	 body of b(); let a() inline them all.  Also decline to inline
	 anything that results in a caller larger than 300 lines.  */
      if (top_node->global.inline_once
	  || (DECL_NUM_STMTS (top_node->decl) + top_node->inliner.additional_lines
	      > 300))
	desirability = 0.0;
    }
  if (desirability < 0.0)
    fprintf (stderr, "\n[*** error bad desirability: 0x%p %f]\n", (void *)edge, desirability);
  return desirability;
}

/* We just inlined this function somewhere, and bumped it's
   'removed_execution_count'.  Walk the graph of callees and revise
   the execution counts and desirabilities accordingly.  */
static inline void
revise_desirabilities_r (edge)
     struct cgraph_edge *edge;
{
  struct cgraph_edge *step;

  for (step=edge; step; step=step->next_callee)
    {
      step->inliner.desirability = cgraph_call_desirability (step);
      if (step->inliner.callees)
	revise_desirabilities_r (step->inliner.callees);
    }
}

/* Given an edge representing an inlined call, find the outermost
   caller and compute a "scaled execution count."  If b() inlines c()
   and d(), and a() has inlined half of the executions of b(), then
   the scaling factor for every call underneath b() is 0.5 .  */
static inline HOST_WIDEST_INT
scaled_execution_count (edge)
     struct cgraph_edge *edge;
{
  struct cgraph_edge *step;
  struct cgraph_node *callee, *top_caller;
  double combined_scale=1.0, scale;
  HOST_WIDEST_INT scaled_execution_count;
  if (!edge)
    return (HOST_WIDEST_INT)0;
  /* Walk back up the callgraph chain, scaling exec count by every edge.  */
  /* FIXME: this needs better documentation.  */
  if (edge->inliner.uplink)
    {
      /* Multiply all the scales together, *except* the bottommost edge.  */
      for (step=edge; step->inliner.uplink; step=step->inliner.uplink)
	{
	  callee = step->callee;
	  scale = ((double)step->inliner.execution_count)
	    / (double)callee->inliner.execution_count;
	  combined_scale *= scale;
	}
    }
  else
    step = edge;
  top_caller = step->caller;
  /* Include scale of topmost caller.  */
  scale = ((double)top_caller->inliner.execution_count - (double)top_caller->inliner.removed_execution_count)
    / (double)top_caller->inliner.execution_count;
  combined_scale *= scale;
  scaled_execution_count = (HOST_WIDEST_INT) (edge->inliner.execution_count * combined_scale);
  return scaled_execution_count;
}

/* Walk the edges "underneath" the given edge and return the "most
   desirable" edge (best inlining candidate) found.  Return NULL if no
   inlinable calls found.  */
static inline struct cgraph_edge *
cgraph_most_desirable_edge (edge)
     struct cgraph_edge *edge;
{
  struct cgraph_edge *child_hottest, *hottest=NULL, *step=edge;
  float child_hottest_desirability = 0.0, hottest_desirability = 0.0;
  for (step = edge ; step ; step = step->next_callee)
    {
      /* If this edge has callees, we've already decided to inline it,
	 so it's no longer an inlining candidate.  */
      if (step->inliner.inline_this && step->inliner.callees)
	{
	  /* Walk the edges underneath this to-be-inlined function.  */
	  child_hottest = cgraph_most_desirable_edge (step->inliner.callees);
	  child_hottest_desirability = (child_hottest) ? child_hottest->inliner.desirability : 0.0 ;
	  /* Best candidate so far found in this to-be-inlined
	     function body.  */
	  if (child_hottest_desirability > hottest_desirability)
	    {
	      hottest = child_hottest;
	      hottest_desirability = child_hottest_desirability;
	    }
	}
      else if ((!step->inliner.inline_this)
	       && (step->inliner.desirability > hottest_desirability))
	{
	  hottest = step;
	  hottest_desirability = hottest->inliner.desirability;
	}
    }
  return hottest;
}

/* Heuristically decide what to inline, and where.
   In the future, use profiling data to guide decisions.  */

static inline void
cgraph_make_inlining_choices ()
{
  struct cgraph_node *node;
  struct cgraph_edge *edge;
  unsigned long bloat_estimate=0, bloat_limit;
  unsigned long total_original_statements=0;
  varray_type caller_nodes_varray;
  bool more, only_call;
/* haifa begin */
  HOST_WIDEST_INT max_edge_exec_count = 0;
/* haifa end */

  if (!quiet_flag)
    fprintf (stderr, "\n\nMaking inlining decisions:");

  /* Allocate a VARRAY for temporary random access to the cgraph_nodes.
     This will be discarded after we've made our inlining choices.
     The "10" is arbitrary.  */
  VARRAY_GENERIC_PTR_INIT(caller_nodes_varray, 10, "node vector");

  /* Compute initial 'desirability' for every CALL_EXPR in the
     original (no inlining yet) callgraph.  If cgraph-directed inlining
     becomes the default, this loop should probably be integrated with
     the construction of the callgraph.  */
  for (node = cgraph_nodes; node; node = node->next)
    {
      /* Debugging aid; not otherwise used.  */
      node->inliner.name = (char *)IDENTIFIER_POINTER (DECL_NAME (node->decl));
      node->inliner.highest_desirability = 0.0;
      node->inliner.most_desirable_edge = NULL;
      node->inliner.additional_lines = 0;
      node->inliner.removed_execution_count = 0;
      node->inliner.top_edge = create_edge ((struct cgraph_node *)NULL,
					    (struct cgraph_node *)NULL,
					    (tree *)NULL);
      /* FIXME: should compute # of callees as-we-go here:  */
      node->inliner.top_edge->inliner.callees = cgraph_inlining_callee_list (node, &DECL_SAVED_TREE(node->decl));
      node->inliner.top_edge->inliner.callee_array = (varray_type)NULL;
      /* If there's a DECL_SAVED_TREE, and there's a CALL_EXPR inside,
	 add this to our list of callers-that-might-inline.  */
      if (node->inliner.top_edge->inliner.callees)
	VARRAY_PUSH (caller_nodes_varray, generic, node);
      node->inliner.top_edge->inliner.inline_this = FALSE;
      node->inliner.body_desirability = cgraph_call_body_desirability (node);
      node->inliner.callee_count = 0;
      /* Execution count of this function.  */
      if (DECL_SAVED_TREE (node->decl))
	node->inliner.execution_count = times_arc_executed (node->decl, /* slot */0);
      else
	node->inliner.execution_count = 0;
      total_original_statements += DECL_NUM_STMTS (node->decl);
    }

  /* Inline until the "most desriable" edge has an execution count
     *smaller* than specified on the commandline.  */
  if (flag_callgraph_inlining_count)
    {
      bloat_limit = 0;
      if (!quiet_flag)
	fprintf (stderr, "\n[** original statements: %lu inline edges with execution counts above %llu]\n",
		 total_original_statements, flag_callgraph_inlining_count);
    }
  else if (flag_callgraph_inlining_limit)
    {
      /* Compute growth target as a percentage of the original program
	 size, in lines-of-code.  "-flag-callgraph-inlining-limit=25"
	 means "grow by 25%".  If the percentage flag is zero (unset),
	 fall back to the old inliner limit.  */
      unsigned long fully_inlined_size_100 = total_original_statements
	* (flag_callgraph_inlining_limit + 100);
      unsigned long fully_inlined_size = fully_inlined_size_100 / 100;
      bloat_limit = fully_inlined_size - total_original_statements;
      if (!quiet_flag)
	fprintf (stderr, "\n[** original statements: %lu bloat limit: %lu (%u%%)]\n",
		 total_original_statements, bloat_limit, flag_callgraph_inlining_limit);
    }
  else
    bloat_limit = MAX_INLINE_INSNS / INSNS_PER_STMT;

  /* For all the CALL_EXPRs in all the functions, compute CALL_EXPR
     desirability and find the most desirable CALL_EXPR in every
     function body.  */
  for (node = cgraph_nodes; node; node = node->next)
    {
      if (node->inliner.top_edge->inliner.callees)
	{
	  /* Allocate a VARRAY for temporary random access to our callee edges.
	     This will be discarded after we've made our inlining choices.  */
	  VARRAY_GENERIC_PTR_INIT(node->inliner.top_edge->inliner.callee_array,
				  /* node->inliner.callee_count FIXME above */10, "callee edges");
	  for (edge = node->inliner.top_edge->inliner.callees;
	       edge;
	       edge = edge->next_callee)
	    {
	      VARRAY_PUSH_GENERIC_PTR (node->inliner.top_edge->inliner.callee_array, edge);

	      if (flag_use_feedback)
		edge->inliner.execution_count = times_call_executed (*edge->inliner.call_expr);
	      /* Estimate the desirability of this CALL_EXPR.  */
	      edge->inliner.desirability = cgraph_call_desirability (edge);
	      /* If this is now the hottest call in this function, remember it.  */
	      if (edge->inliner.desirability > node->inliner.highest_desirability)
		{
		  node->inliner.highest_desirability = edge->inliner.desirability;
		  node->inliner.most_desirable_edge = edge;		  
		}
	    }
	}
    }
/* haifa begin */
  if (flag_use_feedback
      && flag_callgraph_inlining_ratio)
    {
      for (node = cgraph_nodes; node; node = node->next)
        for (edge = node->inliner.top_edge->inliner.callees;
	     edge;
	     edge = edge->next_callee)
          {
	    if (edge->inliner.execution_count > max_edge_exec_count)
	      max_edge_exec_count = edge->inliner.execution_count;
          }
      max_edge_exec_count *= flag_callgraph_inlining_ratio;
    }
/* haifa end */

  /* Find function with the hottest call.  */
  do {
    unsigned int i, limit, new_statements;
    float hottest_fn_desirability = 0.0;
    struct cgraph_node *callee, *hottest_fn = (struct cgraph_node *)NULL, *step;
    struct cgraph_edge *s_edge;

    more = FALSE;
    limit = VARRAY_ACTIVE_SIZE (caller_nodes_varray);
    for (i = 0; i < limit; i++)
      {
	step = VARRAY_GENERIC_PTR (caller_nodes_varray, i);
	if (step->inliner.highest_desirability > hottest_fn_desirability)
	  {
	    hottest_fn = step;
	    hottest_fn_desirability = hottest_fn->inliner.highest_desirability;
	  }
      }

    /* If we didn't find any suitable inlining candidate, bail out.  */
    if ( ! hottest_fn)
      break;

    new_statements = DECL_NUM_STMTS (hottest_fn->inliner.most_desirable_edge->callee->decl);
    if (new_statements == 0)
      new_statements = 1;

    /* If (this next CALL_EXPR is to a fn called exactly once), OR
       (inlining this fn won't push us over the bloat limit), OR (the
       execution count of this edge is larger than
       flag_callgraph_inlining_count), THEN we'll inline it.  */
    only_call = hottest_fn->inliner.most_desirable_edge->callee->global.inline_once;
    if (only_call
	|| ((new_statements + bloat_estimate) < (unsigned int)bloat_limit)
	|| (flag_callgraph_inlining_count
	    && (hottest_fn->inliner.most_desirable_edge->inliner.execution_count > flag_callgraph_inlining_count))
/* haifa begin */
	|| (flag_callgraph_inlining_ratio
	    && ((100 * hottest_fn->inliner.most_desirable_edge->inliner.execution_count) > max_edge_exec_count))
	|| (flag_callgraph_inline_small
	    && (new_statements < flag_callgraph_inline_small)))
/* haifa end */
      {
	if (!quiet_flag)
	  cgraph_display_inlining_step (hottest_fn->inliner.most_desirable_edge, "chooses");
	/* Record decision to inline hottest call.  */
	cgraph_record_inlining_choice (hottest_fn->inliner.most_desirable_edge);
	callee = hottest_fn->inliner.most_desirable_edge->callee;
	/* Remember the callee body will be invoked fewer times
	   because of this inline.  */
	callee->inliner.removed_execution_count += hottest_fn->inliner.most_desirable_edge->inliner.execution_count;
	for (s_edge = hottest_fn->inliner.most_desirable_edge->inliner.callees;
	     s_edge ;
	     s_edge = s_edge->next_callee)
	  {
	    /* b chooses c, removes X calls to c
	       a chooses b, removes Y calls to b, restores Z calls to c */
	    s_edge->callee->inliner.removed_execution_count -= s_edge->inliner.execution_count;
	  }
	if (!quiet_flag && callee->inliner.removed_execution_count > callee->inliner.execution_count)
	  fprintf (stderr, "\n[bad removed_exec_count 0x%p %llu %llu]\n",
		   (void *)callee, callee->inliner.execution_count, callee->inliner.removed_execution_count);
	revise_desirabilities_r (callee->inliner.top_edge);
	/* Record estimated growth of this 'hottest_fn'.  */
	hottest_fn->inliner.additional_lines += new_statements;
	/* Record estimated growth of the module.  Inlining the only
	   call to a function does not increase the size of the
	   program.  Well, that's the theory, anyway.  :-)  */
	if (!only_call)
	  bloat_estimate += new_statements;
	/* Find and record new 'most_desirable_edge' in this function.  */
	hottest_fn->inliner.most_desirable_edge = cgraph_most_desirable_edge (hottest_fn->inliner.top_edge->inliner.callees);
	hottest_fn->inliner.highest_desirability = (hottest_fn->inliner.most_desirable_edge)
	  ? hottest_fn->inliner.most_desirable_edge->inliner.desirability
	  : -1.0;
	more = TRUE;
      }
  } while (more);
  choices_made = TRUE;
}

/* Perform simple optimizations based on callgraph.  */

void
cgraph_optimize ()
{
  struct cgraph_node *node;
  bool changed = true;

  cgraph_mark_local_functions ();

  cgraph_mark_functions_to_inline_once ();

  if (flag_callgraph_inlining && !flag_create_feedback)
    cgraph_make_inlining_choices ();

  cgraph_global_info_ready = true;
  if (!quiet_flag)
    dump_cgraph (stderr);
  if (!quiet_flag)
    fprintf (stderr, "\n\nAssembling functions:");

  /* Output everything.  
     ??? Our inline heuristic may decide to not inline functions previously
     marked as inlinable thus adding new function bodies that must be output.
     Later we should move all inlining decisions to callgraph code to make
     this impossible.  */
  cgraph_expand_functions ();
  if (!quiet_flag)
    fprintf (stderr, "\n\nAssembling functions that failed to inline:");
  while (changed && !errorcount && !sorrycount)
    {
      changed = false;
      for (node = cgraph_nodes; node; node = node->next)
	{
	  tree decl = node->decl;
	  if (!node->origin
	      && !TREE_ASM_WRITTEN (decl)
	      && DECL_SAVED_TREE (decl)
	      && !DECL_EXTERNAL (decl))
	    {
	      struct cgraph_edge *edge;

	      for (edge = node->callers; edge; edge = edge->next_caller)
		if (TREE_ASM_WRITTEN (edge->caller->decl))
		  {
		    changed = true;
		    cgraph_expand_function (node);
		    break;
		  }
	    }
	}
    }
}

/* APPLE LOCAL callgraph inlining */
#include "gt-cgraphunit.h"
