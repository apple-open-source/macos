/* APPLE LOCAL PFE */
/* Freeze/thaw C-specific trees and other data.
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

#ifndef GCC_PFE_C_HEADER_H
#define GCC_PFE_C_HEADER_H

#ifdef PFE

#include "tree.h"
#include "c-common.h"
#include "varray.h"

#include "c-tree.h"

struct pfe_lang_compiler_state {
  /* globals from c-objc-common.c.  */
  varray_type deferred_fns;
};

/* In c-lang.c */
extern void pfe_freeze_thaw_c_lang_globals PARAMS ((struct pfe_lang_compiler_state *));

/* Language hooks (in c-freeze-thaw.c).  */
extern void c_pfe_lang_init	         PARAMS ((int));
extern void c_freeze_thaw_compiler_state PARAMS ((struct pfe_lang_compiler_state **));
extern int c_pfe_freeze_thaw_decl        PARAMS ((union tree_node *));
extern int c_pfe_freeze_thaw_type        PARAMS ((union tree_node *));
extern int c_pfe_freeze_thaw_special     PARAMS ((union tree_node *));
extern void c_pfe_check_all_struct_sizes PARAMS ((void));
extern void c_pfe_check_settings 	 PARAMS ((struct pfe_lang_compiler_state *));

#endif /* PFE */
#endif /* ! GCC_PFE_C_HEADER_H */

