/* APPLE LOCAL file Objective-C++ */
/* Language-specific hook definitions for C++ front end.
   Copyright (C) 1991, 1995, 1997, 1998,
   1999, 2000, 2001 Free Software Foundation, Inc.

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

/* The following hooks are used by the C++ front-end; Objective-C++ redefines
   a couple of these.  */

#undef LANG_HOOKS_NAME
#define LANG_HOOKS_NAME "GNU C++"
#undef LANG_HOOKS_INIT
#define LANG_HOOKS_INIT cxx_init
#undef LANG_HOOKS_FINISH
#define LANG_HOOKS_FINISH cxx_finish
/* APPLE LOCAL Objective-C++ begin */
#undef LANG_HOOKS_FINISH_FILE
#define LANG_HOOKS_FINISH_FILE cxx_finish_file
/* APPLE LOCAL Objective-C++ end */
#undef LANG_HOOKS_CLEAR_BINDING_STACK
#define LANG_HOOKS_CLEAR_BINDING_STACK pop_everything
#undef LANG_HOOKS_INIT_OPTIONS
#define LANG_HOOKS_INIT_OPTIONS lang_init_options
#undef LANG_HOOKS_DECODE_OPTION
#define LANG_HOOKS_DECODE_OPTION cxx_decode_option
#undef LANG_HOOKS_POST_OPTIONS
#define LANG_HOOKS_POST_OPTIONS cxx_post_options
#undef LANG_HOOKS_GET_ALIAS_SET
#define LANG_HOOKS_GET_ALIAS_SET cxx_get_alias_set
#undef LANG_HOOKS_EXPAND_CONSTANT
#define LANG_HOOKS_EXPAND_CONSTANT cplus_expand_constant
#undef LANG_HOOKS_SAFE_FROM_P
#define LANG_HOOKS_SAFE_FROM_P c_safe_from_p
#undef LANG_HOOKS_PRINT_STATISTICS
#define LANG_HOOKS_PRINT_STATISTICS cxx_print_statistics
#undef LANG_HOOKS_PRINT_XNODE
#define LANG_HOOKS_PRINT_XNODE cxx_print_xnode
#undef LANG_HOOKS_PRINT_DECL
#define LANG_HOOKS_PRINT_DECL cxx_print_decl
#undef LANG_HOOKS_PRINT_TYPE
#define LANG_HOOKS_PRINT_TYPE cxx_print_type
#undef LANG_HOOKS_PRINT_IDENTIFIER
#define LANG_HOOKS_PRINT_IDENTIFIER cxx_print_identifier
#undef LANG_HOOKS_SET_YYDEBUG
#define LANG_HOOKS_SET_YYDEBUG cxx_set_yydebug
#/* APPLE LOCAL new tree dump */
#undef LANG_HOOKS_DUMP_DECL
#define LANG_HOOKS_DUMP_DECL cxx_dump_decl
#undef LANG_HOOKS_DUMP_TYPE
#define LANG_HOOKS_DUMP_TYPE cxx_dump_type
#undef LANG_HOOKS_DUMP_IDENTIFIER
#define LANG_HOOKS_DUMP_IDENTIFIER cxx_dump_identifier
#undef LANG_HOOKS_DUMP_BLANK_LINE_P
#define LANG_HOOKS_DUMP_BLANK_LINE_P cxx_dump_blank_line_p
#undef LANG_HOOKS_DUMP_LINENO_P
#define LANG_HOOKS_DUMP_LINENO_P cxx_dump_lineno_p
#undef LANG_HOOKS_DMP_TREE3
#define LANG_HOOKS_DMP_TREE3 cxx_dmp_tree3
/* APPLE LOCAL end new tree dump */
/* APPLE LOCAL PFE */
#ifdef PFE
#undef LANG_HOOKS_PFE_LANG_INIT
#define LANG_HOOKS_PFE_LANG_INIT cxx_pfe_lang_init
#undef LANG_HOOKS_PFE_FREEZE_THAW_COMPILER_STATE
#define LANG_HOOKS_PFE_FREEZE_THAW_COMPILER_STATE cxx_freeze_thaw_compiler_state
#undef LANG_HOOKS_PFE_FREEZE_THAW_DECL
#define LANG_HOOKS_PFE_FREEZE_THAW_DECL	cxx_pfe_freeze_thaw_decl
#undef LANG_HOOKS_PFE_FREEZE_THAW_TYPE
#define LANG_HOOKS_PFE_FREEZE_THAW_TYPE	cxx_pfe_freeze_thaw_type
#undef LANG_HOOKS_PFE_FREEZE_THAW_SPECIAL
#define LANG_HOOKS_PFE_FREEZE_THAW_SPECIAL cxx_pfe_freeze_thaw_special
#undef LANG_HOOKS_PFE_CHECK_ALL_STRUCT_SIZES
#define LANG_HOOKS_PFE_CHECK_ALL_STRUCT_SIZES cxx_pfe_check_all_struct_sizes
#undef LANG_HOOKS_PFE_CHECK_SETTINGS
#define LANG_HOOKS_PFE_CHECK_SETTINGS cxx_pfe_check_settings
#endif
/* APPLE LOCAL end PFE */

#undef LANG_HOOKS_TREE_INLINING_WALK_SUBTREES
#define LANG_HOOKS_TREE_INLINING_WALK_SUBTREES \
  cp_walk_subtrees
#undef LANG_HOOKS_TREE_INLINING_CANNOT_INLINE_TREE_FN
#define LANG_HOOKS_TREE_INLINING_CANNOT_INLINE_TREE_FN \
  cp_cannot_inline_tree_fn
#undef LANG_HOOKS_TREE_INLINING_ADD_PENDING_FN_DECLS
#define LANG_HOOKS_TREE_INLINING_ADD_PENDING_FN_DECLS \
  cp_add_pending_fn_decls
#undef LANG_HOOKS_TREE_INLINING_TREE_CHAIN_MATTERS_P
#define LANG_HOOKS_TREE_INLINING_TREE_CHAIN_MATTERS_P \
  cp_is_overload_p
#undef LANG_HOOKS_TREE_INLINING_AUTO_VAR_IN_FN_P
#define LANG_HOOKS_TREE_INLINING_AUTO_VAR_IN_FN_P \
  cp_auto_var_in_fn_p
#undef LANG_HOOKS_TREE_INLINING_COPY_RES_DECL_FOR_INLINING
#define LANG_HOOKS_TREE_INLINING_COPY_RES_DECL_FOR_INLINING \
  cp_copy_res_decl_for_inlining
#undef LANG_HOOKS_TREE_INLINING_ANON_AGGR_TYPE_P
#define LANG_HOOKS_TREE_INLINING_ANON_AGGR_TYPE_P anon_aggr_type_p
#undef LANG_HOOKS_TREE_INLINING_START_INLINING
#define LANG_HOOKS_TREE_INLINING_START_INLINING cp_start_inlining
#undef LANG_HOOKS_TREE_INLINING_END_INLINING
#define LANG_HOOKS_TREE_INLINING_END_INLINING cp_end_inlining
#undef LANG_HOOKS_TREE_DUMP_DUMP_TREE_FN
#define LANG_HOOKS_TREE_DUMP_DUMP_TREE_FN cp_dump_tree
#undef LANG_HOOKS_TREE_DUMP_TYPE_QUALS_FN
#define LANG_HOOKS_TREE_DUMP_TYPE_QUALS_FN cp_type_quals
