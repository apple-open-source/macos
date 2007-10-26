/* Stub functions for Objective-C and Objective-C++ routines
   that are called from within the C and C++ front-ends,
   respectively.
   Copyright (C) 1991, 1995, 1997, 1998,
   1999, 2000, 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include "c-common.h"

tree
objc_is_class_name (tree ARG_UNUSED (arg))
{
  return 0;
}

tree
objc_is_id (tree ARG_UNUSED (arg))
{
  return 0;
}

tree
objc_is_object_ptr (tree ARG_UNUSED (arg))
{
  return 0;
}

/* APPLE LOCAL begin radar 4133425 */
bool objc_diagnose_private_ivar (tree ARG_UNUSED (arg))
{
  return false;
}
/* APPLE LOCAL end radar 4133425 */

tree
objc_lookup_ivar (tree other, tree ARG_UNUSED (arg))
{
  /* Just use whatever C/C++ found.  */
  return other;
}

void
objc_check_decl (tree ARG_UNUSED (decl))
{
}

/* APPLE LOCAL begin radar 4281748 */
void
objc_check_global_decl (tree ARG_UNUSED (decl))
{
}
/* APPLE LOCAL end radar 4281748 */

/* APPLE LOCAL begin radar 4330422 */
tree
objc_non_volatilized_type (tree type)
{
  return type;
}
/* APPLE LOCAL end radar 4330422 */

/* APPLE LOCAL begin radar 4697411 */
void
objc_volatilize_component_ref (tree ARG_UNUSED (cref), tree ARG_UNUSED (type))
{
}
/* APPLE LOCAL end radar 4697411 */
   
int
objc_is_reserved_word (tree ARG_UNUSED (ident))
{
  return 0;
}

/* APPLE LOCAL begin 4154928 */
tree
objc_common_type (tree ARG_UNUSED (type1), tree ARG_UNUSED (type2))
{
  return 0;
}

/* APPLE LOCAL end 4154928 */
/* APPLE LOCAL begin mainline */
/* The 'objc_comptypes' routine has been removed.  */
bool
objc_compare_types (tree ARG_UNUSED (ltyp), tree ARG_UNUSED (rtyp),
		    int ARG_UNUSED (argno), tree ARG_UNUSED (callee))
{
  return false;
}

/* APPLE LOCAL begin radar 4229905 */
bool
objc_have_common_type (tree ARG_UNUSED (ltyp), tree ARG_UNUSED (rtyp),
		       int ARG_UNUSED (argno), tree ARG_UNUSED (callee))
{
  return false;
}
/* APPLE LOCAL end radar 4229905 */

void
objc_volatilize_decl (tree ARG_UNUSED (decl))
{
}

bool
objc_type_quals_match (tree ARG_UNUSED (ltyp), tree ARG_UNUSED (rtyp))
{
  return false;
}

tree
objc_rewrite_function_call (tree function, tree ARG_UNUSED (params))
{
  return function;
}

tree
/* APPLE LOCAL end mainline */
objc_message_selector (void)
{ 
  return 0;
}

void
objc_declare_alias (tree ARG_UNUSED (alias), tree ARG_UNUSED (orig))
{
}

void
objc_declare_class (tree ARG_UNUSED (list))
{
}

void
objc_declare_protocols (tree ARG_UNUSED (list))
{
}

void
objc_start_protocol (tree ARG_UNUSED (proto),
		     tree ARG_UNUSED (protorefs))
{
}

void
objc_start_class_interface (tree ARG_UNUSED (name),
			    tree ARG_UNUSED (super),
/* APPLE LOCAL begin radar 4548636 */
			    tree ARG_UNUSED (protos),
			    tree ARG_UNUSED (attributes))
/* APPLE LOCAL end radar 4548636 */
{
}

void
objc_start_category_interface (tree ARG_UNUSED (name),
			       tree ARG_UNUSED (categ),
			       tree ARG_UNUSED (protos))
{
}

void
objc_continue_interface (void)
{
}

void
objc_finish_interface (void)
{
}

void
objc_add_instance_variable (tree ARG_UNUSED (decl))
{
}

void
objc_set_visibility (int ARG_UNUSED (vis))
{
}

void
objc_set_method_type (enum tree_code ARG_UNUSED (code))
{
}

void
objc_start_class_implementation (tree ARG_UNUSED (name),
				 tree ARG_UNUSED (super))
{
}

void
objc_start_category_implementation (tree ARG_UNUSED (name),
				    tree ARG_UNUSED (categ))
{
}

void
objc_continue_implementation (void)
{
}

void
objc_clear_super_receiver (void)
{
}

void
objc_finish_implementation (void)
{
}

void
/* APPLE LOCAL begin radar 3803157 - objc attribute */
objc_add_method_declaration (tree ARG_UNUSED (signature),
			     tree ARG_UNUSED (attribute))
/* APPLE LOCAL end radar 3803157 - objc attribute */
{
}

void
/* APPLE LOCAL begin radar 3803157 - objc attribute */
objc_start_method_definition (tree ARG_UNUSED (signature),
			      tree ARG_UNUSED (attribute))
/* APPLE LOCAL end radar 3803157 - objc attribute */
{
}

void
objc_finish_method_definition (tree ARG_UNUSED (fndecl))
{
}

tree
objc_build_keyword_decl (tree ARG_UNUSED (selector),
			 tree ARG_UNUSED (typename),
			 tree ARG_UNUSED (identifier))
{
  return 0;
}

tree
objc_build_method_signature (tree ARG_UNUSED (rettype),
			     tree ARG_UNUSED (selectors),
			     tree ARG_UNUSED (optparms))
{
  return 0;
}

tree
objc_build_encode_expr (tree ARG_UNUSED (expr))
{
  return 0;
}

tree
objc_build_protocol_expr (tree ARG_UNUSED (expr))
{
  return 0;
}

tree
objc_build_selector_expr (tree ARG_UNUSED (expr))
{
  return 0;
}

tree
objc_build_message_expr (tree ARG_UNUSED (expr))
{
  return 0;
}

tree
objc_build_string_object (tree ARG_UNUSED (str))
{
  return 0;
}

tree
objc_get_class_reference (tree ARG_UNUSED (name))
{
  return 0;
}

/* APPLE LOCAL begin mainline */
tree
objc_get_class_ivars (tree ARG_UNUSED (name))
{
  return 0;
}
/* APPLE LOCAL end mainline */

/* APPLE LOCAL begin radar 4291785 */
tree
objc_get_interface_ivars (tree ARG_UNUSED (fieldlist))
{
  return 0;
}
void
objc_detect_field_duplicates (tree ARG_UNUSED (fieldlist))
{
}
/* APPLE LOCAL end radar 4291785 */

tree
objc_get_protocol_qualified_type (tree ARG_UNUSED (name),
				  tree ARG_UNUSED (protos))
{
  return 0;
}

int
objc_static_init_needed_p (void)
{
  return 0;
}

tree
objc_generate_static_init_call (tree ARG_UNUSED (ctors))
{
  return 0;
}

int
objc_is_public (tree ARG_UNUSED (expr), tree ARG_UNUSED (identifier))
{
  return 1;
}

/* APPLE LOCAL begin C* language */
void
objc_set_method_opt (int ARG_UNUSED (opt))
{
}

tree
objc_build_component_ref (tree ARG_UNUSED (datum), tree ARG_UNUSED (component))
{
  return 0;
}

tree
objc_build_foreach_components (tree ARG_UNUSED (receiver),
			       tree *ARG_UNUSED (enumState_decl),
			       tree *ARG_UNUSED (items_decl),
			       tree *ARG_UNUSED (limit_decl),
			       tree *ARG_UNUSED (startMutations_decl),
			       tree *ARG_UNUSED (counter_decl),
			       tree *ARG_UNUSED (countByEnumeratingWithState))
{
  return 0;
}
/* APPLE LOCAL end C* language */

/* APPLE LOCAL begin C* property (Radar 4436866) */
void
objc_set_property_attr (int ARG_UNUSED (code), tree ARG_UNUSED (identifier))
{
}
void
objc_add_property_variable (tree ARG_UNUSED (prop))
{
}
tree
objc_build_getter_call (tree ARG_UNUSED (datum), tree ARG_UNUSED (component))
{
  return 0;
}
tree
objc_build_setter_call (tree ARG_UNUSED (lhs), tree ARG_UNUSED (rhs))
{
  return 0;
}
/* APPLE LOCAL end C* property (Radar 4436866) */

/* APPLE LOCAL begin mainline */
tree
objc_build_throw_stmt (tree ARG_UNUSED (expr))
{
  return 0;
}

void
objc_begin_try_stmt (location_t ARG_UNUSED (loc), tree ARG_UNUSED (stmt))
{
}

tree
objc_finish_try_stmt (void)
{
  return 0;
}

void
objc_begin_catch_clause (tree ARG_UNUSED (parm))
{
}

void
objc_finish_catch_clause (void) {
}

void
objc_build_finally_clause (location_t ARG_UNUSED (location),
			   tree ARG_UNUSED (stmt))
{
}

tree
objc_build_synchronized (location_t ARG_UNUSED (location), 
			 tree ARG_UNUSED (lock), tree ARG_UNUSED (stmt))
{
  return 0;
}

tree
objc_generate_write_barrier (tree ARG_UNUSED (lhs),
			     enum tree_code ARG_UNUSED (modifycode),
			     tree ARG_UNUSED (rhs))
{
  return 0;
}  
/* APPLE LOCAL end mainline */

/* APPLE LOCAL begin radar 5276085 */
void objc_weak_reference_expr (tree* ARG_UNUSED (expr))
{
}

tree
objc_build_weak_reference_tree (tree expr)
{
  return expr;
}
/* APPLE LOCAL end radar 5276085 */

/* APPLE LOCAL begin ObjC new abi */
tree
objc_v2_build_ivar_ref (tree ARG_UNUSED (datum), tree ARG_UNUSED (component))
{
  return 0;
}
/* APPLE LOCAL end ObjC new abi */
/* APPLE LOCAL begin C* warnings to easy porting to new abi */
void
diagnose_selector_cast (tree ARG_UNUSED (cast_type), tree ARG_UNUSED (sel_exp))
{
}
/* APPLE LOCAL end C* warnings to easy porting to new abi */

/* APPLE LOCAL begin radar 4441049 */
tree
objc_v2_component_ref_field_offset (tree ARG_UNUSED (exp))
{
  return 0;
}
tree
objc_v2_bitfield_ivar_bitpos (tree ARG_UNUSED (exp))
{
  return 0;
}
/* APPLE LOCAL end radar 4441049 */ 
/* APPLE LOCAL begin radar 4507230 */
bool 
objc_type_valid_for_messaging (tree ARG_UNUSED (exp))
{
  return false;
}
/* APPLE LOCAL end radar 4507230 */
/* APPLE LOCAL begin radar 3803157 - objc attribute */
bool 
objc_method_decl (enum tree_code ARG_UNUSED (opcode))
{
  return false;
}
/* APPLE LOCAL end radar 3803157 - objc attribute */

/* APPLE LOCAL begin radar 4533974 - ObjC new protocol */
void 
objc_protocol_implementation (tree ARG_UNUSED (protolist))
{
  return;
}
/* APPLE LOCAL end radar 4533974 - ObjC new protocol */
/* APPLE LOCAL begin radar 4592503 */
void
objc_checkon_weak_attribute (tree ARG_UNUSED (decl)) 
{
  return;
}
/* APPLE LOCAL end radar 4592503 */
/* APPLE LOCAL begin radar 4712269 */
tree
objc_build_incr_decr_setter_call (enum tree_code ARG_UNUSED (code), 
				       tree ARG_UNUSED (lhs), 
				       tree ARG_UNUSED (inc))
{
  return NULL_TREE;
}
/* APPLE LOCAL end radar 4712269 */
/* APPLE LOCAL begin objc new property */
void objc_declare_property_impl (int ARG_UNUSED (code), 
				 tree ARG_UNUSED (tree_list))
{
}
/* APPLE LOCAL end objc new property */
/* APPLE LOCAL begin radar 2848255 */
bool 
objc2_valid_objc_catch_type (tree ARG_UNUSED (type))
{
  return false;
}
tree
objc2_build_throw_call (tree ARG_UNUSED (exp))
{
  return NULL_TREE;
}
/* APPLE LOCAL end radar 2848255 */
/* APPLE LOCAL begin radar 4985544 */
bool
objc_check_format_nsstring (tree ARG_UNUSED (argument), 
			    unsigned HOST_WIDE_INT ARG_UNUSED (format_num),
                     	    bool * ARG_UNUSED(no_add_attrs))
{
  return false;
}
/* APPLE LOCAL end radar 4985544 */
/* APPLE LOCAL begin radar 2996215 */
tree
objc_create_init_utf16_var (const unsigned char * ARG_UNUSED (inbuf), 
			    size_t  ARG_UNUSED (length),
			    size_t  *ARG_UNUSED (numUniChars))
{
  return NULL_TREE;
}
/* APPLE LOCAL end radar 2996215 */
