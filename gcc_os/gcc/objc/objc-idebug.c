/* APPLE LOCAL file debugging */
/* ObjC tree & rtl accessors defined as functions for use in a debugger.
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

/* What we do here is to instantiate each macro as a function *BY
   THE SAME NAME*.  Depends on the macro not being expanded when
   it is surrounded by parens.  
  
   Note that this file includes idebug.c or cp/cp-idebug.c (depending
   on weather OBJCPLUS is defined) so that only debugging macros for
   objc-act.h are actually defined here.  For objc only this file is
   included in the link while for C only idebug.c is built and inlcuded
   in the link.  Similarly, for objc++ this is the only included file
   and cp-idebug.c is not linked.  */

#ifdef OBJCPLUS
#include "cp/cp-idebug.c"
#else
#include "c-idebug.c"
#endif

#ifdef ENABLE_IDEBUG

#include "objc-act.h"

/* Macros from objc/objc-act.h  */

fn_1 ( KEYWORD_KEY_NAME, tree, tree )
fn_1 ( KEYWORD_ARG_NAME, tree, tree )
fn_1 ( METHOD_SEL_NAME, tree, tree )
fn_1 ( METHOD_SEL_ARGS, tree, tree )
fn_1 ( METHOD_ADD_ARGS, tree, tree )
fn_1 ( METHOD_DEFINITION, tree, tree )
fn_1 ( METHOD_ENCODING, tree, tree )
fn_1 ( CLASS_NAME, tree, tree )
fn_1 ( CLASS_SUPER_NAME, tree, tree )
fn_1 ( CLASS_IVARS, tree, tree )
fn_1 ( CLASS_RAW_IVARS, tree, tree )
fn_1 ( CLASS_NST_METHODS, tree, tree )
fn_1 ( CLASS_CLS_METHODS, tree, tree )
fn_1 ( CLASS_OWN_IVARS, tree, tree )
fn_1 ( CLASS_STATIC_TEMPLATE, tree, tree )
fn_1 ( CLASS_CATEGORY_LIST, tree, tree )
fn_1 ( CLASS_PROTOCOL_LIST, tree, tree )
fn_1 ( PROTOCOL_NAME, tree, tree )
fn_1 ( PROTOCOL_LIST, tree, tree )
fn_1 ( PROTOCOL_NST_METHODS, tree, tree )
fn_1 ( PROTOCOL_CLS_METHODS, tree, tree )
fn_1 ( PROTOCOL_FORWARD_DECL, tree, tree )
fn_1 ( PROTOCOL_DEFINED, tree, tree )
fn_1 ( TREE_STATIC_TEMPLATE, int, tree )
fn_1 ( TYPE_PROTOCOL_LIST, tree, tree)

#endif /* ENABLE_IDEBUG */
