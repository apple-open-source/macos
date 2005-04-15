/* APPLE LOCAL file lno */
/* Elimination of redundant checks.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Contributed by Sebastian Pop <sebastian.pop@cri.ensmp.fr>

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

/* 
   Description:
   
     Compute the scalar evolutions for all the scalar variables of a
     condition expression, and based on this information performs a
     proof.  The condition is rewritten based on the result of this
     static proof.

   Examples:
   
     Example 1: A simple illustration of the algorithm.
     
     Given the COND_EXPR "if (a < b)" with "a -> {2, +, 1}_1" and "b
     -> {3, +, 1}_1", the proof consists in comparing these evolution
     functions: is it always true for a given iteration x that "{2, +,
     1}_1 (x) < {3, +, 1}_1 (x)"?  The answer is yes, and the test of
     the condition is consequently replaced by "1".  

   Further readings:
   
     There is no further readings for the moment.  

     Based on the fact that this algorithm is similar to the Value
     Range Propagation you can have a look at the corresponding
     papers: ...
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "errors.h"
#include "ggc.h"
#include "tree.h"

/* These RTL headers are needed for basic-block.h.  */
#include "rtl.h"
#include "basic-block.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "timevar.h"
#include "cfgloop.h"
#include "tree-chrec.h"
#include "tree-data-ref.h"
#include "tree-scalar-evolution.h"
#include "tree-pass.h"
#include "flags.h"


/* Given two integer constants A and B, determine whether "A >= B".  */

static inline bool
tree_is_ge (tree a, tree b, bool *res)
{
  tree cmp = fold (build (GE_EXPR, boolean_type_node, a, b));
  if (TREE_CODE (cmp) != INTEGER_CST)
    return false;

  *res = (tree_int_cst_sgn (cmp) != 0);
  return true;
}

/* Determines whether "CHREC0 (x) > CHREC1 (x)" for all the integers x
   such that "0 <= x < nb_iter".  When this property is statically
   computable, set VALUE and return true.  */

static inline bool
prove_truth_value_gt (tree type, tree chrec0, tree chrec1, bool *value)
{
  tree diff = chrec_fold_minus (type, chrec0, chrec1);
  return chrec_is_positive (diff, value);
}

/* Determines whether "CHREC0 (x) < CHREC1 (x)" for all the integers
   x such that "x >= 0".  When this property is statically computable,
   set VALUE and return true.  */

static inline bool
prove_truth_value_lt (tree type, tree chrec0, tree chrec1, bool *value)
{
  return prove_truth_value_gt (type, chrec1, chrec0, value);
}

/* Determines whether "CHREC0 (x) <= CHREC1 (x)" for all the integers
   x such that "x >= 0".  When this property is statically computable,
   set VALUE and return true.  */

static inline bool
prove_truth_value_le (tree type, tree chrec0, tree chrec1, bool *value)
{
  if (prove_truth_value_gt (type, chrec0, chrec1, value))
    {
      *value = !*value;
      return true;
    }
  
  return false;
}

/* Determines whether "CHREC0 (x) >= CHREC1 (x)" for all the integers
   x such that "x >= 0".  When this property is statically computable,
   set VALUE and return true.  */

static inline bool
prove_truth_value_ge (tree type, tree chrec0, tree chrec1, bool *value)
{
  if (prove_truth_value_gt (type, chrec1, chrec0, value))
    {
      *value = !*value;
      return true;
    }
  
  return false;
}

/* Determines whether "CHREC0 (x) == CHREC1 (x)" for all the integers
   x such that "x >= 0".  When this property is statically computable,
   set VALUE and return true.  */

static inline bool
prove_truth_value_eq (tree type, tree chrec0, tree chrec1, bool *value)
{
  tree diff = chrec_fold_minus (type, chrec0, chrec1);
  
  if (TREE_CODE (diff) == INTEGER_CST)
    {
      if (integer_zerop (diff))
	*value = true;
      
      else
	*value = false;
      
      return true;
    }
  
  else
    return false;  
}

/* Determines whether "CHREC0 (x) != CHREC1 (x)" for all the integers
   x such that "x >= 0".  When this property is statically computable,
   set VALUE and return true.  */

static inline bool
prove_truth_value_ne (tree type, tree chrec0, tree chrec1, bool *value)
{
  if (prove_truth_value_eq (type, chrec0, chrec1, value))
    {
      *value = !*value;
      return true;
    }
  
  return false;
}

/* Try to determine whether "CHREC0 (x) CODE CHREC1 (x)", using
   symbolic computations.  When this property is computable, set VALUE
   and return true.  */

static bool
prove_truth_value_symbolic (enum tree_code code, tree chrec0, tree chrec1, 
			    bool *value)
{
  tree type0 = chrec_type (chrec0);
  tree type1 = chrec_type (chrec1);

  /* Disabled for the moment.  */
  return false;

  if (type0 != type1)
    return false;

  switch (code)
    {
    case EQ_EXPR:
      return prove_truth_value_eq (type1, chrec0, chrec1, value);

    case NE_EXPR:
      return prove_truth_value_ne (type1, chrec0, chrec1, value);

    case LT_EXPR:
      return prove_truth_value_lt (type1, chrec0, chrec1, value);

    case LE_EXPR:
      return prove_truth_value_le (type1, chrec0, chrec1, value);

    case GT_EXPR:
      return prove_truth_value_gt (type1, chrec0, chrec1, value);

    case GE_EXPR:
      return prove_truth_value_ge (type1, chrec0, chrec1, value);
      
    default:
      return false;
    }
}

/* Return the negation of the comparison code.  */

static inline enum tree_code
not_code (enum tree_code code)
{
  switch (code)
    {
    case EQ_EXPR:
      return NE_EXPR;
    case NE_EXPR:
      return EQ_EXPR;
    case LT_EXPR:
      return GE_EXPR;
    case LE_EXPR:
      return GT_EXPR;
    case GT_EXPR:
      return LE_EXPR;
    case GE_EXPR:
      return LT_EXPR;
      
    default:
      return code;
    }
}

/* Determine whether "CHREC0 (x) CODE CHREC1 (x)", for all the
   integers x such that "0 <= x <= NB_ITERS_IN_LOOP".  When this
   property is statically computable, set VALUE and return true.  */

static bool
prove_truth_value (enum tree_code code, 
		   unsigned loop_nb, 
		   tree chrec0, 
		   tree chrec1, 
		   tree nb_iters_in_loop, 
		   bool *value)
{
  bool val = false;
  tree nb_iters_in_then, nb_iters_in_else;

  if (automatically_generated_chrec_p (nb_iters_in_loop))
    return prove_truth_value_symbolic (code, chrec0, chrec1, value);
  
  /* Compute the number of iterations that fall in the THEN clause,
     and the number of iterations that fall in the ELSE clause.  */
  nb_iters_in_then = first_iteration_non_satisfying 
    (code, loop_nb, chrec0, chrec1);
  nb_iters_in_else = first_iteration_non_satisfying 
    (not_code (code), loop_nb, chrec0, chrec1);
  
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "  (nb_iters_in_loop = ");
      print_generic_expr (dump_file, nb_iters_in_loop, 0);
      fprintf (dump_file, ")\n  (nb_iters_in_then = ");
      print_generic_expr (dump_file, nb_iters_in_then, 0);
      fprintf (dump_file, ")\n  (nb_iters_in_else = ");
      print_generic_expr (dump_file, nb_iters_in_else, 0);
      fprintf (dump_file, ")\n");
    }
  
  if (chrec_contains_undetermined (nb_iters_in_then)
      || chrec_contains_undetermined (nb_iters_in_else))
    return prove_truth_value_symbolic (code, chrec0, chrec1, value);
  
  if (nb_iters_in_then == chrec_known
      && integer_zerop (nb_iters_in_else))
    {
      *value = true;
      return true;
    }
  
  if (nb_iters_in_else == chrec_known
      && integer_zerop (nb_iters_in_then))
    {
      *value = false;
      return true;
    }
  
  if (TREE_CODE (nb_iters_in_then) == INTEGER_CST
      && TREE_CODE (nb_iters_in_else) == INTEGER_CST)
    {
      if (integer_zerop (nb_iters_in_then)
	  && tree_is_ge (nb_iters_in_else, nb_iters_in_loop, &val)
	  && val)
	{
	  *value = false;
	  return true;
	}
      
      if (integer_zerop (nb_iters_in_else)
	  && tree_is_ge (nb_iters_in_then, nb_iters_in_loop, &val)
	  && val)
	{
	  *value = true;
	  return true;
	}
    }

  return prove_truth_value_symbolic (code, chrec0, chrec1, value);
}

/* Remove the check by setting the condition COND to VALUE.  */

static inline void 
remove_redundant_check (tree cond, bool value)
{
  /* A dead COND_EXPR means the condition is dead. We don't change any
     flow, just replace the expression with a constant.  */
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "Replacing one of the conditions.\n");
  
  if (value == true)
    COND_EXPR_COND (cond) = integer_one_node;
  
  else
    COND_EXPR_COND (cond) = integer_zero_node;
  
  modify_stmt (cond);
}

/* If the condition TEST is decidable at compile time, then eliminate
   the check.  */

static void
try_eliminate_check (tree cond)
{
  bool value;
  tree test, opnd0, opnd1;
  tree chrec0, chrec1;
  struct loop *loop = loop_containing_stmt (cond);
  tree nb_iters = number_of_iterations_in_loop (loop);
  enum tree_code code;

  if (automatically_generated_chrec_p (nb_iters))
    return;

  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "(try_eliminate_check \n");
      fprintf (dump_file, "  (cond = ");
      print_generic_expr (dump_file, cond, 0);
      fprintf (dump_file, ")\n");
    }

  test = COND_EXPR_COND (cond);
  code = TREE_CODE (test);
  switch (code)
    {
    case SSA_NAME:
      /* Matched "if (opnd0)" ie, "if (opnd0 != 0)".  */
      opnd0 = test;
      chrec0 = instantiate_parameters 
	(loop, analyze_scalar_evolution (loop, opnd0));
      if (chrec_contains_undetermined (chrec0))
	goto end;

      chrec1 = convert (TREE_TYPE (opnd0), integer_zero_node);
      code = NE_EXPR;
      break;

    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
      opnd0 = TREE_OPERAND (test, 0);
      opnd1 = TREE_OPERAND (test, 1);

      chrec0 = instantiate_parameters 
	(loop, analyze_scalar_evolution (loop, opnd0));
      chrec1 = instantiate_parameters 
	(loop, analyze_scalar_evolution (loop, opnd1));

      if (chrec_contains_undetermined (chrec0)
	  || chrec_contains_undetermined (chrec1))
	goto end;

      break;

    default:
      goto end;
    }

  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "  (test = ");
      print_generic_expr (dump_file, test, 0);
      fprintf (dump_file, ")\n  (loop_nb = %d)\n", loop->num);
      fprintf (dump_file, "  (nb_iters = ");
      print_generic_expr (dump_file, nb_iters, 0);
      fprintf (dump_file, ")\n  (chrec0 = ");
      print_generic_expr (dump_file, chrec0, 0);
      fprintf (dump_file, ")\n  (chrec1 = ");
      print_generic_expr (dump_file, chrec1, 0);
      fprintf (dump_file, ")\n");
    }

  if (prove_truth_value (code, loop->num, chrec0, chrec1, nb_iters, &value))
    remove_redundant_check (cond, value);

 end:;
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, ")\n");
}

/* Compute the exit edges for all the loops.  */

static void 
scan_all_loops_r (struct loop *loop)
{
  if (!loop)
    return;
  
  /* Recurse on the inner loops, then on the next (sibling) loops.  */
  scan_all_loops_r (loop->inner);
  scan_all_loops_r (loop->next);
  
  flow_loop_scan (loop, LOOP_EXIT_EDGES);
}

/* Walk over all the statements, searching for conditional statements.
   
   A better way to determine the conditional expressions that are good
   candidates for elimination would be needed.  For the moment
   systematically search the conditional expressions over the whole
   function.  */

void 
eliminate_redundant_checks (void)
{
  basic_block bb;
  block_stmt_iterator bsi;

#if 0
  dump_file = stderr;
  dump_flags = 31;
#endif
  
  bb = BASIC_BLOCK (0);
  if (bb && bb->loop_father)
    {
      scan_all_loops_r (bb->loop_father);
      
      FOR_EACH_BB (bb)
	{
	  struct loop *loop = bb->loop_father;
	  
	  /* Don't try to prove anything about the loop exit
	     conditions: avoid the block that contains the condition
	     that guards the exit of the loop.  */
	  if (!loop->exit_edges
	      || loop->exit_edges[0]->src == bb)
	    continue;
	  
	  for (bsi = bsi_start (bb); !bsi_end_p (bsi); bsi_next (&bsi))
	    {
	      tree expr = bsi_stmt (bsi);
	      
	      switch (TREE_CODE (expr))
		{
		case COND_EXPR:
		  try_eliminate_check (expr);
		  break;
		  
		default:
		  break;
		}
	    }
	}
    }
}

