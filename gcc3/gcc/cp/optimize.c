/* Perform optimizations on tree structure.
   Copyright (C) 1998, 1999, 2000, 2001 Free Software Foundation, Inc.
   Written by Mark Michell (mark@codesourcery.com).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "cp-tree.h"
#include "rtl.h"
#include "insn-config.h"
#include "input.h"
#include "integrate.h"
#include "toplev.h"
#include "varray.h"
#include "ggc.h"
#include "params.h"
#include "hashtab.h"
#include "debug.h"
#include "tree-inline.h"

/* Prototypes.  */

static tree calls_setjmp_r PARAMS ((tree *, int *, void *));
static void update_cloned_parm PARAMS ((tree, tree));
static void dump_function PARAMS ((enum tree_dump_index, tree));

/* Optimize the body of FN. */

void
optimize_function (fn)
     tree fn;
{
  dump_function (TDI_original, fn);

  /* While in this function, we may choose to go off and compile
     another function.  For example, we might instantiate a function
     in the hopes of inlining it.  Normally, that wouldn't trigger any
     actual RTL code-generation -- but it will if the template is
     actually needed.  (For example, if it's address is taken, or if
     some other function already refers to the template.)  If
     code-generation occurs, then garbage collection will occur, so we
     must protect ourselves, just as we do while building up the body
     of the function.  */
  ++function_depth;

  if (flag_inline_trees
      /* We do not inline thunks, as (a) the backend tries to optimize
         the call to the thunkee, (b) tree based inlining breaks that
         optimization, (c) virtual functions are rarely inlineable,
         and (d) ASM_OUTPUT_MI_THUNK is there to DTRT anyway.  */
      && !DECL_THUNK_P (fn))
    {
      optimize_inline_calls (fn);

      dump_function (TDI_inlined, fn);
    }
  
  /* Undo the call to ggc_push_context above.  */
  --function_depth;
  
  dump_function (TDI_optimized, fn);
}

/* Called from calls_setjmp_p via walk_tree.  */

static tree
calls_setjmp_r (tp, walk_subtrees, data)
     tree *tp;
     int *walk_subtrees ATTRIBUTE_UNUSED;
     void *data ATTRIBUTE_UNUSED;
{
  /* We're only interested in FUNCTION_DECLS.  */
  if (TREE_CODE (*tp) != FUNCTION_DECL)
    return NULL_TREE;

  return setjmp_call_p (*tp) ? *tp : NULL_TREE;
}

/* Returns non-zero if FN calls `setjmp' or some other function that
   can return more than once.  This function is conservative; it may
   occasionally return a non-zero value even when FN does not actually
   call `setjmp'.  */

int
calls_setjmp_p (fn)
     tree fn;
{
  return walk_tree_without_duplicates (&DECL_SAVED_TREE (fn),
				       calls_setjmp_r,
				       NULL) != NULL_TREE;
}

/* CLONED_PARM is a copy of CLONE, generated for a cloned constructor
   or destructor.  Update it to ensure that the source-position for
   the cloned parameter matches that for the original, and that the
   debugging generation code will be able to find the original PARM.  */

static void
update_cloned_parm (parm, cloned_parm)
     tree parm;
     tree cloned_parm;
{
  DECL_ABSTRACT_ORIGIN (cloned_parm) = parm;

  /* We may have taken its address. */
  TREE_ADDRESSABLE (cloned_parm) = TREE_ADDRESSABLE (parm);

  /* The definition might have different constness. */
  TREE_READONLY (cloned_parm) = TREE_READONLY (parm);
  
  TREE_USED (cloned_parm) = TREE_USED (parm);
  
  /* The name may have changed from the declaration. */
  DECL_NAME (cloned_parm) = DECL_NAME (parm);
  DECL_SOURCE_FILE (cloned_parm) = DECL_SOURCE_FILE (parm);
  DECL_SOURCE_LINE (cloned_parm) = DECL_SOURCE_LINE (parm);
}

/* APPLE LOCAL begin structor thunks */
/* FN is a constructor or destructor, and there are FUNCTION_DECLs cloned from it nearby.
   If the clone and the original funciton have identical parameter lists,
   it is a fully-degenerate (does absolutely nothing) thunk.
   Make the clone an alias for the original function label.  */
int
maybe_alias_body (fn, clone)
     tree fn;
     tree clone;
{
  extern FILE *asm_out_file;

#ifdef ASM_MAYBE_ALIAS_BODY
  ASM_MAYBE_ALIAS_BODY (asm_out_file, fn, clone);
#endif
  return 0;
}

/* FN is a constructor or destructor, and there are FUNCTION_DECLs cloned from it nearby.
   Instead of cloning this body, leave it alone and create tiny one-call bodies
   for the cloned FUNCTION_DECLs.  These clones are sibcall candidates, and their resulting code
   will be very thunk-esque.  */
int
maybe_thunk_body (fn)
     tree fn;
{
  tree call, clone, expr_stmt, fn_parm, last_arg, start;
  int parmno, vtt_parmno;

  if (flag_apple_kext || flag_clone_structors)
    return 0;

  /* If we've already seen this structor, avoid re-processing it.  */
  if (TREE_ASM_WRITTEN (fn))
    return 1;

  /* If function accepts variable arguments, give up.  */
  last_arg = tree_last (TYPE_ARG_TYPES (TREE_TYPE (fn)));
  if ( ! VOID_TYPE_P (TREE_VALUE (last_arg)))
       return 0;

  /* If constructor expects vector (AltiVec) arguments, give up.  */
  for (fn_parm = DECL_ARGUMENTS( fn); fn_parm; fn_parm = TREE_CHAIN (fn_parm))
    if (TREE_CODE (fn_parm) == VECTOR_TYPE)
      return 0;

  /* If we don't see a clone, nothing to do.  */
  clone = TREE_CHAIN (fn);
  if (!clone || ! DECL_CLONED_FUNCTION_P (clone))
    return 0;

  /* This is only a win if there are two or more clones.  */
  if ( ! TREE_CHAIN (clone))
    return 0;

  /* Only thunk-ify non-trivial structors.  */
  if (DECL_NUM_STMTS (fn) < 2)
     return 0;

  /* If we got this far, we've decided to turn the clones into thunks.  */

  /* Find the vtt_parm, if present.  */
  for (vtt_parmno = -1, parmno = 0, fn_parm = DECL_ARGUMENTS (fn);
       fn_parm;
       ++parmno, fn_parm = TREE_CHAIN (fn_parm))
    {
      if (DECL_ARTIFICIAL (fn_parm) && DECL_NAME (fn_parm) == vtt_parm_identifier)
	{
	  vtt_parmno = parmno;	/* Compensate for removed in_charge parameter.  */
	  break;
	}
    }

  /* We know that any clones immediately follow FN in the TYPE_METHODS
     list.  */
  for (clone = start = TREE_CHAIN (fn);
       clone && DECL_CLONED_FUNCTION_P (clone);
       clone = TREE_CHAIN (clone))
    {
      tree clone_parm, parmlist;

      /* If the clone and original parmlists are identical, turn the clone into an alias.  */
      if (maybe_alias_body (fn, clone))
	continue;

      /* If we've already generated a body for this clone, avoid duplicating it.
	 (Is it possible for a clone-list to grow after we first see it?)  */
      if (DECL_SAVED_TREE (clone) || TREE_ASM_WRITTEN (clone))
	continue;

      /* Start processing the function.  */
      push_to_top_level ();
      start_function (NULL_TREE, clone, NULL_TREE, SF_PRE_PARSED);

      /* Walk parameter lists together, creating parameter list for call to original function.  */
      for (parmno = 0,
	     parmlist = NULL,
	     fn_parm = DECL_ARGUMENTS (fn),
	     clone_parm = DECL_ARGUMENTS (clone);
	   fn_parm;
	   ++parmno,
	     fn_parm = TREE_CHAIN (fn_parm))
	{
	  if (parmno == vtt_parmno && ! DECL_HAS_VTT_PARM_P (clone))
	    {
	      tree typed_null_pointer_node = copy_node (null_pointer_node);
	      TREE_TYPE (typed_null_pointer_node) = TREE_TYPE (fn_parm);
	      parmlist = tree_cons (NULL, typed_null_pointer_node, parmlist);
	    }
	  else if (parmno == 1 && DECL_HAS_IN_CHARGE_PARM_P (fn))
	    {
	      tree in_charge = in_charge_arg_for_name (DECL_NAME (clone));
	      parmlist = tree_cons (NULL, in_charge, parmlist);
	    }
	  /* Map other parameters to their equivalents in the cloned
	     function.  */
	  else
	    {
	      my_friendly_assert (clone_parm, 0);
	      parmlist = tree_cons (NULL, clone_parm, parmlist);
	      if (parmno == vtt_parmno && DECL_HAS_VTT_PARM_P (clone))
		DECL_ABSTRACT_ORIGIN (clone_parm) = fn_parm;
	      clone_parm = TREE_CHAIN (clone_parm);
	    }
	}

      /* We built this list backwards; fix now.  */
      parmlist = nreverse (parmlist);
      mark_used (fn);
      call = build_function_call (fn, parmlist);
      expr_stmt = build_stmt (EXPR_STMT, call);
      add_stmt (expr_stmt);

      /* Now, expand this function into RTL, if appropriate.  */
      finish_function (0);
      /* BLOCK_ABSTRACT_ORIGIN (DECL_INITIAL (clone)) = DECL_INITIAL (fn); ?? */
      expand_body (clone);
      pop_from_top_level ();
    }
  return 1;
}
/* APPLE LOCAL end structor thunks */

/* FN is a function that has a complete body.  Clone the body as
   necessary.  Returns non-zero if there's no longer any need to
   process the main body.  */

int
maybe_clone_body (fn)
     tree fn;
{
  tree clone;
  int first = 1;

  /* We only clone constructors and destructors.  */
  if (!DECL_MAYBE_IN_CHARGE_CONSTRUCTOR_P (fn)
      && !DECL_MAYBE_IN_CHARGE_DESTRUCTOR_P (fn))
    return 0;

  /* Emit the DWARF1 abstract instance.  */
  (*debug_hooks->deferred_inline_function) (fn);

  /* We know that any clones immediately follow FN in the TYPE_METHODS
     list.  */
  for (clone = TREE_CHAIN (fn);
       clone && DECL_CLONED_FUNCTION_P (clone);
       clone = TREE_CHAIN (clone), first = 0)
    {
      tree parm;
      tree clone_parm;

      /* Update CLONE's source position information to match FN's.  */
      DECL_SOURCE_FILE (clone) = DECL_SOURCE_FILE (fn);
      DECL_SOURCE_LINE (clone) = DECL_SOURCE_LINE (fn);
      DECL_INLINE (clone) = DECL_INLINE (fn);
      DECL_DECLARED_INLINE_P (clone) = DECL_DECLARED_INLINE_P (fn);
      DECL_COMDAT (clone) = DECL_COMDAT (fn);
      DECL_WEAK (clone) = DECL_WEAK (fn);
      DECL_ONE_ONLY (clone) = DECL_ONE_ONLY (fn);
      DECL_SECTION_NAME (clone) = DECL_SECTION_NAME (fn);
      DECL_USE_TEMPLATE (clone) = DECL_USE_TEMPLATE (fn);
      DECL_EXTERNAL (clone) = DECL_EXTERNAL (fn);
      DECL_INTERFACE_KNOWN (clone) = DECL_INTERFACE_KNOWN (fn);
      DECL_NOT_REALLY_EXTERN (clone) = DECL_NOT_REALLY_EXTERN (fn);
      TREE_PUBLIC (clone) = TREE_PUBLIC (fn);
      /* APPLE LOCAL private extern  */
      DECL_PRIVATE_EXTERN (clone) = DECL_PRIVATE_EXTERN (fn);
      /* APPLE LOCAL coalescing  */
      DECL_COALESCED (clone) = DECL_COALESCED (fn);

      /* Adjust the parameter names and locations. */
      parm = DECL_ARGUMENTS (fn);
      clone_parm = DECL_ARGUMENTS (clone);
      /* Update the `this' parameter, which is always first.  */
      update_cloned_parm (parm, clone_parm);
      parm = TREE_CHAIN (parm);
      clone_parm = TREE_CHAIN (clone_parm);
      if (DECL_HAS_IN_CHARGE_PARM_P (fn))
	parm = TREE_CHAIN (parm);
      if (DECL_HAS_VTT_PARM_P (fn))
	parm = TREE_CHAIN (parm);
      if (DECL_HAS_VTT_PARM_P (clone))
	clone_parm = TREE_CHAIN (clone_parm);
      for (; parm;
	   parm = TREE_CHAIN (parm), clone_parm = TREE_CHAIN (clone_parm))
	{
	  /* Update this parameter.  */
	  update_cloned_parm (parm, clone_parm);
	  /* We should only give unused information for one clone. */
	  if (!first)
	    TREE_USED (clone_parm) = 1;
	}
      /* APPLE LOCAL structor thunks */
    }

  /* APPLE LOCAL begin structor thunks */
  /* If we decide to turn clones into thunks, they will branch to fn.
     Must have original function available to call.  */
  if (maybe_thunk_body (fn))
    return 0;
  /* APPLE LOCAL end structor thunks */

  /* APPLE LOCAL begin structor thunks */
  /* We know that any clones immediately follow FN in the TYPE_METHODS
     list.  */
  for (clone = TREE_CHAIN (fn);
       clone && DECL_CLONED_FUNCTION_P (clone);
       clone = TREE_CHAIN (clone))
    {
      tree parm;
      tree clone_parm;
      int parmno;
      splay_tree decl_map;
   /* APPLE LOCAL end structor thunks */

      /* Start processing the function.  */
      push_to_top_level ();
      start_function (NULL_TREE, clone, NULL_TREE, SF_PRE_PARSED);

      /* Remap the parameters.  */
      decl_map = splay_tree_new (splay_tree_compare_pointers, NULL, NULL);
      for (parmno = 0,
	     parm = DECL_ARGUMENTS (fn),
	     clone_parm = DECL_ARGUMENTS (clone);
	   parm;
	   ++parmno,
	     parm = TREE_CHAIN (parm))
	{
	  /* Map the in-charge parameter to an appropriate constant.  */
	  if (DECL_HAS_IN_CHARGE_PARM_P (fn) && parmno == 1)
	    {
	      tree in_charge;
	      in_charge = in_charge_arg_for_name (DECL_NAME (clone));
	      splay_tree_insert (decl_map,
				 (splay_tree_key) parm,
				 (splay_tree_value) in_charge);
	    }
	  else if (DECL_ARTIFICIAL (parm)
		   && DECL_NAME (parm) == vtt_parm_identifier)
	    {
	      /* For a subobject constructor or destructor, the next
		 argument is the VTT parameter.  Remap the VTT_PARM
		 from the CLONE to this parameter.  */
	      if (DECL_HAS_VTT_PARM_P (clone))
		{
		  DECL_ABSTRACT_ORIGIN (clone_parm) = parm;
		  splay_tree_insert (decl_map,
				     (splay_tree_key) parm,
				     (splay_tree_value) clone_parm);
		  clone_parm = TREE_CHAIN (clone_parm);
		}
	      /* Otherwise, map the VTT parameter to `NULL'.  */
	      else
		{
		  splay_tree_insert (decl_map,
				     (splay_tree_key) parm,
				     (splay_tree_value) null_pointer_node);
		}
	    }
	  /* Map other parameters to their equivalents in the cloned
	     function.  */
	  else
	    {
	      splay_tree_insert (decl_map,
				 (splay_tree_key) parm,
				 (splay_tree_value) clone_parm);
	      clone_parm = TREE_CHAIN (clone_parm);
	    }
	}

      /* Clone the body.  */
      clone_body (clone, fn, decl_map);

      /* There are as many statements in the clone as in the
	 original.  */
      DECL_NUM_STMTS (clone) = DECL_NUM_STMTS (fn);

      /* Clean up.  */
      splay_tree_delete (decl_map);

      /* Now, expand this function into RTL, if appropriate.  */
      finish_function (0);
      BLOCK_ABSTRACT_ORIGIN (DECL_INITIAL (clone)) = DECL_INITIAL (fn);
      expand_body (clone);
      pop_from_top_level ();
    }

  /* We don't need to process the original function any further.  */
  return 1;
}

/* Dump FUNCTION_DECL FN as tree dump PHASE. */

static void
dump_function (phase, fn)
     enum tree_dump_index phase;
     tree fn;
{
  FILE *stream;
  int flags;

  stream = dump_begin (phase, &flags);
  if (stream)
    {
      fprintf (stream, "\n;; Function %s",
	       decl_as_string (fn, TFF_DECL_SPECIFIERS));
      fprintf (stream, " (%s)\n",
	       decl_as_string (DECL_ASSEMBLER_NAME (fn), 0));
      fprintf (stream, ";; enabled by -%s\n", dump_flag_name (phase));
      fprintf (stream, "\n");
      
      dump_node (fn, TDF_SLIM | flags, stream);
      dump_end (phase, stream);
    }
}
