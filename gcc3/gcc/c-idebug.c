/* APPLE LOCAL file debugging */
/* C-specific tree accessors defined as functions for use in a debugger.
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
  
   Note that this file includes idebug.c so that only debugging
   macros for c-tree.h are actually defined here.  For C only
   this file is included in the link while for C only idebug.c
   is built and included in the link.  */

#include "idebug.c"

#ifdef ENABLE_IDEBUG

#include "c-tree.h"

/* Macros from c-tree.h */

fn_1( IDENTIFIER_GLOBAL_VALUE, tree, tree )
fn_1( IDENTIFIER_LOCAL_VALUE, tree, tree )
fn_1( IDENTIFIER_LABEL_VALUE, tree, tree )
fn_1( IDENTIFIER_LIMBO_VALUE, tree, tree )
fn_1( IDENTIFIER_IMPLICIT_DECL, tree, tree )
fn_1( IDENTIFIER_ERROR_LOCUS, tree, tree )
fn_1( C_TYPE_FIELDS_READONLY, int, tree )
fn_1( C_TYPE_FIELDS_VOLATILE, int, tree )
fn_1( C_TYPE_BEING_DEFINED, int, tree )
fn_1( C_IS_RESERVED_WORD, int, tree )
fn_1( C_TYPE_VARIABLE_SIZE, int, tree )
fn_1( C_DECL_VARIABLE_SIZE, int, tree )
fn_2( C_SET_EXP_ORIGINAL_CODE, int, tree, tree )
fn_1( C_TYPEDEF_EXPLICITLY_SIGNED, int, tree )
fn_1( C_DECL_ANTICIPATED, int, tree )
fn_1( TYPE_ACTUAL_ARG_TYPES, tree, tree )

#endif /* ENABLE_IDEBUG */
