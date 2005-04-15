/* APPLE LOCAL new tree dump */
/* Common condenced tree display routine definitions.
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

#ifndef GCC_DMP_TREE_H
#define GCC_DMP_TREE_H

/* Language-specific initialization  */
typedef int (*lang_dump_tree_p_t)  PARAMS ((FILE *, const char *, tree, int));
extern lang_dump_tree_p_t set_dump_tree_p PARAMS ((lang_dump_tree_p_t));

/* Main entry  */
extern void dmp_tree		   PARAMS ((tree));
extern void dmp_tree1		   PARAMS ((tree, int));
extern void dmp_tree2		   PARAMS ((tree));
extern void dmp_tree3		   PARAMS ((FILE *, tree, int));

/* Recursive entry  */
extern void dump_tree 		   PARAMS ((FILE *, const char *, tree, int));

/* Special purpose node routines  */
extern int node_seen		   PARAMS ((tree, int));
extern void newline_and_indent	   PARAMS ((FILE *, int));
extern void print_type 		   PARAMS ((FILE *, const char *, tree, int));
extern void print_decl 		   PARAMS ((FILE *, const char *, tree, int));
extern void print_ref 		   PARAMS ((FILE *, const char *, tree, int));
extern void print_operands	   PARAMS ((FILE *file, tree, int, int, ...));
extern void print_lineno	   PARAMS ((FILE *, tree));
extern void print_integer_constant PARAMS ((FILE *, tree, int));
extern void print_real_constant    PARAMS ((FILE *, tree));
extern void print_string_constant  PARAMS ((FILE *, const char *, int));
extern void print_tree_flags	   PARAMS ((FILE *, tree));

/* State switches for dmp_tree() to tell it how to record and handle
   previously visited nodes.  */
enum dmp_tree_visit_state {
  DMP_TREE_VISIT_ANY,			/* allow display of any node anytime */
  DMP_TREE_VISIT_ONCE,			/* only display once per dmp_tree()  */
  DMP_TREE_VISIT_ONCE1,			/* only once, but need to init hash  */
  DMP_TREE_VISIT_ONCE2			/* only once, but do not clear hash  */
};

typedef struct {		  	/* dmp_tree.c state switches... */
  int  max_code; 			/* max_node_code must be 1st    */
  int  nesting_depth;
  int  dump_full_type;
  int  really_follow;
  int  doing_parm_decl; 
  int  doing_call_expr; 
  char *curr_file;
  int  no_new_line;
  int  line_cnt;
  int  doing_tree_list;
  int  max_depth;
  enum dmp_tree_visit_state visit_only_once;
} dump_tree_state_t;

extern dump_tree_state_t dump_tree_state;

#define SET_MAX_DMP_TREE_CODE(code) \
  dump_tree_state.max_code = MAX(dump_tree_state.max_code, (int)(code))

/*-------------------------------------------------------------------*/

/* DMP_TREE is ONLY defined by the actual tree dumping code to cause 
   some common definitions that they specifically use.  */
   
#ifdef DMP_TREE

/* The DMP_TREE_WRAPPED_OUTPUT switch is a master contol on wheter we
   actually use these routines.  */
#define DMP_TREE_WRAPPED_OUTPUT 1

#if DMP_TREE_WRAPPED_OUTPUT

/* The following redefines fprintf, fputs, fputc as calls to our routines
   which handle line wrapping of long node line displays.  It is assumed
   that this header is the last #include in the tree dump file's include
   list and that DMP_TREE is defined by those files (e.g., dmp-tree.c) 
   that which to use these output routines.  */
   
extern int dmp_tree_fprintf	   PARAMS ((FILE *, const char *, ...));
extern int dmp_tree_fputc	   PARAMS((int, FILE *));
extern int dmp_tree_fputs	   PARAMS((const char *, FILE *));

#define fprintf dmp_tree_fprintf
#define fputc dmp_tree_fputc
#define fputs dmp_tree_fputs
#endif /* DMP_TREE_WRAPPED_OUTPUT */

#define HOST_PTR_PRINTF_VALUE(p) (char *) (p)

#define INDENT 1		   /* controls nesting tab value */

#endif /* DMP_TREE */


#endif /* GCC_DMP_TREE_H */
