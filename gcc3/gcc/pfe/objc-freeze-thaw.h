/* APPLE LOCAL PFE */
/* Freeze/thaw ObjC/ObjC++-specific trees and other data.
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

#ifndef GCC_PFE_OBJC_HEADER_H
#define GCC_PFE_OBJC_HEADER_H

#ifdef PFE

/* The ObjC tree definitions are piggybacked onto C; the ObjC++
   are piggybacked onto C++ (though are othewise identical).  */

#define pfe_lang_compiler_state pfe_base_lang_compiler_state   

#ifdef OBJCPLUS
#include "cp-freeze-thaw.h"
#else
#include "c-freeze-thaw.h"
#endif

#undef pfe_lang_compiler_state

#include "objc/objc-act.h"
#include "varray.h"

struct pfe_lang_compiler_state {
  /* Language-specific state for the base language (C or C++).  */
  struct pfe_base_lang_compiler_state *base_lang;

  /* Global trees from objc/objc-act.h  */ 
  tree objc_global_trees[OCTI_MAX];
  /* Hash tables to manage the global pool of method prototypes.  */
  hash *nst_method_hash_list;
  hash *cls_method_hash_list;
  int selector_ref_idx;
  int class_names_idx;
  int meth_var_names_idx;
  int meth_var_types_idx;
  int class_ref_idx;
};

/* Language hooks (in objc-freeze-thaw.c).  */
extern void objc_pfe_lang_init	 	    PARAMS ((int));
extern void objc_freeze_thaw_compiler_state PARAMS ((struct pfe_lang_compiler_state **));
extern int objc_pfe_freeze_thaw_decl        PARAMS ((union tree_node *));
extern int objc_pfe_freeze_thaw_type        PARAMS ((union tree_node *));
extern int objc_pfe_freeze_thaw_special	    PARAMS ((union tree_node *));
extern void objc_pfe_check_all_struct_sizes PARAMS ((void));
extern void objc_pfe_check_settings         PARAMS ((struct pfe_lang_compiler_state *));

/* objc/objc-act.c */
extern void pfe_freeze_thaw_objc_act_globals PARAMS ((struct pfe_lang_compiler_state *));

/* Macros.  */
#define PFE_FREEZE_THAW_GLOBAL_HASH_ENTRY(g) \
  do { \
    PFE_GLOBAL_TO_HDR_IF_FREEZING (g); \
    freeze_thaw_hash_entry (&hdr->g); \
    PFE_HDR_TO_GLOBAL_IF_THAWING (g); \
  } while (0)


#endif /* PFE */
#endif /* ! GCC_PFE_OBJC_HEADER_H */

