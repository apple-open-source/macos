/* APPLE LOCAL PFE */
/* Freeze/thaw common language trees and other data.
   Copyright (C) 2001  Free Software Foundation, Inc.
   Contributed by Ira L. Ruben (ira@apple.com)

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/*
  All the pointers that require freezing or thawing defined by tree.h
  are handled here.  Language specific handling of ..._DECL, ..._TYPE,
  and "special" (type 'x' and 'c') nodes are handled by three REQUIRED
  routines which should be uniquely defined as a function of that
  language (e.g., for C, C++, etc.) via the language hooks.
  
  For ..._DECL ('d' type) nodes,
     int pfe_freeze_thaw_decl(tree node);
  
  For ..._TYPE ('t' type) nodes,    
     int pfe_freeze_thaw_type(tree node);
  
  For 'x' and 'c' type nodes,    
     int pfe_freeze_thaw_special(tree node);
  
  These routines are expected to handle the language-specific portions of
  the specified tree node.  It should freeze or thaw depending on the
  setting of pfe_operation (in pfe.h).  For convenience a single function
  pointer, pfe_freeze_thaw_ptr_fp, pointing to either pfe_freeze_ptr() or
  pfe_thaw_ptr() is set (as determined by the action) to allow factoring
  out of the freeze and thaw code when such operations are symmetric.
  
  Each of these routines are expected to return 0 if the routine does not
  handle the node.  If it does, then 1 should be returned if the common
  processing of certain selected nodes is to be bypassed.  See code in
  freeze_thaw_tree_decl(), freeze_thaw_tree_type(), and
  freeze_thaw_tree_special() for which nodes have some additional
  processing in the "normal" case.
  
  Note, pfe_freeze_thaw_decl() may freely use DECL_LANG_SPECIFIC.
  Similarly pfe_freeze_thaw_type() may freely use TYPE_LANG_SPECIFIC.
  The freezing and thawing of these fields should not be done by these
  routines.
*/

#include "config.h"

#ifdef PFE

#include "system.h"
#include "tree.h"
#include "c-common.h"
#include "rtl.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "langhooks.h"

#include "pfe.h"

static int debug_pfe_walk    = 0;	/* intended to be set from debugger */
static int debug_rtx_walk    = 0;
static int tree_nesting_level= -1;
static int rtx_nesting_level = -1;
#if !PFE_NEW_TREE_WALK
static int max_nesting_level = 0;
static tree max_nested_node  = NULL;
static int max_stk_size      = 0;
#endif

static void freeze_thaw_tree_common  	  PARAMS ((tree));
static void freeze_thaw_tree_decl    	  PARAMS ((tree));
static void freeze_thaw_tree_type    	  PARAMS ((tree));
static void freeze_thaw_tree_block   	  PARAMS ((tree));
static void freeze_thaw_tree_expr    	  PARAMS ((tree));
static void freeze_thaw_tree_special 	  PARAMS ((tree));
static void freeze_thaw_type_variant	  PARAMS ((tree));

static void freeze_thaw_trace_tree     	  PARAMS ((tree, tree));
static void freeze_thaw_trace_rtx	  PARAMS ((rtx));

static void freeze_thaw_struct_mem_attrs  PARAMS ((mem_attrs **));
static void freeze_thaw_bitmap_element 	  PARAMS ((bitmap_element **));
static void freeze_thaw_bitmap_head_def   PARAMS ((struct bitmap_head_def **));
static void freeze_thaw_edge_def 	  PARAMS ((struct edge_def **));
static void freeze_thaw_basic_block_def	  PARAMS ((struct basic_block_def **));

#define PFE_DEBUG 0

#if PFE_DEBUG
#define HASH_SIZE 37
struct bucket
{
  void *node;
  struct bucket *next;
};
static struct bucket **table = NULL;
static int pfe_seen_node (tree);
#endif

#if PFE_NEW_TREE_WALK
#define PFE_NEW_TREE_WALK_DISPLAY 1
extern int pfe_display_tree_walk;
#endif

/*-------------------------------------------------------------------*/

/* Trace pfe_freeze_thaw_tree_walk() calls.  */
static void freeze_thaw_trace_tree (node, chain_start)
     tree node;
     tree chain_start;
{
  tree type0, tname, dname;
  char *s;
  
  if (tree_nesting_level > 0)
    fprintf (stderr, "%*c", MIN (tree_nesting_level, 41), ' ');
  else
    fputc ('\n', stderr);
  fprintf (stderr, "%s:", tree_code_name[(int) TREE_CODE (node)]);
  fprintf (stderr, HOST_PTR_PRINTF, node);
  fprintf (stderr, " (\'%c\') [depth=%d", 
			   TREE_CODE_CLASS (TREE_CODE (node)),
			   tree_nesting_level);
  if (chain_start)
    {
      fprintf (stderr, ", on ");
      fprintf (stderr, HOST_PTR_PRINTF, chain_start);
      fprintf (stderr, " chain]");
    }  
  else
    fputc (']', stderr);
  
  #if 0
  /* pfe_freeze_ptrs and pfe_thaw_ptrs are static inside pfe.c.  But we can
     temporarily make them non-static and use this code to make sure that
     freeze/thaw operations are totally symmetric in the freeze/thaw counts.  */
  if (PFE_FREEZING)
    {
       extern int pfe_freeze_ptrs;
       fprintf (stderr, "(%d)", pfe_freeze_ptrs);
    }
  else
    {
       extern int pfe_thaw_ptrs;
       fprintf (stderr, "(%d)", pfe_thaw_ptrs);
    }
  #endif
   
  if ((type0 = RP (TREE_TYPE (node)))
      && TREE_CODE_CLASS (TREE_CODE (node)) != 't'
      && TREE_CODE_CLASS (TREE_CODE (type0)) == 't')
    {
      fprintf (stderr, " t=");
      fprintf (stderr, HOST_PTR_PRINTF, type0);
      
      if ((tname = RP (TYPE_NAME (type0))))
	{
	  if (TREE_CODE (tname) == IDENTIFIER_NODE)
	    {
	      s = RP ((char *)IDENTIFIER_POINTER (tname));
	      if (s && *s)
		fprintf (stderr, " {%s}", s);
	    }
	  else if (TREE_CODE (tname) == TYPE_DECL
		   && (dname = RP (DECL_NAME (tname)))
		   && (s = RP ((char *)IDENTIFIER_POINTER (dname)))
		   && *s)
	    fprintf (stderr, " {%s}", s);
	}
      else
	fprintf (stderr, " {%s}", tree_code_name[(int) TREE_CODE (type0)]);
    }
  else if (type0)
    {
      fprintf (stderr, " t=");
      fprintf (stderr, HOST_PTR_PRINTF, type0);
    }  
  if (TREE_CODE (node) == IDENTIFIER_NODE
      && (s = RP ((char *)IDENTIFIER_POINTER (node)))
      && *s)
    fprintf (stderr, " %s", s);
  fputc ('\n', stderr);
}

#if PFE_NEW_TREE_WALK
/* We use TREE_LANG_FLAG_7(node) to mark nodes as "currently being
   visited" or "has been visited".  For freezing we set the flag to 1
   to mark it as visited and for thawing we set the flag to 0 (since
   it will be loaded with the 1's we set when we froze it).
   
   The following macro encodes this test in terms of the pfe_operation.
   For freezing !PFE_FREEZING is always 0.  So if the visited flag is
   still 0 we haven't visited the node and the equality is true.
   Thus negation means we HAVE visitied the node.
   
   Conversly, for thawing, !PFE_FREEZING is always 1.  A node which
   hasn't been visited will still be 1 (since this is what was
   written when we froze them).  So if the visited flag is still 1
   we haven't visited the node and the equality is true.  Thus
   again negation means we HAVE visitied the node.  */
    
#define NODE_VISITED(node) !(TREE_LANG_FLAG_7 (node) == !PFE_FREEZING)

typedef tree pfe_tree_stack_entry;
static pfe_tree_stack_entry *pfe_tree_stack = NULL;
static int pfe_tree_stack_size = 0;
static int pfe_tree_stack_last = -1;
static int pfe_tree_stack_total = 0;
static int pfe_tree_stack_max = -1;
#define PFE_INITIAL_TREE_STACK_SIZE (64 * 1024)
#define PFE_INITIAL_TREE_STACK_INCR (16 * 1024)

/* Push a pointer to a tree node that needs freezing/thawing onto
   a stack of nodes still to be handled.  Make sure that the pointer
   itself is frozen/thawed.  The actual argument here is a pointer
   to the pointer to the tree node, so that the pointer can be
   modified in place.  Nodes are marked as visited when they are
   pushed onto the stack so that they will not be put in the stack
   more than once.  */
void 
pfe_freeze_thaw_tree_push (nodep)
     tree *nodep;
{
  tree node;
  
  /* Nothing to do if we did not get a pointer.  */
  if (!nodep)
    return;
  
  /* If we are freezing, there is nothing to do if we have a NULL
     pointer or if the pointer is already frozen; otherwise, we
     freeze the pointer.  If we are thawing, there is nothing to do 
     if the pointer is already thawed; otherwise, we thaw the 
     pointer.  */
  if (PFE_FREEZING)
    {
      node = *nodep;
      if (!node || PFE_IS_FROZEN (node))
        return;
      pfe_freeze_ptr (nodep);
    }
  else
    {
      if (!PFE_IS_FROZEN (*nodep))
        return;
      node = pfe_thaw_ptr (nodep);
    }
  
  /* If the node has already been marked as visited, that means that
     it already is or has been in the stack, so nothing needs to be
     done.  */
  if (!node || NODE_VISITED (node))
    return;
  
  /* If we get here we need to add the tree node pointer to the
     tree stack.  Create the stack if it doesn't exist.  Grow it
     if it needs to be larger to accomodate the new entry.  */
  if (pfe_tree_stack == NULL)
    {
      pfe_tree_stack_size = PFE_INITIAL_TREE_STACK_SIZE;
      pfe_tree_stack = (pfe_tree_stack_entry *) 
      		       xmalloc (sizeof (pfe_tree_stack_entry) 
      		       		* pfe_tree_stack_size);
    }
  
  if (++pfe_tree_stack_last >= pfe_tree_stack_size)
    {
      pfe_tree_stack_size += PFE_INITIAL_TREE_STACK_INCR;
      pfe_tree_stack = (pfe_tree_stack_entry *) 
      		       xrealloc (pfe_tree_stack,
      		       		 sizeof (pfe_tree_stack_entry) 
      		       		 * pfe_tree_stack_size);
    }
  
  pfe_tree_stack_total++;
  if (pfe_tree_stack_last > pfe_tree_stack_max)
    pfe_tree_stack_max = pfe_tree_stack_last;
    
  pfe_tree_stack[pfe_tree_stack_last] = node;
  
  /* Mark this node as "visited" so we don't try to stack it
     again.  */
  TREE_LANG_FLAG_7 (node) = PFE_FREEZING;
}

/* Process all of the tree nodes that have been pushed onto the
   pfe_tree_stack and freeze/thaw them.  We do this until the stack
   is exhausted, i.e., the walk is complete.  Processing the
   tree nodes will add new nodes to the stack if nodes that have
   not been frozen/thawed are encountered.  */
void
pfe_freeze_thaw_tree_walk (void)
{
  tree node;

#if PFE_NEW_TREE_WALK_DISPLAY
  if (pfe_display_tree_walk)
    printf ("pfe_freeze_thaw_tree_walk (entry): pfe_tree_stack_max = %d\n",
            pfe_tree_stack_max);
#endif

  while (pfe_tree_stack_last >= 0)
    {
      /* Get the last tree node pointer off the top (end) of the tree
         stack.  */
      node = pfe_tree_stack[pfe_tree_stack_last--];
      
      if (debug_pfe_walk)
	freeze_thaw_trace_tree (node, NULL);
      
      /* For thawing, we need to thaw the common tree portion first
	 because there may be need to use the TREE_TYPE in places like
	 freeze_thaw_tree_decl(), freeze_thaw_tree_type(), etc.  For
	 example, the lang specific c++ code for type decls needs
	 TREE_TYPE.  */
    
      if (PFE_THAWING)
	freeze_thaw_tree_common (node);
      
      switch (TREE_CODE_CLASS (TREE_CODE (node)))
	{
	case 'd':
	  freeze_thaw_tree_decl (node);
	  pfe_freeze_thaw_tree_push (&TREE_CHAIN (node));
	  break;
	  
	case 't':
	  freeze_thaw_tree_type (node);
	  pfe_freeze_thaw_tree_push (&TREE_CHAIN (node));
	  break;
	
	case 'b':
	  freeze_thaw_tree_block (node);
	  pfe_freeze_thaw_tree_push (&TREE_CHAIN (node));
	  break;
	
	case 'e':
	case '<':
	case '1':
	case '2':
	case 'r':
	case 's':
	  freeze_thaw_tree_expr (node);
	  pfe_freeze_thaw_tree_push (&TREE_CHAIN (node));
	  break;
	
	case 'c':
	  freeze_thaw_tree_special (node);
	  break;
	  
	case 'x':
	  freeze_thaw_tree_special (node);
	  pfe_freeze_thaw_tree_push (&TREE_CHAIN (node));
	  break;
	
	default:
	  break;
	}
      
      /* For freezing, we need to delay the common portion until here
         because there may be need to use the TREE_TYPE in places like
         freeze_thaw_tree_decl(), freeze_thaw_tree_type(), etc.  For
         example, the lang specific c++ code for type decls needs
         TREE_TYPE.  */
      
      if (PFE_FREEZING)
      	freeze_thaw_tree_common (node);
    }
    
  free (pfe_tree_stack);
  
#if PFE_NEW_TREE_WALK_DISPLAY
  if (pfe_display_tree_walk)
    printf ("pfe_freeze_thaw_tree_walk (exit): "
  	    "pfe_tree_stack_max = %d; pfe_tree_stack_total = %d\n", 
  	    pfe_tree_stack_max, pfe_tree_stack_total);
#endif
}

#else /* !PFE_NEW_TREE_WALK */

/* Freeze/thaw tree node and its direct descendents (i.e., walk tree).  */
void 
pfe_freeze_thaw_tree_walk (nodep)
     tree *nodep;
{
  tree node, chain_start;
  static int follow_chain;
  static void *start_stk_addr;
  
  /* We use TREE_LANG_FLAG_7(node) to mark nodes as "currently being
     visited" or "has been visited".  For freezing we set the flag to 1
     to mark it as visited and for thawing we set the flag to 0 (since
     it will be loaded with the 1's we set when we froze it).
     
     The following macro encodes this test in terms of the pfe_operation.
     For freezing !PFE_FREEZING is always 0.  So if the visited flag is
     still 0 we haven't visited the node and the equality is true.
     Thus negation means we HAVE visitied the node.
     
     Conversly, for thawing, !PFE_FREEZING is always 1.  A node which
     hasn't been visited will still be 1 (since this is what was
     written when we froze them).  So if the visited flag is still 1
     we haven't visited the node and the equality is true.  Thus
     again negation means we HAVE visitied the node.  */
      
  #define NODE_VISITED(node) !(TREE_LANG_FLAG_7 (node) == !PFE_FREEZING)
    
  if (!nodep)
    return;
  
  if (PFE_FREEZING)
    {
      node = *nodep;
      if (!node || PFE_IS_FROZEN (node))
        return;
      pfe_freeze_ptr (nodep);
    }
  else
    {
      if (!PFE_IS_FROZEN (*nodep))
        return;
      node = pfe_thaw_ptr (nodep);
    }
    
  if (!node || NODE_VISITED (node))
    return;
  
  #if PFE_DEBUG
  if (pfe_seen_node (node))
    {
      int stop_here;
      fprintf (stderr, "trying to freeze node %x more than once!\n", (unsigned)node);
      stop_here = 1;
    }
  #endif
    
  /* We always track nesting to for debugging purposes.  */
  
  if (++tree_nesting_level > max_nesting_level)
    {
      int stk_size = (int)start_stk_addr - (int)&node;
      if (stk_size < 0) stk_size = -stk_size;
      if (stk_size > max_stk_size)
	max_stk_size = stk_size;
      max_nesting_level = tree_nesting_level;
      max_nested_node   = node;
    }
  else if (tree_nesting_level == 0)
    start_stk_addr = &node;
      
  chain_start = node;
  if (debug_pfe_walk)
    freeze_thaw_trace_tree (node, NULL);
  
  do {
    TREE_LANG_FLAG_7 (node) = PFE_FREEZING;

    /* For thawing, we need to thaw the common tree portion first
       because there may be need to use the TREE_TYPE in places like
       freeze_thaw_tree_decl(), freeze_thaw_tree_type(), etc.  For
       example, the lang specific c++ code for type decls needs
       TREE_TYPE.  */
    
    if (PFE_THAWING)
      freeze_thaw_tree_common (node);
    
    switch (TREE_CODE_CLASS (TREE_CODE (node)))
      {
      case 'd':
	freeze_thaw_tree_decl (node);
	follow_chain = 1;
	break;
	
      case 't':
	freeze_thaw_tree_type (node);
	follow_chain = 1;
	break;
      
      case 'b':
	freeze_thaw_tree_block (node);
	follow_chain = 1;
	break;
      
      case 'e':
      case '<':
      case '1':
      case '2':
      case 'r':
      case 's':
	freeze_thaw_tree_expr (node);
	follow_chain = 1;
	break;
      
      case 'c':
	freeze_thaw_tree_special (node);
	follow_chain = 0;
	break;
	
      case 'x':
	freeze_thaw_tree_special (node);
	follow_chain = 1;
	break;
      
      default:
	follow_chain = 0;
      }

    /* For freezing, we need to delay the common portion until here
       because there may be need to use the TREE_TYPE in places like
       freeze_thaw_tree_decl(), freeze_thaw_tree_type(), etc.  For
       example, the lang specific c++ code for type decls needs
       TREE_TYPE.  */
    
    if (PFE_FREEZING)
      freeze_thaw_tree_common (node);
    
    if (follow_chain)
      {
        node = PFE_FREEZE_THAW_PTR (&TREE_CHAIN (node));
        follow_chain = node && !NODE_VISITED (node);
	if (debug_pfe_walk && follow_chain)
	  freeze_thaw_trace_tree (node, chain_start);
      }
    }
  while (follow_chain);
  
  --tree_nesting_level;
}
#endif

/* Handle tree_common portion of every node */
static void
freeze_thaw_tree_common (node)
     tree node;
{
  /* Put back all nodes in their "unwritten" state since,
     conceptually no asm or debug output has been generated
     when a file is loaded.  We save the setting in the file
     too since it doesn't pay to check for freeze or thaw.  */
  TREE_ASM_WRITTEN (node) = 0;
  PFE_FREEZE_THAW_WALK (TREE_TYPE (node));
  /* future common stuff goes here.  */
}

/* Handle 'd' nodes */
static void
freeze_thaw_tree_decl (node)
     tree node;
{
  pfe_freeze_thaw_ptr_fp (&DECL_SOURCE_FILE (node));
  PFE_FREEZE_THAW_WALK (DECL_SIZE (node));
  PFE_FREEZE_THAW_WALK (DECL_SIZE_UNIT (node));
  PFE_FREEZE_THAW_WALK (DECL_NAME (node));
  PFE_FREEZE_THAW_WALK (DECL_ARGUMENTS (node));
  PFE_FREEZE_THAW_WALK (DECL_INITIAL (node));
  PFE_FREEZE_THAW_WALK (DECL_ABSTRACT_ORIGIN (node));
  if (node->decl.assembler_name) /* no useful accessor macro */
    PFE_FREEZE_THAW_WALK ((node->decl.assembler_name));
  PFE_FREEZE_THAW_WALK (DECL_SECTION_NAME (node));	/* NEEDED? */
  PFE_FREEZE_THAW_WALK (DECL_ATTRIBUTES (node));
  PFE_FREEZE_THAW_RTX (node->decl.rtl);
  PFE_FREEZE_THAW_RTX (DECL_LIVE_RANGE_RTL (node));	/* FIXME: needed? */
  
  /* Some fields of the decl node are used to access the language
     specific portions of the node.  Thus we cannot freeeze them
     until that portion is done.  Similarly we need to thaw them
     before we use them.  */
     
  if (PFE_THAWING)
    {
      pfe_thaw_ptr (&DECL_LANG_SPECIFIC (node)); /* do this first */
      PFE_FREEZE_THAW_WALK (DECL_CONTEXT (node));
    }
    
  if (!(*lang_hooks.pfe_freeze_thaw_decl) (node))
    {
      switch (TREE_CODE (node))
	{
	  case FUNCTION_DECL:
	    pfe_freeze_thaw_function (&DECL_SAVED_INSNS (node));
      	    PFE_FREEZE_THAW_WALK (DECL_SAVED_TREE (node));
      	    PFE_FREEZE_THAW_WALK (DECL_INLINED_FNS (node));
	    PFE_FREEZE_THAW_WALK (DECL_VINDEX (node));
	    PFE_FREEZE_THAW_WALK (DECL_RESULT (node));
	    break;
	  
	  case PARM_DECL:
	    PFE_FREEZE_THAW_RTX (DECL_INCOMING_RTL (node));
	    PFE_FREEZE_THAW_WALK (DECL_ARG_TYPE_AS_WRITTEN (node));
	    break;
	  
	  case FIELD_DECL:
	    PFE_FREEZE_THAW_WALK (DECL_FIELD_BIT_OFFSET (node));
	    PFE_FREEZE_THAW_WALK (DECL_BIT_FIELD_TYPE (node));
	    PFE_FREEZE_THAW_WALK (DECL_FCONTEXT (node));
	    break;

	  case TYPE_DECL:
	    PFE_FREEZE_THAW_WALK (DECL_ORIGINAL_TYPE (node));
	    break;
	    
	  default: ;
	    /* Unless it's one of the above nodes, or unless
	       pfe_freeze_thaw_decl() handled the node,
	       we assume there is no other additional pointers
	       to freeze or thaw at this point.  */
	    break;
	}
    }
    
  if (PFE_FREEZING)
    {
      PFE_FREEZE_THAW_WALK (DECL_CONTEXT (node));
      pfe_freeze_ptr (&DECL_LANG_SPECIFIC (node)); /* do this last */
    }
}

/* Freeze/thaw a 't' node's TYPE_MAIN_VARIANT and all the nodes
   on that node's TYPE_NEXT_VARIANT chain (variants made by type
   modifiers such as "const" and "volatile").  */
static void freeze_thaw_type_variant (node)
     tree node;
{
  /* Each type has a TYPE_MAIN_VARIANT and TYPE_NEXT_VARIANT.  For the
     "main" type, TYPE_MAIN_VARIANT equals that type's node (i.e.,
     itself) and TYPE_NEXT_VARIANT points to the first variant of that
     main type.  
     
     Thus, for example, for variants of main type "int", it could have
     variants "const int", "volatile int", and "const volatile int", and
     maybe even FOO if FOO is a typedef of int.  The "int" node's
     TYPE_MAIN_VARIANT will point to itself and it's TYPE_NEXT_VARIANT
     to one of the other variants.  
     
     In each of the other variants, TYPE_MAIN_VARIANT points back to
     its main variant and TYPE_NEXT_VARIANT points to the next variant
     or is NULL.
     
     This algorithm here is "tricky" and complicated by the fact that
     current node could be the main variant or any of its next variants.
     It does not lend itself well to using the PFE_FREEZE_THAW_WALK macro
     so this is a rare instance we have to call pfe_freeze_thaw_tree_walk()
     explicitly.  */
  
  if (PFE_FREEZING)
    {
      tree *nodep = &TYPE_MAIN_VARIANT (node);
      while (*nodep && !PFE_IS_FROZEN (*nodep))
        {
          node = *nodep;
#if PFE_NEW_TREE_WALK
	  pfe_freeze_thaw_tree_push (nodep);
#else
          pfe_freeze_thaw_tree_walk (nodep);
#endif
	  PFE_FREEZE_THAW_PTR (&TYPE_MAIN_VARIANT (node));
          nodep = &TYPE_NEXT_VARIANT (node);
        }
    }
  else
    {
      tree *nodep = &TYPE_MAIN_VARIANT (node);
      while (*nodep && PFE_IS_FROZEN (*nodep))
        {
#if PFE_NEW_TREE_WALK
	  pfe_freeze_thaw_tree_push (nodep);
#else
          pfe_freeze_thaw_tree_walk (nodep);
#endif
	  PFE_FREEZE_THAW_PTR (&TYPE_MAIN_VARIANT (*nodep));
          nodep = &TYPE_NEXT_VARIANT (*nodep);
        }
    }
}

/* Handle 't' nodes */
static void
freeze_thaw_tree_type (node)
     tree node;
{
  PFE_FREEZE_THAW_WALK (TYPE_VALUES (node));
  PFE_FREEZE_THAW_WALK (TYPE_SIZE (node));
  PFE_FREEZE_THAW_WALK (TYPE_SIZE_UNIT (node));
  PFE_FREEZE_THAW_WALK (TYPE_ATTRIBUTES (node));
  PFE_FREEZE_THAW_WALK (TYPE_POINTER_TO (node));
  PFE_FREEZE_THAW_WALK (TYPE_REFERENCE_TO (node));
  
  /* FIXME: We know what to do for DBX_DEBUGGING_INFO.
     It's not clear we are correct for the others.  */
  #if defined(DBX_DEBUGGING_INFO)
  /* TYPE_SYMTAB_ADDRESS(node) used instead of TYPE_SYMTAB_POINTER().  */
  /* Always make sure this is zero when at least thawed.  This will
     cause dbxout.c to regenerate the stab info for this type.  */
  TYPE_SYMTAB_ADDRESS (node) = 0;
  #elif defined(XCOFF_DEBUGGING_INFO)
  /* TYPE_SYMTAB_ADDRESS(node) used instead of TYPE_SYMTAB_POINTER().  */
  TYPE_SYMTAB_ADDRESS (node) = 0;
  #elif defined(SDB_DEBUGGING_INFO)
  pfe_freeze_thaw_ptr_fp (&TYPE_SYMTAB_POINTER (node));
  #elif defined(DWARF_DEBUGGING_INFO)
  pfe_freeze_thaw_ptr_fp (&TYPE_SYMTAB_POINTER (node));
  #elif defined(DWARF2_DEBUGGING_INFO)
  pfe_freeze_thaw_ptr_fp (&TYPE_SYMTAB_POINTER (node));
  #endif
  
  PFE_FREEZE_THAW_WALK (TYPE_NAME (node));
  PFE_FREEZE_THAW_WALK (TYPE_MIN_VALUE (node));
  PFE_FREEZE_THAW_WALK (TYPE_MAX_VALUE (node));

  freeze_thaw_type_variant (node);
  PFE_FREEZE_THAW_WALK (TYPE_CONTEXT (node)); 		/* NEEDED? */
  
  if (PFE_THAWING)
    pfe_thaw_ptr (&TYPE_LANG_SPECIFIC (node));
    
  if (!(*lang_hooks.pfe_freeze_thaw_type) (node))
    {
      switch (TREE_CODE (node))
	{
	  case RECORD_TYPE:
	  case UNION_TYPE:
	  case QUAL_UNION_TYPE:
	    PFE_FREEZE_THAW_WALK (TYPE_METHODS (node));
	    PFE_FREEZE_THAW_WALK (TYPE_BINFO (node));
	    break;
	  
	  case FUNCTION_TYPE:
	  case METHOD_TYPE:
	    PFE_FREEZE_THAW_WALK (TYPE_METHOD_BASETYPE (node));
	    break;
	  
	  case OFFSET_TYPE:
	    PFE_FREEZE_THAW_WALK (TYPE_OFFSET_BASETYPE (node));
	    break;
	    
	  default: ;
	    /* Unless it's one of the above nodes, or unless
	       pfe_freeze_thaw_type() handled the node,
	       we assume there is no other additional pointers
	       to freeze or thaw at this point.  */
	    break;
	}
    }
    
  if (PFE_FREEZING)
    pfe_freeze_ptr (&TYPE_LANG_SPECIFIC (node));
}

/* Handle 'b' nodes */
static void
freeze_thaw_tree_block (node)
     tree node;
{
  PFE_FREEZE_THAW_WALK (BLOCK_VARS (node));
  PFE_FREEZE_THAW_WALK (BLOCK_SUBBLOCKS (node));
  PFE_FREEZE_THAW_WALK (BLOCK_SUPERCONTEXT (node));
  PFE_FREEZE_THAW_WALK (BLOCK_ABSTRACT_ORIGIN (node));
  PFE_FREEZE_THAW_WALK (BLOCK_FRAGMENT_ORIGIN (node));
  PFE_FREEZE_THAW_WALK (BLOCK_FRAGMENT_CHAIN (node));
}

/* Handle 'e', '<', '1', '2', 'r', and 's' nodes */
static void
freeze_thaw_tree_expr (node)
     tree node;
{
  int i, len, first_rtl;
  
  len = TREE_CODE_LENGTH (TREE_CODE (node));
  first_rtl = first_rtl_op (TREE_CODE (node));
  
  for (i = 0; i < len; ++i)
    {
      if (i >= first_rtl)
        {
          rtx *rtxp = (struct rtx_def **)&TREE_OPERAND (node, i);
	  pfe_freeze_thaw_rtx (rtxp); /* cannot use macro here */ /* FIXME: needed? */
	}
      else
	PFE_FREEZE_THAW_WALK (TREE_OPERAND (node, i));
    }
}

/* Handle 'c' and 'x' nodes */
static void
freeze_thaw_tree_special (node)
     tree node;
{
  int i, len;
  
  if (!(*lang_hooks.pfe_freeze_thaw_special) (node))
    {
      switch (TREE_CODE (node))
	{
	case INTEGER_CST:
	  PFE_FREEZE_THAW_RTX (TREE_CST_RTL(node)); /* FIXME: needed? */
	  break;
	
	case REAL_CST:
	  PFE_FREEZE_THAW_RTX (TREE_CST_RTL(node)); /* FIXME: needed? */
	  break;
	
	case COMPLEX_CST:
	  PFE_FREEZE_THAW_RTX (TREE_CST_RTL(node)); /* FIXME: needed? */
	  PFE_FREEZE_THAW_WALK (TREE_REALPART (node));
	  PFE_FREEZE_THAW_WALK (TREE_IMAGPART (node));
	  break;

#if 0 /* APPLE MERGE generalize this if necessary */	
	/* APPLE LOCAL: AltiVec */
	case VECTOR_CST:
	  PFE_FREEZE_THAW_RTX (TREE_CST_RTL(node)); /* FIXME: needed? */
	  PFE_FREEZE_THAW_WALK (TREE_VECTOR_CST_0 (node));
	  PFE_FREEZE_THAW_WALK (TREE_VECTOR_CST_1 (node));
	  PFE_FREEZE_THAW_WALK (TREE_VECTOR_CST_2 (node));
	  PFE_FREEZE_THAW_WALK (TREE_VECTOR_CST_3 (node));
	  break;
#endif

	case STRING_CST:
	  PFE_FREEZE_THAW_RTX (TREE_CST_RTL(node)); /* FIXME: needed? */
	  pfe_freeze_thaw_ptr_fp (&TREE_STRING_POINTER (node));
	  PFE_FREEZE_THAW_WALK (TREE_CHAIN (node));
	  break;
	
	case TREE_LIST:
	  PFE_FREEZE_THAW_WALK (TREE_PURPOSE (node));
	  PFE_FREEZE_THAW_WALK (TREE_VALUE (node));
	  break;
	
	case TREE_VEC:
	  len = TREE_VEC_LENGTH (node);
	  for (i = 0; i < len; i++)
	    PFE_FREEZE_THAW_WALK (TREE_VEC_ELT (node, i));
	  break;
	
	case IDENTIFIER_NODE:
	  /* All IDENTIFIER_NODE's contain a cpp_hashnode and
	     cpp_hashnode's are frozen/thawed as part of the
	     normal freezing and thawing of hashtables.  So
	     nothing futher needs to be done here.  */
	  break;
	  
	default: ;
	  /* Unless it's one of the above nodes, or unless
	     pfe_freeze_thaw_special() handled the node,
	     we assume there is no other additional pointers
	     to freeze or thaw at this point.  */
	  break;
	}
    }
}

/*-------------------------------------------------------------------*/

/* Trace pfe_freeze_thaw_rtx() calls.  */
static void freeze_thaw_trace_rtx (x)
     rtx x;
{
  if (rtx_nesting_level > 0)
    fprintf (stderr, "%*c", MIN (tree_nesting_level + rtx_nesting_level, 60), ' ');
    
  fprintf (stderr, "%s:", GET_RTX_NAME (GET_CODE (x)));
  fprintf (stderr, HOST_PTR_PRINTF, x);
  fprintf (stderr, " [depth=%d] codes=\"%s\"", 
			   rtx_nesting_level,
			   (char *)GET_RTX_FORMAT (GET_CODE (x)));
  #if 0
  /* pfe_freeze_ptrs and pfe_thaw_ptrs are static inside pfe.c.  But we can
     temporarily make them non-static and use this code to make sure that
     freeze/thaw operations are totally symmetric in the freeze/thaw counts.  */
  if (PFE_FREEZING)
    {
       extern int pfe_freeze_ptrs;
       fprintf (stderr, "(%d)", pfe_freeze_ptrs);
    }
  else
    {
       extern int pfe_thaw_ptrs;
       fprintf (stderr, "(%d)", pfe_thaw_ptrs);
    }
  #endif
  
  fputc ('\n', stderr);
}

/* Freeze/thaw rtx as directed by its format codes.
                        *** CAUTION/WARNING ***
   Like anything else that is frozen/thawed, the strings for XSTR and
   XTMPL rtx's must be allocated in pfe memory.  While this is taken
   care if on the compiler, care must be taken of how XSTR's are
   created in target dependent code (i.e., code in the gcc/config
   directory).  Using ggc_alloc_string() is fine.  But allocation
   any other way must involve one of the pfe allocators (e.g.,
   pfe_malloc).  Currently we have no good way to cover such cases.  */
void
pfe_freeze_thaw_rtx (xp)
     rtx *xp;
{
  rtx  x;
  int  i;
  char *format_ptr;

  if (PFE_FREEZING)
    {
      x = *xp;
      if (!x || PFE_IS_FROZEN (x))
        return;
      pfe_freeze_ptr (xp);
    }
  else
    {
      if (!PFE_IS_FROZEN (*xp))
        return;
      x = pfe_thaw_ptr (xp);
    }

  if (x == NULL_RTX)
    return;
    
  ++rtx_nesting_level;
  if (debug_rtx_walk)
    freeze_thaw_trace_rtx (x);
  
  /* Handle operands as a function of the format codes...  */

  format_ptr = (char *)GET_RTX_FORMAT (GET_CODE (x));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (x)); i++)
    switch (*format_ptr++)
      {
      case 'T':
	pfe_freeze_thaw_ptr_fp (&XTMPL (x, i));
	break;

      case 'S':
      case 's':
	pfe_freeze_thaw_ptr_fp (&XSTR (x, i));
	break;

      case '0':
	if (i == 3 && GET_CODE (x) == NOTE)
	  {
	    switch (NOTE_LINE_NUMBER (x))
	      {
	      case NOTE_INSN_EH_REGION_BEG:
	      case NOTE_INSN_EH_REGION_END:
		break;

	      case NOTE_INSN_BLOCK_BEG:
	      case NOTE_INSN_BLOCK_END:
		PFE_FREEZE_THAW_WALK (NOTE_BLOCK (x));
		break;

	      case NOTE_INSN_RANGE_BEG:
	      case NOTE_INSN_RANGE_END:
	      case NOTE_INSN_LIVE:
		PFE_FREEZE_THAW_RTX (NOTE_RANGE_INFO (x));
		break;

	      case NOTE_INSN_BASIC_BLOCK:
		freeze_thaw_basic_block_def (&NOTE_BASIC_BLOCK (x));
		break;

	      case NOTE_INSN_EXPECTED_VALUE:
		PFE_FREEZE_THAW_RTX (NOTE_EXPECTED_VALUE (x));
		break;

	      case NOTE_INSN_DELETED_LABEL:
	        pfe_freeze_thaw_ptr_fp (&NOTE_SOURCE_FILE (x));
		break;

	      default:
		if (NOTE_LINE_NUMBER (x) >= 0)
		  pfe_freeze_thaw_ptr_fp (&X0STR (x, i));
		break;
	      }
	  }
	else if (GET_CODE (x) == MEM && MEM_ATTRS (x))
	  freeze_thaw_struct_mem_attrs (&MEM_ATTRS (x));
	break;

      case 'e':
      case 'u':
	PFE_FREEZE_THAW_RTX (XEXP (x, i));
	break;

      case 'E':
      case 'V':
	pfe_freeze_thaw_rtvec (&XVEC (x, i));
	break;

      case 'b':
        freeze_thaw_bitmap_head_def (&XBITMAP (x, i));
	break;

      case 't':
	PFE_FREEZE_THAW_WALK (XTREE (x, i));
	break;

      case 'w':
      case 'i':
      case 'n':
      case '*':
	break;

      default:
	fprintf (stderr,
		 "switch format wrong in pfe_freeze_thaw_rtx(); format was: %c.\n",
		 format_ptr[-1]);
	abort ();
      }
      
  --rtx_nesting_level;
}

/* Freeze/thaw rtvec an rtvec.  */
void
pfe_freeze_thaw_rtvec (rtvecp)
     struct rtvec_def **rtvecp;
{
  struct rtvec_def *v = PFE_FREEZE_THAW_PTR (rtvecp);
  int i;
  
  if (!v)
    return;
    
  for (i = 0; i < GET_NUM_ELEM (v); ++i)
    {
      if (debug_rtx_walk)
        {
          ++rtx_nesting_level;
          fprintf (stderr, "%*c[%d]\n", 
                   MIN (tree_nesting_level + rtx_nesting_level, 61), ' ', i);
        }
        
      PFE_FREEZE_THAW_RTX (RTVEC_ELT (v, i));
      
      if (debug_rtx_walk)
        --rtx_nesting_level;
    }
}

/* Freeze/thaw a mem_attrs struct for a MEM rtx.  */
static void
freeze_thaw_struct_mem_attrs (mpp)
     mem_attrs **mpp;
{
  mem_attrs *mp = (mem_attrs *)PFE_FREEZE_THAW_PTR (mpp);
 
  if (!mp)
    return;
  PFE_FREEZE_THAW_WALK (mp->expr);
  PFE_FREEZE_THAW_RTX (mp->offset);
  PFE_FREEZE_THAW_RTX (mp->size);
}

/*-------------------------------------------------------------------*/

/* Freeze/thaw the chain of elts pointed to by *bep.  The elt pointed
   to by bep is frozen/thawed along with all elts that follow and
   preceed that elt on the chain of bitmap_element's.  */
static void 
freeze_thaw_bitmap_element (bep)
     bitmap_element **bep;
{
  bitmap_element *be, *next, *prev;
    
  be = (bitmap_element *)PFE_FREEZE_THAW_PTR (bep);
  if (!be)
    return;
    
  next = (bitmap_element *)PFE_FREEZE_THAW_PTR (&be->next);
  prev = (bitmap_element *)PFE_FREEZE_THAW_PTR (&be->prev);
  
  be = next;
  while (be)
    {
      PFE_FREEZE_THAW_PTR (&be->prev);
      be = (bitmap_element *)PFE_FREEZE_THAW_PTR (&be->next);
    }
  
  be = prev;  
  while (be)
    {
      PFE_FREEZE_THAW_PTR (&be->next);
      be = (bitmap_element *)PFE_FREEZE_THAW_PTR (&be->prev);
    }
}

/* Freeze/thaw the chain of bitmap_head_def pointed to by *bhp.  The
   entry pointed to by (bhp) is frozen/thawed along with all the
   entries linked to it in its succ_next and pred_next fields.  */
static void 
freeze_thaw_bitmap_head_def (bhp)
     struct bitmap_head_def **bhp;
{
  bitmap_head *bh = (bitmap_head *)PFE_FREEZE_THAW_PTR (bhp);
  
  if (!bh)
    return;
  
  PFE_FREEZE_THAW_PTR (&bh->current);
  freeze_thaw_bitmap_element (&bh->first);
}

/* Freeze/thaw a flow "edge".  */
static void 
freeze_thaw_edge_def (edgep)
     struct edge_def **edgep;
{
  edge e, succ_next, pred_next;
  
  /* Freeze/thaw caller's pointer.  */
  
  e = (edge)PFE_FREEZE_THAW_PTR (edgep);
  if (!e)
    return;
  
  freeze_thaw_basic_block_def (&e->src); 	/* FIXME: is this recursive? */
  freeze_thaw_basic_block_def (&e->dest); 	/* FIXME: is this recursive? */
  PFE_FREEZE_THAW_RTX (e->insns);
  succ_next = (edge)PFE_FREEZE_THAW_PTR (&e->succ_next);
  pred_next = (edge)PFE_FREEZE_THAW_PTR (&e->pred_next);
  
  e = succ_next;
  while (e)
    {
      freeze_thaw_basic_block_def (&e->src);
      freeze_thaw_basic_block_def (&e->dest);
      PFE_FREEZE_THAW_RTX (e->insns);
      PFE_FREEZE_THAW_PTR (&e->pred_next);
      e = (edge)PFE_FREEZE_THAW_PTR (&e->succ_next);
    }
  
  e = pred_next;  
  while (e)
    {
      freeze_thaw_basic_block_def (&e->src);
      freeze_thaw_basic_block_def (&e->dest);
      PFE_FREEZE_THAW_RTX (e->insns);
      PFE_FREEZE_THAW_PTR (&e->succ_next);
      e = (edge)PFE_FREEZE_THAW_PTR (&e->pred_next);
    }
}

/* Freeze/thaw a basic_block_def.  */
static void 
freeze_thaw_basic_block_def (bbp)
     struct basic_block_def **bbp;
{
  struct basic_block_def *bb = (struct basic_block_def *)PFE_FREEZE_THAW_PTR (bbp);
  
  if (!bb)
    return;
    
  PFE_FREEZE_THAW_RTX (bb->head);
  PFE_FREEZE_THAW_RTX (bb->end);
  PFE_FREEZE_THAW_WALK (bb->head_tree);
  PFE_FREEZE_THAW_WALK (bb->end_tree);
  freeze_thaw_edge_def (&bb->pred);
  freeze_thaw_edge_def (&bb->succ);
  freeze_thaw_bitmap_head_def (&bb->local_set);
  freeze_thaw_bitmap_head_def (&bb->cond_local_set);
  freeze_thaw_bitmap_head_def (&bb->global_live_at_start);
  freeze_thaw_bitmap_head_def (&bb->global_live_at_end);
}

/*-------------------------------------------------------------------*/

void pfe_print_TREE_CHAIN (tree);

void
pfe_print_TREE_CHAIN (node)
     tree node;
{
  tree node1;
  char *name;
  
  for (node1 = node, node = RP(node); node; node1 = TREE_CHAIN (node), node = RP(TREE_CHAIN (node)))
    {
      name = (char *)"?";
      if (TREE_CODE_CLASS (TREE_CODE (node)) != 't'
	  && TREE_TYPE (node)
	  && TREE_CODE_CLASS (TREE_CODE (TREE_TYPE (node))) == 't')
	{
	  tree type = TREE_TYPE (node);
	  
	  if (TYPE_NAME (type))
	    {
	      if (TREE_CODE (TYPE_NAME (type)) == IDENTIFIER_NODE)
	        {
	          if (IDENTIFIER_POINTER (TYPE_NAME (type))
	              && *IDENTIFIER_POINTER (TYPE_NAME (type)))
		  name = (char *)IDENTIFIER_POINTER (TYPE_NAME (type));
		}
	      else if (TREE_CODE (TYPE_NAME (type)) == TYPE_DECL
		       && DECL_NAME (TYPE_NAME (type))
		       && IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)))
		       && *IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type))))
		name  = (char *)IDENTIFIER_POINTER (DECL_NAME (TYPE_NAME (type)));
	    }
	  else
	    name = (char *)tree_code_name[(int) TREE_CODE (type)];
	}
      
      if (PFE_FREEZING)
        {
          if (!PFE_IS_FROZEN (node1))
            {
              fprintf (stderr, "0x%x (%s) is not frozen", (unsigned)node, name);
	      if (TREE_LANG_FLAG_7 (node) != 1)
		fprintf (stderr, "  and doesn't have TREE_LANG_FLAG_7 set to 1");
	      fprintf (stderr, "\n");
            }
          else if (TREE_LANG_FLAG_7 (node) != 1)
	    fprintf (stderr, "0x%x (%s) doesn't have TREE_LANG_FLAG_7 set to 1\n", (unsigned)node, name);
        }
      else
        {
          if (PFE_IS_FROZEN (node1))
            {
              fprintf (stderr, "0x%x (%s) is frozen", (unsigned)node, name);
	      if (TREE_LANG_FLAG_7 (node) != 0)
		fprintf (stderr, "  and doesn't have TREE_LANG_FLAG_7 set to 0");
	      fprintf (stderr, "\n");
            }
          else if (TREE_LANG_FLAG_7 (node) != 0)
	    fprintf (stderr, "0x%x  (%s) doesn't have TREE_LANG_FLAG_7 set to 0\n", (unsigned)node, name);
        }
    }
}

#if PFE_DEBUG
static int 
pfe_seen_node (node)
     tree node;
{
  int hash;
  struct bucket *b;
  
  if (!table)
    {
      table = (struct bucket **) xmalloc (HASH_SIZE * sizeof (struct bucket *));
      memset ((char *) table, 0, HASH_SIZE * sizeof (struct bucket *));
    }
  
  hash = ((unsigned long) node >> 4) % HASH_SIZE;

  for (b = table[hash]; b; b = b->next)
    if (b->node == node)
	return 1;
  
  if (tree_nesting_level >= 0)
    {
      b = (struct bucket *) xmalloc (sizeof (struct bucket));
      b->node = node;
      b->next = table[hash];
      table[hash] = b;
    }
    
  return 0;
}

static void analyze_hash()
{
  int i, n, t;
  struct bucket *b;
  
  for (i = t = 0; i < HASH_SIZE; ++ i)
    {
      for (n = 0, b = table[i]; b; b = b->next, ++n) ;
      fprintf (stderr, "%4d: %6d\n", i, n);
      t += n;
    }
  fprintf (stderr, "total number of hash entries = %d\n", t);
}
#endif

#endif /* PFE */

/*-------------------------------------------------------------------*/

#if 0

cd $gcc3/gcc/pfe; \
cc -no-cpp-precomp -c  -DIN_GCC  -g \
  -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wtraditional -pedantic -Wno-long-long \
  -DHAVE_CONFIG_H \
  -I$gcc3obj \
  -I. \
  -I.. \
  -I../config \
  -I../../include \
  freeze-thaw.c -o ~/tmp.o -w

#endif
