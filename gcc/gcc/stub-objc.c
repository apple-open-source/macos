/* APPLE LOCAL file Objective-C++ */
/* Stub functions for Objective-C and Objective-C++ routines
   that are called from within the C and C++ front-ends,
   respectively.
   Copyright (C) 1991, 1995, 1997, 1998,
   1999, 2000, 2001, 2002 Free Software Foundation, Inc.

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
#include "tree.h"

tree
lookup_interface (arg)
     tree arg ATTRIBUTE_UNUSED;
{
  return 0;
}

tree
is_class_name (arg)
    tree arg ATTRIBUTE_UNUSED;
{
  return 0;
}

tree
objc_is_id (arg)
    tree arg ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Used by c-typeck.c (build_external_ref), but only for objc.  */

tree
lookup_objc_ivar (id)
     tree id ATTRIBUTE_UNUSED;
{
  return 0;
}

/* APPLE LOCAL begin Objective-C++ */
/* Used by cp/lex.c (do_identifier), but only for objc++.  */
int
objcp_lookup_identifier (token, id, check_conflict)
     tree token ATTRIBUTE_UNUSED;
     tree *id ATTRIBUTE_UNUSED;
     int check_conflict ATTRIBUTE_UNUSED;
{
  return 0;
}  

/* Used by cp/decl2.c (build_expr_from_tree), but only for objc++.  */
tree
finish_message_expr (receiver, sel_name, method_params)
     tree receiver ATTRIBUTE_UNUSED;
     tree sel_name ATTRIBUTE_UNUSED;
     tree method_params ATTRIBUTE_UNUSED;
{
  return 0;
}

/* Used by cp/decl2.c (build_expr_from_tree), but only for objc++.  */
tree
get_class_reference (ident)
     tree ident ATTRIBUTE_UNUSED;
{
  return 0;
}
/* APPLE LOCAL end Objective-C++ */
