/* APPLE LOCAL PFE */
/* Freeze/thaw C++-specific trees and other data.
   Copyright (C) 2001  Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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

#ifndef GCC_PFE_CP_HEADER_H
#define GCC_PFE_CP_HEADER_H

#ifdef PFE

#include "tree.h"
#include "c-common.h"
#include "varray.h"

#include "cp/cp-tree.h"
#include "cp/lex.h"

struct pfe_lang_compiler_state {
  /* global trees from cp/decl.h.  */
  tree cp_global_trees[CPTI_MAX]; 

  /* global name space from cp/decl2.c.  */
  tree global_namespace;

  /* globals from cp/decl2.c.  */
  varray_type deferred_fns;
  varray_type pending_statics;

  /* globals from cp/decl2.c.  */
  tree static_ctors;
  tree static_dtors; 
  int flag_apple_kext;

  /* globals from cp/class.c.  */
  varray_type local_classes;  /* Needed ? */

  /* static globals from cp/decl.c.  */
  tree global_type_node;
  int only_namespace_names;
  int pfe_anon_cnt;

  /* globals from cp/decl.c.  
     integer_zero_node and integer_one_node are part of global_trees array.  */
  tree integer_two_node;
  tree integer_three_node;
  int function_depth;
  struct saved_scope *scope_chain;
  
  /* static globals from cp/pt.c.  */
  tree pending_templates;
  tree last_pending_template;
  int template_header_count;
  tree saved_trees;
  varray_type inline_parm_levels;
  size_t inline_parm_levels_used;
  tree current_tinst_level;

  /* globals from cp/pt.c.  */
  int processing_template_parmlist;
  
  /* globals from cp/lex.c.  */
  operator_name_info_t operator_name_info[(int) LAST_CPLUS_TREE_CODE];
  operator_name_info_t assignment_operator_name_info[(int) LAST_CPLUS_TREE_CODE];
};

/* Freeze/thaw the struct pfe_lang_compiler_state.  */
void cp_lang_freeze_thaw_compiler_state   PARAMS ((struct pfe_lang_compiler_state **));

/* In cp/decl2.c */
extern void pfe_freeze_thaw_decl2_globals PARAMS ((struct pfe_lang_compiler_state *));

/* In cp/decl.c */
extern void pfe_freeze_thaw_decl_globals PARAMS ((struct pfe_lang_compiler_state *));
extern int pfe_get_anon_cnt 		 PARAMS ((void));
extern void pfe_set_anon_cnt 		 PARAMS ((int));

/* In cp/pt.c */
extern void pfe_freeze_thaw_pt_globals 	 PARAMS (( struct pfe_lang_compiler_state *));

/* For scope_chain global from cp/decl.c. saved_scope is in cp/cp-tree.h.  */
extern void pfe_freeze_thaw_saved_scope  PARAMS ((struct saved_scope **pp));

extern void pfe_freeze_thaw_operator_name_info PARAMS ((struct operator_name_info_t *p));

/* Language hooks (in cp-freeze-thaw.c).  */
extern void cxx_pfe_lang_init	           PARAMS ((int));
extern void cxx_freeze_thaw_compiler_state PARAMS ((struct pfe_lang_compiler_state **));
extern int cxx_pfe_freeze_thaw_decl        PARAMS ((union tree_node *));
extern int cxx_pfe_freeze_thaw_type        PARAMS ((union tree_node *));
extern int cxx_pfe_freeze_thaw_special     PARAMS ((union tree_node *));
extern void cxx_pfe_check_all_struct_sizes PARAMS ((void));
extern void cxx_pfe_check_settings         PARAMS ((struct pfe_lang_compiler_state *));

#endif /* PFE */ 
#endif /* ! GCC_PFE_CP_HEADER_H */

