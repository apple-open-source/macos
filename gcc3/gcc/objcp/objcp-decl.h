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

#ifndef GCC_OBJCP_DECL_H
#define GCC_OBJCP_DECL_H

extern tree objcp_start_struct		PARAMS ((enum tree_code, tree));
extern tree objcp_finish_struct		PARAMS ((tree, tree, tree));
extern int objcp_start_function		PARAMS ((tree, tree, tree));
extern void objcp_finish_function	PARAMS ((int));
extern tree objcp_start_decl		PARAMS ((tree, tree, int, tree));
extern void objcp_finish_decl		PARAMS ((tree, tree, tree));
extern tree objcp_lookup_name		PARAMS ((tree));
extern tree objcp_push_parm_decl	PARAMS ((tree));
extern tree objcp_get_parm_info		PARAMS ((int));
extern void objcp_store_parm_decls	PARAMS ((void));
extern tree objcp_build_function_call	PARAMS ((tree, tree));
extern tree objcp_xref_tag		PARAMS ((enum tree_code, tree));
extern tree objcp_grokfield		PARAMS ((const char *, int, tree, tree, tree));
extern tree objcp_build_component_ref	PARAMS ((tree, tree));
extern int objcp_comptypes		PARAMS ((tree, tree));
extern tree objcp_type_name		PARAMS ((tree));
extern tree objcp_type_size		PARAMS ((tree));
extern tree objcp_builtin_function 	PARAMS ((const char *, tree, int, 
						 enum built_in_class, const char *));

extern int objcp_lookup_identifier	PARAMS ((tree, tree *, int));
						 
/* Now "cover up" the corresponding C++ functions if required (NB: the 
   OBJCP_ORIGINAL_FUNCTION macro, shown below, can still be used to
   invoke the original C++ functions if needed).  */
#ifdef OBJCP_REMAP_FUNCTIONS

#define start_struct(code, name) \
	objcp_start_struct (code, name)
#define finish_struct(t, fieldlist, attributes) \
	objcp_finish_struct (t, fieldlist, attributes)
#define start_function(declspecs, declarator, attributes) \
	objcp_start_function (declspecs, declarator, attributes)
#define finish_function(nested) \
	objcp_finish_function (nested)
#define start_decl(declarator, declspecs, initialized, attributes) \
	objcp_start_decl (declarator, declspecs, initialized, attributes)
#define finish_decl(decl, init, asmspec) \
	objcp_finish_decl (decl, init, asmspec)
#define lookup_name(name) \
	objcp_lookup_name (name)
#define push_parm_decl(parm) \
	objcp_push_parm_decl (parm)
#define get_parm_info(void_at_end) \
	objcp_get_parm_info (void_at_end)
#define store_parm_decls() \
	objcp_store_parm_decls ()
#define build_function_call(function, args) \
	objcp_build_function_call (function, args)
#define xref_tag(code, name) \
	objcp_xref_tag (code, name)
#define grokfield(filename, line, declarator, declspecs, width) \
	objcp_grokfield (filename, line, declarator, declspecs, width)
#define build_component_ref(datum, component) \
        objcp_build_component_ref (datum, component)
#define comptypes(type1, type2) \
	objcp_comptypes (type1, type2)
#define builtin_function(name, type, code, class, libname) \
	objcp_builtin_function(name, type, code, class, libname)
	
#undef TYPE_NAME
#define TYPE_NAME(type) \
	objcp_type_name (type)

#undef TYPE_SIZE
#define TYPE_SIZE(type) \
	objcp_type_size (type)
   	
#define OBJCP_ORIGINAL_FUNCTION(name, args) 	(name)args

#endif  /* OBJCP_REMAP_FUNCTIONS */

#endif /* ! GCC_OBJCP_DECL_H */
