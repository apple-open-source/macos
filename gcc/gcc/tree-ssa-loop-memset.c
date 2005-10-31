/* APPLE LOCAL begin loops-to-memset  (ENTIRE FILE!)  */
/*  Loops to memset.
    Copyright (C) 2004 Free Software Foundation, Inc.
    Contributed by Andrew Pinski <apinski@apple.com>.
  
    This file is part of GCC.
    
    GCC is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2, or (at your option) any
    later version.
    
    GCC is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.
    
    You should have received a copy of the GNU General Public License
    along with GCC; see the file COPYING.  If not, write to the Free
    Software Foundation, 59 Temple Place - Suite 330, Boston, MA
    02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "ggc.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "timevar.h"
#include "cfgloop.h"
#include "domwalk.h"
#include "params.h"
#include "tree-pass.h"
#include "flags.h"
#include "tree-data-ref.h"
#include "tree-scalar-evolution.h"
#include "tree-chrec.h"
#include "tree-vectorizer.h"

static bool memset_debug_stats (struct loop *loop);
static bool memset_debug_details (struct loop *loop);

/* Function memset_analyze_data_refs.

   Find all the data references in the loop.

   Handle INDIRECT_REFs and one dimensional ARRAY_REFs 
   which base is really an array (not a pointer).

   This is different from vect_analyze_data_refs in tree-vectorizer.c as
   we handle unaligned data stores and we only handle data stores.  */

static bool
memset_analyze_data_refs (loop_vec_info loop_vinfo)
{
  struct loop *loop = LOOP_VINFO_LOOP (loop_vinfo);
  basic_block *bbs = LOOP_VINFO_BBS (loop_vinfo);
  int nbbs = loop->num_nodes;
  block_stmt_iterator si;
  int j;
  struct data_reference *dr;
  
  if (memset_debug_details (NULL))
    fprintf (dump_file, "\n<<memset_analyze_data_refs>>\n");
  
  for (j = 0; j < nbbs; j++)
    {
      basic_block bb = bbs[j];
      for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
	{
	  tree stmt = bsi_stmt (si);
	  stmt_vec_info stmt_info = vinfo_for_stmt (stmt);
	  v_may_def_optype v_may_defs = STMT_V_MAY_DEF_OPS (stmt);
	  v_must_def_optype v_must_defs = STMT_V_MUST_DEF_OPS (stmt);
	  vuse_optype vuses = STMT_VUSE_OPS (stmt);
	  varray_type *datarefs = NULL;
	  int nvuses, nv_may_defs, nv_must_defs;
	  tree memref = NULL;
	  tree array_base;
	  tree symbl;
	  
	  /* Assumption: there exists a data-ref in stmt, if and only if 
	     it has vuses/vdefs.  */
	  
	  if (!vuses && !v_may_defs && !v_must_defs)
	    continue;
	  
	  nvuses = NUM_VUSES (vuses);
	  nv_may_defs = NUM_V_MAY_DEFS (v_may_defs);
	  nv_must_defs = NUM_V_MUST_DEFS (v_must_defs);
	  
	  if (nvuses && (nv_may_defs || nv_must_defs))
	    {
	      if (memset_debug_details (NULL))
		{
		  fprintf (dump_file, "unexpected vdefs and vuses in stmt: ");
		  print_generic_expr (dump_file, stmt, TDF_SLIM);
		}
	      return false;
	    }
	  
	  if (TREE_CODE (stmt) != MODIFY_EXPR)
	    {
	      if (memset_debug_details (NULL))
		{
		  fprintf (dump_file, "unexpected vops in stmt: ");
		  print_generic_expr (dump_file, stmt, TDF_SLIM);
		}
	      return false;
	    }
	  
	  if (vuses)
	    {
	      if (memset_debug_details (NULL))
		{
		  fprintf (dump_file, "Memory access in the loop: ");
		  print_generic_expr (dump_file, stmt, TDF_SLIM);
		}
	      return false;
	    } 
	  else /* vdefs */
	    {
	      memref = TREE_OPERAND (stmt, 0);
	      datarefs = &(LOOP_VINFO_DATAREF_WRITES (loop_vinfo));
	    }
	  
	  if (TREE_CODE (memref) == INDIRECT_REF)
            {
	      /* MERGE FIXME */
	      abort ();
	      /* dr = vect_analyze_pointer_ref_access (memref, stmt, false); */
	      if (! dr)
		return false; 
	      symbl = DR_BASE_NAME (dr);	
            }
	  else if (TREE_CODE (memref) == ARRAY_REF)
	    {
	      tree base;		
	      array_base = TREE_OPERAND (memref, 0);
	      
	      if (TREE_CODE (array_base) == ARRAY_REF)
                {
		  if (memset_debug_stats (loop) || memset_debug_details (loop))
		    {
		      fprintf (dump_file, 
			       "not vectorized: multi-dimensional array.");
		      print_generic_expr (dump_file, stmt, TDF_SLIM);
		    }
		  return false;
                }
	      
	      dr = analyze_array (stmt, memref, false);
	      
	      /* Find the relevant symbol for aliasing purposes.  */	
	      base = DR_BASE_NAME (dr);
	      switch (TREE_CODE (base))	
		{
		case VAR_DECL:
		  symbl = base;
		  break;
		default:
		  if (memset_debug_stats (loop) 
		      || memset_debug_details (loop))
		    {
		      fprintf (dump_file,
			       "not transformed: unhandled struct/class field access ");
		      print_generic_expr (dump_file, stmt, TDF_SLIM);
		    }
		  return false;
		} /* switch */
	    }
	  else
	    {
	      if (memset_debug_stats (loop) || memset_debug_details (loop))
		{
		  fprintf (dump_file, "not transformed: unhandled data ref: ");
		  print_generic_expr (dump_file, stmt, TDF_SLIM);
		}
	      return false;
	    }
		
	  /* Find and record the memtag assigned to this data-ref.  */
	  if (TREE_CODE (symbl) == VAR_DECL 
	      || (TREE_CODE (symbl) == COMPONENT_REF 
		  && TREE_CODE (TREE_OPERAND (symbl, 0)) == VAR_DECL))
	    STMT_VINFO_MEMTAG (stmt_info) = symbl;
	  else if (TREE_CODE (symbl) == SSA_NAME)
	    {
	      tree tag;
	      symbl = SSA_NAME_VAR (symbl);
	      tag = get_var_ann (symbl)->type_mem_tag;
	      if (!tag)
		{
		  tree ptr = TREE_OPERAND (memref, 0);
		  if (TREE_CODE (ptr) == SSA_NAME)
		    tag = get_var_ann (SSA_NAME_VAR (ptr))->type_mem_tag;
		}
	      if (!tag)
		{
		  if (memset_debug_stats (loop) || memset_debug_details (loop))
		    fprintf (dump_file, "not vectorized: no memtag for ref.");
		  return false;
		}
	      STMT_VINFO_MEMTAG (stmt_info) = tag;
	    }
	  else
	    {
	      if (memset_debug_stats (loop) || memset_debug_details (loop))
		{
		  fprintf (dump_file, "not vectorized: unsupported data-ref: ");
		  print_generic_expr (dump_file, memref, TDF_SLIM);
		}
	      return false;
	    }
	  
	  VARRAY_PUSH_GENERIC_PTR (*datarefs, dr);
	  STMT_VINFO_DATA_REF (stmt_info) = dr;
	}
    }
  
  return true;
}

/* Function memset_analyze_loop_with_symbolic_num_of_iters.

   In case the number of iterations that LOOP iterates in unknown at compile 
   time, an epilog loop will be generated, and the loop induction variables 
   (IVs) will be "advanced" to the value they are supposed to take just before 
   the epilog loop. Here we check that the access function of the loop IVs
   and the expression that represents the loop bound are simple enough.
   These restrictions will be relxed in the future.  */

static bool 
memset_analyze_loop_with_symbolic_num_of_iters (tree niters, 
					        struct loop *loop)
{
  basic_block bb = loop->header;
  tree phi;

  if (memset_debug_details (NULL))
    fprintf (dump_file, 
	     "\n<<memset_analyze_loop_with_symbolic_num_of_iters>>\n");
  
  if (chrec_contains_undetermined (niters))
    {
      if (memset_debug_details (NULL))
        fprintf (dump_file, "Infinite number of iterations.");
      return false;
    }

  if (!niters)
    {
      if (memset_debug_details (NULL))
        fprintf (dump_file, "niters is NULL pointer.");
      return false;
    }

  if (memset_debug_details (NULL))
    {
      fprintf (dump_file, "Symbolic number of iterations is ");
      print_generic_expr (dump_file, niters, TDF_DETAILS);
    }
   
  /* Analyze phi functions of the loop header.  */

  for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
    {
      tree access_fn = NULL;
      tree evolution_part;

      if (memset_debug_details (NULL))
	{
          fprintf (dump_file, "Analyze phi: ");
          print_generic_expr (dump_file, phi, TDF_SLIM);
	}

      /* Skip virtual phi's. The data dependences that are associated with
         virtual defs/uses (i.e., memory accesses) are analyzed elsewhere.  */

      if (!is_gimple_reg (SSA_NAME_VAR (PHI_RESULT (phi))))
	{
	  if (memset_debug_details (NULL))
	    fprintf (dump_file, "virtual phi. skip.");
	  continue;
	}

      /* Analyze the evolution function.  */

      access_fn = instantiate_parameters
	(loop, analyze_scalar_evolution (loop, PHI_RESULT (phi)));

      if (!access_fn)
	{
	  if (memset_debug_details (NULL))
	    fprintf (dump_file, "No Access function.");
	  return false;
	}

      if (memset_debug_details (NULL))
        {
	  fprintf (dump_file, "Access function of PHI: ");
	  print_generic_expr (dump_file, access_fn, TDF_SLIM);
        }

      evolution_part = evolution_part_in_loop_num (access_fn, loop->num);
      
      if (evolution_part == NULL_TREE)
	return false;
  
      /* FORNOW: We do not transform initial conditions of IVs 
	 which evolution functions are a polynomial of degree >= 2.  */

      if (tree_is_chrec (evolution_part))
	return false;  
    }

  return  true;
}


/* Function debug_loop_details.

   For memset debug dumps.  */

static bool
memset_debug_details (struct loop *loop)
{
   basic_block bb;
   block_stmt_iterator si;
   tree node = NULL_TREE;

  if (!dump_file || !(dump_flags & TDF_DETAILS))
    return false;

  if (!loop)
    {
      fprintf (dump_file, "\n");
      return true;
    }

  if (!loop->header)
    return false;

  bb = loop->header;

  for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
    {
      node = bsi_stmt (si);
      if (node && EXPR_P (node) && EXPR_LOCUS (node))
	break;
    }

  if (node && EXPR_P (node) && EXPR_LOCUS (node)
      && EXPR_FILENAME (node) && EXPR_LINENO (node))
    {
      fprintf (dump_file, "\nloop at %s:%d: ", 
               EXPR_FILENAME (node), EXPR_LINENO (node));
      return true;
    }

  return false;
}

/* Function debug_loop_stats.

   For vectorization statistics dumps.  */

static bool
memset_debug_stats (struct loop *loop)
{
  basic_block bb;
  block_stmt_iterator si;
  tree node = NULL_TREE;

  if (!dump_file || !(dump_flags & TDF_STATS))
    return false;

  if (!loop)
    {
      fprintf (dump_file, "\n");
      return true;
    }

  if (!loop->header)
    return false;

  bb = loop->header;

  for (si = bsi_start (bb); !bsi_end_p (si); bsi_next (&si))
    {
      node = bsi_stmt (si);
      if (node && EXPR_P (node) && EXPR_LOCUS (node))
        break;
    }

  if (node && EXPR_P (node) && EXPR_LOCUS (node) 
      && EXPR_FILENAME (node) && EXPR_LINENO (node))
    {
      fprintf (dump_file, "\nloop at %s:%d: ", 
	EXPR_FILENAME (node), EXPR_LINENO (node));
      return true;
    }

  return false;
}

/* Function memset_analyze_loop_form.

   Verify the following restrictions:
   - it's an inner-most loop
   - number of BBs = 2 (which are the loop header and the latch)
   - the loop has a pre-header
   - the loop has a single entry and exit
   - the loop exit condition is simple enough, and the number of iterations
     can be analyzed (a countable loop).

   This differs from vect_analyze_loop_form by we handle non constant
   interations.  */

static loop_vec_info
memset_analyze_loop_form (struct loop *loop)
{
  loop_vec_info loop_vinfo;
  tree loop_cond;
  tree number_of_iterations = NULL_TREE;

  if (memset_debug_details (loop))
    fprintf (dump_file, "\n<<memset_analyze_loop_form>>\n");

  if (loop->level > 1		/* FORNOW: inner-most loop  */
      || loop->num_exits > 1 || loop->num_entries > 1 || loop->num_nodes != 2
      || !loop->pre_header || !loop->header || !loop->latch)
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))	
	{
	  fprintf (dump_file, "not vectorized: bad loop form.\n");
	  if (loop->level > 1)
	    fprintf (dump_file, "nested loop.\n");
	  else if (loop->num_exits > 1 || loop->num_entries > 1)
	    fprintf (dump_file, "multiple entries or exits.\n");
	  else if (loop->num_nodes != 2 || !loop->header || !loop->latch)
	    fprintf (dump_file, "too many BBs in loop.\n");
	  else if (!loop->pre_header)
	    fprintf (dump_file, "no pre-header BB for loop.\n");
	}
      
      return NULL;
    }

  /* We assume that the loop exit condition is at the end of the loop. i.e,
     that the loop is represented as a do-while (with a proper if-guard
     before the loop if needed), where the loop header contains all the
     executable statements, and the latch is empty.  */
  if (!empty_block_p (loop->latch))
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))     
        fprintf (dump_file, "not vectorized: unexpectd loop form.");
      return NULL;
    }

  if (empty_block_p (loop->header))
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))     
        fprintf (dump_file, "not transformed: empty loop.");
      return NULL;
    }

  loop_cond = vect_get_loop_niters (loop, &number_of_iterations);
  if (!loop_cond)
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))
	fprintf (dump_file, "not vectorized: complicated exit condition.\n");
      return NULL;
    }
    
  if (!number_of_iterations) 
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))
	fprintf (dump_file, 
		 "not vectorized: number of iterations cannot be computed.");
      return NULL;
    }

  loop_vinfo = new_loop_vec_info (loop);
  LOOP_VINFO_NITERS (loop_vinfo) = number_of_iterations;
  
  if (!LOOP_VINFO_NITERS_KNOWN_P (loop_vinfo))
    {
      if (memset_debug_details (NULL))
	fprintf (dump_file, "loop bound unknown.");

      /* Unknown loop bound.  */
      if (!memset_analyze_loop_with_symbolic_num_of_iters (number_of_iterations,
							   loop))
	{
          if (memset_debug_stats (loop) || memset_debug_details (loop))
	    fprintf (dump_file, "not transformed: can't determine loop bound.\n");
	  return NULL;
	}
      else
	{
	  /* We need only one loop entry for unknown loop bound support.  */
	  if (loop->num_entries != 1 || !loop->pre_header)
	    {	      
	      if (memset_debug_stats (loop) || memset_debug_details (loop))
		fprintf (dump_file, 
			 "not transformed: more than one loop entry.");
	      return NULL;
	    }
	}
    }
  else if (LOOP_VINFO_INT_NITERS (loop_vinfo) == 0)
    {
      if (memset_debug_stats (loop) || memset_debug_details (loop))
	fprintf (dump_file, "not transformed: number of iterations = 0.\n");
      return NULL;
    }

  LOOP_VINFO_EXIT_COND (loop_vinfo) = loop_cond;

  return loop_vinfo;
}

/* Mark all the variables in V_MAY_DEF or V_MUST_DEF operands for STMT for
   renaming. This becomes necessary when we modify all of a non-scalar.  */

static void
mark_all_v_defs (tree stmt)
{
  tree sym;
  ssa_op_iter iter;

  get_stmt_operands (stmt);

  FOR_EACH_SSA_TREE_OPERAND (sym, stmt, iter, SSA_OP_VIRTUAL_DEFS)
    { 
      if (TREE_CODE (sym) == SSA_NAME) 
	sym = SSA_NAME_VAR (sym);
      bitmap_set_bit (vars_to_rename, var_ann (sym)->uid);
    }
}


/* This is the main entry point for the transformation.  */
void
tree_ssa_memset (struct loops *loops)
{
  unsigned i;
  
  for (i = 1; i < loops->num; i++)
    {
      struct loop *loop = loops->parray[i];
      loop_vec_info vectorizer_info;
      varray_type writes;
      struct data_reference *drw;
      tree access_chrec;
      tree noi;
      
      if (!loop)
        continue;
      
      flow_loop_scan (loop, LOOP_ALL);
      vectorizer_info = memset_analyze_loop_form (loop);
      if (!vectorizer_info)
        continue;
      
      if (!memset_analyze_data_refs (vectorizer_info))
	{
	  if (memset_debug_details (loop))
	    fprintf (dump_file, "bad data references.");
	  destroy_loop_vec_info (vectorizer_info);
	  continue;
	}
      
      writes = LOOP_VINFO_DATAREF_WRITES (vectorizer_info);
      
      /* TODO: handle more than data write.  */
      if (VARRAY_ACTIVE_SIZE (writes) != 1)
	{
	  if (memset_debug_details (loop))
	    fprintf (dump_file, "no or more than one store.");
	  destroy_loop_vec_info (vectorizer_info);
	  continue;
	}
      
      drw = VARRAY_GENERIC_PTR (writes, 0);
      
      /* TODO: handle multi-dimension arrays.  */
      if (DR_NUM_DIMENSIONS (drw) != 1)
	{
	  if (memset_debug_details (loop))
	    fprintf (dump_file, "cannot handle multiple dimension array.");
	  destroy_loop_vec_info (vectorizer_info);
	  continue;
	}
      
      if (TREE_CODE (TREE_OPERAND (DR_STMT (drw), 1)) != INTEGER_CST)
	{
	  if (memset_debug_details (loop))
	    fprintf (dump_file, "non constant store value.");
	  destroy_loop_vec_info (vectorizer_info);
	  continue;
	}
      
      /* TODO: handle other than zero values in types
         where the unit size is greater than one. */
      if (!integer_zerop (TREE_OPERAND (DR_STMT (drw), 1))
	  && !integer_onep (TYPE_SIZE_UNIT (TREE_TYPE (DR_REF (drw)))))
	{
	  if (memset_debug_details (loop))
	    fprintf (dump_file, "cannot handle other zero value for types of other than char (for now).");
	  destroy_loop_vec_info (vectorizer_info);
	  continue;
	}
      
      access_chrec = DR_ACCESS_FN (drw, 0);
      
      noi = LOOP_VINFO_NITERS (vectorizer_info);      
      
      /* Build the memset call.  */
      {
        tree array = DR_BASE_NAME (drw);
	tree value = TREE_OPERAND (DR_STMT (drw), 1);
	tree function = implicit_built_in_decls[BUILT_IN_MEMSET];
	tree args = NULL_TREE;
	block_stmt_iterator bsi = bsi_last (loop->pre_header);
	tree array_1 = make_rename_temp (ptr_type_node, NULL);
	tree temp, stmt, var;
	tree ni_name;
	
	stmt = DR_STMT (drw);
	
	/* Remove the array access stmt. */
	{
	  block_stmt_iterator access_bsi;
	  /* Mark the MAY_DEF as needed to be renamed. */
	  mark_all_v_defs (stmt);
	  access_bsi = bsi_for_stmt (stmt);
	  bsi_remove (&access_bsi);
	}
	
	if (TREE_CODE (TREE_TYPE (array)) == ARRAY_TYPE)
	  {
	    tree type = TREE_TYPE (TREE_TYPE (array));
	    tree base = array;
	    
	    while (TREE_CODE (base) == REALPART_EXPR
		   || TREE_CODE (base) == IMAGPART_EXPR
		   || handled_component_p (base))
	      base = TREE_OPERAND (base, 0);
	    
	    if (DECL_P (base))
	      TREE_ADDRESSABLE (base) = 1;
	    
	    array = build4 (ARRAY_REF, type, array, size_zero_node,
	                    NULL_TREE, NULL_TREE);
	    array = build1 (ADDR_EXPR, build_pointer_type (TREE_TYPE (array)),
	                    array);
	  }
	
	{
	  tree start = CHREC_LEFT (access_chrec);
	  tree size_mult;
	  tree array_var;
	  start = fold_convert (TREE_TYPE (array), start);
	  size_mult = fold_convert (TREE_TYPE (array),
			            TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (array))));
	  array = fold (build2 (PLUS_EXPR, 
				TREE_TYPE (array),
				array, 
				fold (build2 (MULT_EXPR, 
					      TREE_TYPE (array), 
					      start, 
					      size_mult))));
	  array_var = create_tmp_var (TREE_TYPE (array), "tmp");
	  add_referenced_tmp_var (array_var);
	  array = force_gimple_operand (array, &stmt, false, array_var);

	  if (stmt)
	    bsi_insert_after (&bsi, stmt, BSI_CONTINUE_LINKING);
	}
	
	var = create_tmp_var (size_type_node, "tmp");
	add_referenced_tmp_var (var);
	
	noi = fold (build2 (MULT_EXPR, TREE_TYPE (noi), noi,
	                    fold_convert (TREE_TYPE (noi),
			                  TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (array))))));

	stmt = NULL_TREE;
	ni_name = force_gimple_operand (noi, &stmt, false, var);

	if (stmt)
	  bsi_insert_after (&bsi, stmt, BSI_CONTINUE_LINKING);
      
	temp = build2 (MODIFY_EXPR, void_type_node, array_1,
		       array);
	
	bsi_insert_after (&bsi, temp, BSI_CONTINUE_LINKING);
	
	args = tree_cons (NULL, ni_name, args);
	args = tree_cons (NULL, fold_convert (integer_type_node, value), args);
	args = tree_cons (NULL, array_1, args);
	
	temp = build_function_call_expr (function, args);
	
	bsi_insert_after (&bsi, temp, BSI_CONTINUE_LINKING);
      }
      
      destroy_loop_vec_info (vectorizer_info);
      
    }
  
  rewrite_into_ssa (false);
  if (!bitmap_empty_p (vars_to_rename))
    {
      /* The rewrite of ssa names may cause violation of loop closed ssa
         form invariants.  TODO -- avoid these rewrites completely.
         Information in virtual phi nodes is sufficient for it.  */
      rewrite_into_loop_closed_ssa (); 
    }
  bitmap_clear (vars_to_rename);
}

/* APPLE LOCAL end loops-to-memset (ENTIRE FILE!)  */
