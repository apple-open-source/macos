/* APPLE LOCAL PFE */
/* Freeze/thaw data common to c, c++, and objc.
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

#include "config.h"

#ifdef PFE

#include "system.h"
#include "tree.h"
#include "c-common.h"

#include "pfe.h"

/*-------------------------------------------------------------------*/

/* Handles freeezing/thawing the common struct language_function.  */
   
void 
pfe_freeze_thaw_common_language_function (p)
     struct language_function *p;
{
  if (p)
    {
      PFE_FREEZE_THAW_WALK (p->x_stmt_tree.x_last_stmt);
      PFE_FREEZE_THAW_WALK (p->x_stmt_tree.x_last_expr_type);
      pfe_freeze_thaw_ptr_fp (&p->x_stmt_tree.x_last_expr_filename);
      PFE_FREEZE_THAW_WALK (p->x_scope_stmt_stack);
    }
}

/*-------------------------------------------------------------------*/

#endif /* PFE */


#if 0

cd $gcc3/gcc/pfe; \
cc -no-cpp-precomp -c  -DIN_GCC  -g \
  -W -Wall -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wtraditional -pedantic -Wno-long-long \
  -DHAVE_CONFIG_H \
  -I$gcc3obj \
  -I. \
  -I.. \
  -I../config \
  -I../../include \
  c-common-freeze-thaw.c -o ~/tmp.o -w 

#endif
