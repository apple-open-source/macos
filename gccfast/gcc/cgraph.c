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

#include "config.h"
#include "system.h"
/* APPLE LOCAL begin callgraph inlining */
/* #include "coretypes.h" */
/* #include "tm.h" */
/* APPLE LOCAL end callgraph inlining */
#include "tree.h"
#include "tree-inline.h"
#include "langhooks.h"
#include "hashtab.h"
#include "toplev.h"
#include "flags.h"
#include "ggc.h"
#include "debug.h"
#include "target.h"
/* APPLE LOCAL begin callgraph inlining */
#include "varray.h"
#include "cgraph.h"

/* The known declarations must not get garbage collected.  Callgraph
   datastructures should not get saved via PCH code since this would
   make it difficult to extend into intra-module optimizer later.  So
   we store only the references into the array to prevent gabrage
   collector from deleting live data.  */
static GTY(()) varray_type known_fns;
/* APPLE LOCAL end callgraph inlining */

/* Hash table used to convert declarations into nodes.  */
static GTY((param_is (struct cgraph_node))) htab_t cgraph_hash;

/* APPLE LOCAL callgraph inlining */
/* The linked list of cgraph nodes.  */
struct cgraph_node *cgraph_nodes;

/* APPLE LOCAL callgraph inlining */
/* Number of nodes in existence.  */
int cgraph_n_nodes;

/* Set when whole unit has been analyzed so we can access global info.  */
bool cgraph_global_info_ready = false;

/* APPLE LOCAL begin callgraph inlining */
static void cgraph_remove_edge PARAMS ((struct cgraph_node *, struct cgraph_node *));
static hashval_t hash_node PARAMS ((const PTR));
static int eq_node PARAMS ((const PTR, const PTR));
static struct cgraph_edge *clone_callee_list   PARAMS ((struct cgraph_edge *, struct cgraph_edge *));
static char *callee_string PARAMS ((tree));
static void dump_inlining_choices PARAMS ((FILE *, struct cgraph_edge *));
/* APPLE LOCAL end callgraph inlining */

/* Returns a hash code for P.  */

static hashval_t
hash_node (p)
     const PTR p;
{
  return (hashval_t)
    htab_hash_pointer (DECL_ASSEMBLER_NAME
		       (((struct cgraph_node *) p)->decl));
}

/* Returns non-zero if P1 and P2 are equal.  */

static int
eq_node (p1, p2)
     const PTR p1;
     const PTR p2;
{
  return ((DECL_ASSEMBLER_NAME (((struct cgraph_node *) p1)->decl)) ==
	  DECL_ASSEMBLER_NAME ((tree) p2));
}

/* APPLE LOCAL callgraph inlining */
/* Return cgraph node assigned to DECL.  Create new one when needed.  */
struct cgraph_node *
cgraph_node (decl)
     tree decl;
{
  struct cgraph_node *node;
  struct cgraph_node **slot;
  struct cgraph_node *step;
  static bool cgraph_hash_init = FALSE;

  /* APPLE LOCAL begin callgraph inlining */
  if (TREE_CODE (decl) != FUNCTION_DECL)
    abort ();

  if (!cgraph_hash_init)
    {
      cgraph_hash = htab_create_ggc (10, hash_node, eq_node, NULL);
      VARRAY_TREE_INIT (known_fns, 32, "known_fns");
      /* Rebuild our hashtable after waking from a PCH-addled sleep.  */
      for (step = cgraph_nodes; step ; step = step->next)
	{
	  slot =
	    (struct cgraph_node **)
	    htab_find_slot_with_hash (cgraph_hash, step->decl,
				      htab_hash_pointer
				      (DECL_ASSEMBLER_NAME
				       (step->decl)), 1);
						      
	  *slot = step;
	}
      cgraph_hash_init = TRUE;
    }

  slot =
    (struct cgraph_node **) htab_find_slot_with_hash (cgraph_hash, decl,
						      htab_hash_pointer
						      (DECL_ASSEMBLER_NAME
						       (decl)), 1);
  if (*slot)
    return *slot;
  node = (struct cgraph_node *) ggc_alloc_cleared (sizeof (*node));
  node->decl = decl;
  node->next = cgraph_nodes;
  if (cgraph_nodes)
    cgraph_nodes->previous = node;
  node->previous = NULL;
  cgraph_nodes = node;
  cgraph_n_nodes++;
  *slot = node;
  /* APPLE LOCAL callgraph inlining */
  if (DECL_CONTEXT (decl) && TREE_CODE (DECL_CONTEXT (decl)) == FUNCTION_DECL)
    {
      node->origin = cgraph_node (DECL_CONTEXT (decl));
      node->next_nested = node->origin->nested;
      node->origin->nested = node;
    }
  VARRAY_PUSH_TREE (known_fns, decl);
  return node;
}

/* Create edge from CALLER to CALLEE in the cgraph.  */

/* APPLE LOCAL begin callgraph inlining */
struct cgraph_edge *
create_edge (caller, callee, call_expr)
     struct cgraph_node *caller, *callee;
     tree *call_expr;
{
  struct cgraph_edge *edge = (struct cgraph_edge *) ggc_alloc_cleared (sizeof (struct cgraph_edge));

  edge->caller = caller;
  edge->callee = callee;
  if (callee)
    {
      edge->next_caller = callee->callers;
      callee->callers = edge;
    }
  if (caller)
    {
      edge->next_callee = caller->callees;
      caller->callees = edge;
    }
  edge->inliner.call_expr = call_expr;
  /* i = times_call_executed (*call_expr); */
  /* edge->inline.execution_count = (i > 0) ? i : 0 ; */
  return edge;
}
/* APPLE LOCAL end callgraph inlining */

/* Remove the edge from CALLER to CALLEE in the cgraph.  */

/* APPLE LOCAL begin callgraph inlining */
static void
cgraph_remove_edge (caller, callee)
     struct cgraph_node *caller, *callee;
{
  struct cgraph_edge **edge, **edge2;

  for (edge = &callee->callers; *edge && (*edge)->caller != caller;
       edge = &((*edge)->next_caller))
    continue;
  if (!*edge)
    abort ();
  *edge = (*edge)->next_caller;
  for (edge2 = &caller->callees; *edge2 && (*edge2)->callee != callee;
       edge2 = &(*edge2)->next_callee)
    continue;
  if (!*edge2)
    abort ();
  *edge2 = (*edge2)->next_callee;
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Remove the node from cgraph.  */

void
cgraph_remove_node (node)
     struct cgraph_node *node;
{
  while (node->callers)
    cgraph_remove_edge (node->callers->caller, node);
  while (node->callees)
    cgraph_remove_edge (node, node->callees->callee);
  while (node->nested)
    cgraph_remove_node (node->nested);
  if (node->origin)
    {
      struct cgraph_node **node2 = &node->origin->nested;

      while (*node2 != node)
	node2 = &(*node2)->next_nested;
      *node2 = node->next_nested;
    }
  if (node->previous)
    node->previous->next = node->next;
  else
    cgraph_nodes = node;
  if (node->next)
    node->next->previous = node->previous;
  DECL_SAVED_TREE (node->decl) = NULL;
  /* Do not free the structure itself so the walk over chain can continue.  */
}
/* APPLE LOCAL end callgraph inlining */


/* APPLE LOCAL begin callgraph inlining */
/* Record call from CALLER to CALLEE  */

struct cgraph_edge *
cgraph_record_call (caller, callee, call_expr, invocations)
     tree caller, callee, *call_expr;
     HOST_WIDE_INT invocations;
{
  struct cgraph_node *node = cgraph_node (caller);
  struct cgraph_edge *edge = create_edge (cgraph_node (caller), cgraph_node (callee), call_expr);
  varray_type callee_array = (varray_type)NULL;
  edge->inliner.invocations = invocations;
  edge->inliner.desirability = cgraph_call_desirability (edge);
  node->inliner.callee_count++;
  if (node->inliner.top_edge)
    callee_array = node->inliner.top_edge->inliner.callee_array;
  /* Record the address of the pointer to the call.  */
  if (callee_array)
    VARRAY_PUSH_GENERIC_PTR (callee_array, call_expr);
  return edge;
}
/* APPLE LOCAL end callgraph inlining */

void
cgraph_remove_call (caller, callee)
     tree caller, callee;
{
  /* APPLE LOCAL callgraph inlining */
  cgraph_remove_edge (cgraph_node (caller), cgraph_node (callee));
}

/* Return true when CALLER_DECL calls CALLEE_DECL.  */

bool
cgraph_calls_p (caller_decl, callee_decl)
     tree caller_decl, callee_decl;
{
  struct cgraph_node *caller = cgraph_node (caller_decl);
  struct cgraph_node *callee = cgraph_node (callee_decl);
  struct cgraph_edge *edge;

  for (edge = callee->callers; edge && (edge)->caller != caller;
       edge = (edge->next_caller))
    continue;
  return edge != NULL;
}

/* APPLE LOCAL begin callgraph inlining */
/* Return local info for the compiled function.  */

struct cgraph_local_info *
cgraph_local_info (decl)
     tree decl;
{
  struct cgraph_node *node;
  if (TREE_CODE (decl) != FUNCTION_DECL)
    abort ();
  node = cgraph_node (decl);
  return &node->local;
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Return local info for the compiled function.  */

struct cgraph_global_info *
cgraph_global_info (decl)
     tree decl;
{
  struct cgraph_node *node;
  if (TREE_CODE (decl) != FUNCTION_DECL || !cgraph_global_info_ready)
    abort ();
  node = cgraph_node (decl);
  return &node->global;
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Return local info for the compiled function.  */

struct cgraph_rtl_info *
cgraph_rtl_info (decl)
     tree decl;
{
  struct cgraph_node *node;
  if (TREE_CODE (decl) != FUNCTION_DECL)
    abort ();
  node = cgraph_node (decl);
  if (decl != current_function_decl
      && !TREE_ASM_WRITTEN (node->decl))
    return NULL;
  return &node->rtl;
}
/* APPLE LOCAL end callgraph inlining */


/* APPLE LOCAL begin callgraph inlining */
/* Duplicate the given list of callee edges.  */

static inline struct cgraph_edge *
clone_callee_list (callee, caller)
     struct cgraph_edge *callee, *caller;
{
  struct cgraph_edge *callee_list = callee->inliner.callees;
  struct cgraph_edge *new_list=NULL, *new, *prev=(struct cgraph_edge *)NULL, *step;
  double scale;
  if (caller->callee->inliner.execution_count == 0)
    scale = 0.0;
  else
    scale = (double)caller->inliner.execution_count 
	    / (double)caller->callee->inliner.execution_count;
  /* It's necessary for scale * caller->callee->inliner.execution_count
     to be == caller->inliner.execution.count.  If this is not the case
     tweak scale until it is.  */
  {
    HOST_WIDEST_INT tweak = 1;
    unsigned int maxtweak = 10;
    while (((HOST_WIDEST_INT)(scale * caller->callee->inliner.execution_count)
	    < caller->inliner.execution_count)
	   && --maxtweak)
      scale = (double)caller->inliner.execution_count
	      / ((double)caller->callee->inliner.execution_count - tweak++);
    tweak = 1;
    maxtweak = 10;
    while (((HOST_WIDEST_INT)(scale * caller->callee->inliner.execution_count)
	    > caller->inliner.execution_count)
	   && --maxtweak)
      scale = (double)caller->inliner.execution_count
	      / ((double)caller->callee->inliner.execution_count + tweak++);
  }
  for (step = callee_list ; step ; step = step->next_callee)
    {
      new = create_edge (step->caller, step->callee, (tree *)NULL);
      *new = *step;
      new->inliner.inline_this = FALSE;
      new->inliner.callees = (struct cgraph_edge *)NULL;
      new->inliner.callee_array = (varray_type)NULL;
      new->inliner.uplink = caller;
      if (flag_use_feedback)
	{
	  /* Scale the execution counts of the CALL_EXPRs created by 
	     inlining this CALL_EXPR.  The scale factor was computed
	     above and is the same for each call.  It is also the
	     same as the scale we'll use for nodes other than calls. */
	  new->inliner.execution_count =
	    (HOST_WIDE_INT)(step->inliner.execution_count * scale);
	  /* Can't set_times_call_executed() here becuase this CALL_EXPR hasn't been created yet.  :-) */
	  new->inliner.desirability = cgraph_call_desirability (new);
	}
      /* FIXME: incomplete callgraph.  */
      new->next_caller = (struct cgraph_edge *)NULL;
      if (prev)
	prev->next_callee = new;
      if (!new_list)
	new_list = new;
      prev = new;
    }
  return new_list;
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Record decision to inline this call.  */
void
cgraph_record_inlining_choice (edge)
     struct cgraph_edge *edge;
{
  /* We've chosen to inline the call represented by this edge.  When
     this call has been replaced by the body of the callee, here is
     the list of calls from the callee body that will now be part of
     the callers body. */
  /* FIXME: Choose one? */
  edge->inliner.callees = clone_callee_list (edge->callee->inliner.top_edge, edge);
  edge->inliner.callee_array = NULL;
  edge->inliner.inline_this = TRUE;
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Given a CALL_EXPR tree, return a the (char *) name of the callee.  Debugging only.  */
static inline char *
callee_string (call_node)
     tree call_node;
{
  tree addr_node, decl_node;

  if (TREE_CODE (call_node) == CALL_EXPR)
    {
      addr_node = TREE_OPERAND (call_node, 0);
      if (TREE_CODE (addr_node) == ADDR_EXPR)
	{
	  decl_node = TREE_OPERAND (addr_node, 0);
	  if (TREE_CODE (decl_node) == FUNCTION_DECL)
	    return (char *) IDENTIFIER_POINTER (DECL_NAME (decl_node));
	}
    }
  abort();
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */
/* Dump a list of inlining choices.  */

static inline void
dump_inlining_choices (f, inlines)
     FILE *f;
     struct cgraph_edge *inlines;
{
  struct cgraph_edge *step;
  char *name;

  for (step = inlines; step; step = step->next_callee)
    {
      name = callee_string (*step->inliner.call_expr);
      fprintf (f, " %s", name ? name : "");
      fprintf (f, " [%llu %e]", step->inliner.execution_count,
	       step->inliner.desirability);
      if (step->inliner.callees)
	{
	  fprintf (f, "(");
	  dump_inlining_choices (f, step->inliner.callees);
	  fprintf (f, ")");
	}
    }
}
/* APPLE LOCAL end callgraph inlining */

/* APPLE LOCAL begin callgraph inlining */

/* Dump the callgraph.  */

void
dump_cgraph (f)
     FILE *f;
{
  struct cgraph_node *node;

  fprintf (f, "\nCallgraph:\n\n");
  for (node = cgraph_nodes; node; node = node->next)
    {
      struct cgraph_edge *edge;
      fprintf (f, "%s", IDENTIFIER_POINTER (DECL_NAME (node->decl)));
      if (node->origin)
	fprintf (f, " nested in: %s",
		 IDENTIFIER_POINTER (DECL_NAME (node->origin->decl)));
      if (node->needed)
	fprintf (f, " needed");
      else if (node->reachable)
	fprintf (f, " reachable");
      if (DECL_SAVED_TREE (node->decl))
	fprintf (f, " tree");

      fprintf (f, "\n  called by :");
      for (edge = node->callers; edge; edge = edge->next_caller)
	fprintf (f, "%s ",
		 IDENTIFIER_POINTER (DECL_NAME (edge->caller->decl)));

      fprintf (f, "\n  calls: ");
      for (edge = node->callees; edge; edge = edge->next_callee)
	fprintf (f, "%s ",
		 IDENTIFIER_POINTER (DECL_NAME (edge->callee->decl)));

      /* APPLE LOCAL callgraph inlining */
      /* big deletion; code has moved to callgraphunit.c  */

      if (node->inliner.top_edge && node->inliner.top_edge->inliner.callees)
	{
	  fprintf (f, "\n  inlines: ");
	  dump_inlining_choices (f, node->inliner.top_edge->inliner.callees);
	}
      else
	fprintf (f, "\n  (no inlines)");

      fprintf (f, "\n  decl: %p  saved_tree: %p  exec_count: %llu\n",
	       (void *)node->decl, (void *)DECL_SAVED_TREE (node->decl),
	       node->inliner.execution_count);
      fprintf (f, "\n");
    }
}

#include "gt-cgraph.h"

/* APPLE LOCAL end callgraph inlining */
