/* APPLE LOCAL file Objective-C++ */
/* Process the ObjC-specific declarations and variables for 
   the Objective-C++ compiler.
   Copyright (C) 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000,
   2001 Free Software Foundation, Inc.

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

/* Process declarations and symbol lookup for C front end.
   Also constructs types; the standard scalar types at initialization,
   and structure, union, array and enum types when they are declared.  */

/* ??? not all decl nodes are given the most useful possible
   line numbers.  For example, the CONST_DECLs for enum values.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "rtl.h"
#include "expr.h"
#include "cp-tree.h"
#include "lex.h"
#include "c-common.h"
#include "flags.h"
#include "input.h"
#include "except.h"
#include "output.h"
#include "toplev.h"
#include "cpplib.h"
#include "debug.h"
#include "target.h"
#include "varray.h"

#include "objc-act.h"
#include "objcp-decl.h"

/* APPLE LOCAL indexing */
#include "genindex.h"

static tree objcp_parmlist = NULL_TREE;

/* Hacks to simulate start_struct() and finish_struct(). */

tree 
objcp_start_struct (code, name)
     enum tree_code code ATTRIBUTE_UNUSED; 
     tree name;
{ 
  int new_scope = 0;
  tree h, s;
  /* The idea here is to mimic the actions that the C++ parser takes when
     constructing 'extern "C" struct {'.  */
  push_lang_context (lang_name_c);
  if (!name)
     name = make_anon_name ();
  h = handle_class_head (record_type, 0, name, 0, 1, &new_scope);

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  s = begin_class_definition (TREE_TYPE (h));

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;

  return s;	
}

tree 
objcp_finish_struct (t, fieldlist, attributes)
     tree t; 
     tree fieldlist, attributes;
{
  tree s, field, next_field;

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 1;

  for (field = fieldlist; field; field = next_field) 
  {
    next_field = TREE_CHAIN (field);      /* insert one field at a time;  */
    TREE_CHAIN (field) = NULL_TREE;       /* otherwise, grokfield croaks. */
    finish_member_declaration (field);
  } 
  s = finish_class_definition (t, attributes, 1, 0);  

  /* APPLE LOCAL indexing dpatel */
  flag_suppress_builtin_indexing = 0;

  pop_lang_context ();
  return s;
}

int
objcp_start_function (declspecs, declarator, attributes)
     tree declspecs, declarator, attributes;
{ 
  return start_function (declspecs, declarator, attributes, 0);
}

void
objcp_finish_function (nested)
     int nested ATTRIBUTE_UNUSED;
{
  /* The C++ flavor of 'finish_function' does not generate RTL -- one has
     to call 'expand_body' to do that.  */
  expand_body (finish_function (0));
}

tree 
objcp_start_decl (declarator, declspecs, initialized, attributes)
     tree declarator, declspecs;
     int initialized;
     tree attributes;
{
  return start_decl (declarator, declspecs, initialized, 
		     attributes, NULL_TREE);
}
			  
void
objcp_finish_decl (decl, init, asmspec)
     tree decl, init, asmspec;
{
  cp_finish_decl (decl, init, asmspec, 0);
}

tree
objcp_lookup_name (name)
     tree name;
{
  return lookup_name (name, -1);
}

/* Hacks to simulate push_parm_decl() and get_parm_info(). */

tree
objcp_push_parm_decl (parm)
     tree parm;
{
  /* C++ parms are laid out slightly differently from C parms.  Adjust
     for this here.  */
  TREE_VALUE (parm) = TREE_PURPOSE (parm);
  TREE_PURPOSE (parm) = NULL_TREE;   
  
  if (objcp_parmlist)
    objcp_parmlist = chainon (objcp_parmlist, parm);
  else
    objcp_parmlist = parm;

  return objcp_parmlist;
}

tree 
objcp_get_parm_info (void_at_end) 
     int void_at_end;
{
  tree parm_info = finish_parmlist (objcp_parmlist, !void_at_end);
  
  /* The C++ notion of a parameter list differs slightly from that of
     C.  Adjust for this.  */
  parm_info = build_tree_list (parm_info, NULL_TREE);       
  objcp_parmlist = NULL_TREE;

  return parm_info;
}

void 
objcp_store_parm_decls ()
{
  /* In C++ land, 'start_function' calls 'store_parm_decls'; hence we
     do not need to do anything here.  */
}

tree 
objcp_xref_tag (code, name)
     enum tree_code code;
     tree name;
{  
  if (code != RECORD_TYPE)
    abort ();   /* this is sheer laziness... */   
  return xref_tag (record_type, name, 0, 1);
}

tree
objcp_grokfield (filename, line, declarator, declspecs, width)
     const char *filename ATTRIBUTE_UNUSED;
     int line ATTRIBUTE_UNUSED;
     tree declarator, declspecs, width;
{     
  return (width) ? grokbitfield (declarator, declspecs, width)
		 : grokfield (declarator, declspecs, 0, 0, 0);
}		 

tree
objcp_build_component_ref (datum, component) 
     tree datum, component;
{
  /* The 'build_component_ref' routine has been removed from the C++
     front-end, but 'finish_class_member_access_expr' seems to be
     a worthy substitute.  */
  return finish_class_member_access_expr (datum, component);
}  

int
/* BEGIN APPLE LOCAL IMI */
objcp_comptypes (type1, type2, different_tu) 
/* END APPLE LOCAL IMI */
     tree type1, type2;
/* BEGIN APPLE LOCAL IMI */
     int different_tu;
/* END APPLE LOCAL IMI */
{     
/* BEGIN APPLE LOCAL IMI */
  if (different_tu)
    abort ();
  
/* END APPLE LOCAL IMI */
  return comptypes (type1, type2, 0);
}

/* C++'s version of 'builtin_function' winds up placing our precious
   objc_msgSend and friends in namespace std!  This will not do.
   We shall hence duplicate C's 'builtin_function' here instead.   */
   
tree
objcp_builtin_function (name, type, code, class, libname, attrs)
     const char *name;
     tree type;
     int code;
     enum built_in_class class;
     const char *libname ATTRIBUTE_UNUSED;
     tree attrs;
{
  tree decl = NULL;
  decl = build_decl (FUNCTION_DECL, get_identifier (name), type);
  DECL_EXTERNAL (decl) = 1;
  TREE_PUBLIC (decl) = 1;
  make_decl_rtl (decl, NULL);
  pushdecl (decl);
  DECL_BUILT_IN_CLASS (decl) = class;
  DECL_FUNCTION_CODE (decl) = code;
  DECL_ANTICIPATED (decl) = 1;

  /* Possibly apply some default attributes to this built-in function.  */
  if (attrs)
    decl_attributes (&decl, attrs, ATTR_FLAG_BUILT_IN);
  else
    decl_attributes (&decl, NULL_TREE, 0);

  return decl;
}

int
objcp_lookup_identifier (token, id, check_conflict)
     tree token;
     tree *id;
     int check_conflict;
{
  tree objc_id = lookup_objc_ivar (token);
  
  if (!check_conflict || (objc_id && IS_SUPER (objc_id)))
    *id = objc_id;
  else if (objc_id && *id && IDENTIFIER_BINDING (token)) 
    warning ("local declaration of `%s' hides instance variable",
	     IDENTIFIER_POINTER (token));
	     
  return (objc_id != NULL_TREE);
}  
