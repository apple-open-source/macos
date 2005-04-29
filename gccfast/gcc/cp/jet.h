/* APPLE LOCAL file jet */
/* Eliminate dead declarations from the token stream. 
   Copyright (C) 2004 Apple

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef GCC_CP_JET_H
#define GCC_CP_JET_H

#include "tree.h"

/* If flag_jet is nonzero, this function must be called before calling
   any of the other functions declared in this header. */
extern void initialize_jet (void);

/* Jet equivalents of c_lex, and _cpp_backup_tokens. */
extern int c_get_token (tree *value);
extern void c_backup_tokens (unsigned int n);

/* A hash code, computed by initialize_jet.  It is based on 
   toplev_argv_hash and on all of the tokens that initialize_jet
   marks as being used. */
extern unsigned int jet_token_hash;

#endif /* GCC_CP_JET_H */
