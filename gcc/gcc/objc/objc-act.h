/* Declarations for objc-act.c.
   Copyright (C) 1990 Free Software Foundation, Inc.

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


/*** Public Interface (procedures) ***/

/* used by yyparse */

void finish_file				PROTO((void));
tree start_class				PROTO((enum tree_code, tree, tree, tree, unsigned));
tree continue_class				PROTO((tree));
void finish_class				PROTO((tree));
void start_method_def				PROTO((tree));
void continue_method_def			PROTO((void));
void finish_method_def				PROTO((void));
tree start_protocol				PROTO((enum tree_code, tree, tree));
void finish_protocol				PROTO((tree));
void add_objc_decls				PROTO((void));

tree is_ivar					PROTO((tree, tree));
int is_private					PROTO((tree));
int is_public					PROTO((tree, tree));
#ifdef MODERN_OBJC_SYNTAX
int looks_like_ansistyle_message_ref		PROTO((tree, tree));
int looks_like_ansistyle_method_decl		PROTO((tree, tree));
#endif
tree add_instance_variable			PROTO((tree, int, tree, tree, tree));
tree add_class_method				PROTO((tree, tree));
tree add_instance_method			PROTO((tree, tree));
tree get_super_receiver				PROTO((void));
tree get_class_ivars				PROTO((tree));
tree get_class_reference			PROTO((tree));
tree get_static_reference			PROTO((tree, tree));
tree get_object_reference			PROTO((tree));
#ifdef OBJCPLUS
tree build_message_expr				PROTO((tree));
#else
tree build_message_expr				PROTO((tree, int));
#endif
#ifdef MODERN_OBJC_SYNTAX
tree build_modern_message_expression	PROTO((tree, tree));
#endif
tree build_selector_expr			PROTO((tree));
tree build_ivar_reference			PROTO((tree));
tree build_keyword_decl				PROTO((tree, tree, tree));
tree build_method_decl				PROTO((enum tree_code, tree, tree, tree, tree));
#ifdef MODERN_OBJC_SYNTAX
tree build_method_decl_from_ansiproto		PROTO((tree, tree, tree));
#endif
tree build_protocol_expr			PROTO((tree));
tree build_objc_string_object			PROTO((tree));
tree lookup_interface				PROTO((tree));
tree lookup_name_type				PROTO((tree));

extern tree objc_ivar_chain;
extern tree objc_method_context;

void objc_declare_alias				PROTO((tree, tree));
void objc_declare_class				PROTO((tree));
#ifdef MODERN_OBJC_SYNTAX
void objc_selector_alias_pragma			PROTO((tree, tree));
#endif

extern int objc_receiver_context;

/* the following routines are used to implement statically typed objects */

int objc_comptypes				PROTO((tree, tree, int));
void objc_check_decl				PROTO((tree));

/* NeXT extensions */

tree build_encode_expr				PROTO((tree));

/* Objective-C structures */

/* KEYWORD_DECL */
#define KEYWORD_KEY_NAME(DECL) ((DECL)->decl.name)
#define KEYWORD_ARG_NAME(DECL) ((DECL)->decl.arguments)

/* INSTANCE_METHOD_DECL, CLASS_METHOD_DECL */
#define METHOD_SEL_NAME(DECL) ((DECL)->decl.name)
#define METHOD_SEL_ARGS(DECL) ((DECL)->decl.arguments)
#define METHOD_ADD_ARGS(DECL) ((DECL)->decl.result)
#define METHOD_DEFINITION(DECL) ((DECL)->decl.initial)
#define METHOD_ENCODING(DECL) ((DECL)->decl.context)
#ifdef MODERN_OBJC_SYNTAX
#define METHOD_ALIAS(DECL) ((DECL)->decl.assembler_name)
#endif

#if defined(NEXT_SEMANTICS) || defined(NEXT_PDO)
#define DIRECT_METHOD_FLAG(DECL) DECL_LANG_FLAG_2(DECL)
#define STATIC_METHOD_FLAG(DECL) DECL_LANG_FLAG_3(DECL)
#define INLINE_METHOD_FLAG(DECL) DECL_LANG_FLAG_4(DECL)
#ifdef MODERN_OBJC_SYNTAX
#define ANSI_STYLE_METHOD_FLAG(DECL) DECL_LANG_FLAG_5(DECL)
#endif
#define METHOD_ORIGIN(DECL) ((DECL)->decl.abstract_origin)
#endif /* NEXT_SEMANTICS */

/* CLASS_INTERFACE_TYPE, CLASS_IMPLEMENTATION_TYPE,
   CATEGORY_INTERFACE_TYPE, CATEGORY_IMPLEMENTATION_TYPE,
   PROTOCOL_INTERFACE_TYPE */
#define CLASS_NAME(CLASS) ((CLASS)->type.name)
#define CLASS_SUPER_NAME(CLASS) ((CLASS)->type.context)
#define CLASS_IVARS(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 0)
#define CLASS_RAW_IVARS(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 1)
#define CLASS_NST_METHODS(CLASS) ((CLASS)->type.minval)
#define CLASS_CLS_METHODS(CLASS) ((CLASS)->type.maxval)
#define CLASS_STATIC_TEMPLATE(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 2)
#define CLASS_CATEGORY_LIST(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 3)
#define CLASS_PROTOCOL_LIST(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 4)
#define PROTOCOL_NAME(CLASS) ((CLASS)->type.name)
#define PROTOCOL_LIST(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 0)
#define PROTOCOL_NST_METHODS(CLASS) ((CLASS)->type.minval)
#define PROTOCOL_CLS_METHODS(CLASS) ((CLASS)->type.maxval)
#define PROTOCOL_FORWARD_DECL(CLASS) TREE_VEC_ELT (TYPE_BINFO (CLASS), 1)
#define PROTOCOL_DEFINED(CLASS) TREE_USED (CLASS)
#define TYPE_PROTOCOL_LIST(TYPE) ((TYPE)->type.context)

/* Define the Objective-C or Objective-C++ language-specific tree codes.  */

/* this requires tree.h and if C++ also cp-tree.h to be included already. */

#define OBJC_TREE_CODES
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) SYM,
enum objc_tree_code {
  __OBJC_DUMMY =
#ifdef OBJCPLUS
    LAST_CPLUS_TREE_CODE,
#else
    LAST_AND_UNUSED_TREE_CODE,
#endif
#include "objc-tree.def"
  LAST_OBJC_TREE_CODE
};
#undef DEFTREECODE


