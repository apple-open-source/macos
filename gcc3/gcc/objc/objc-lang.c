/* Language-dependent hooks for Objective-C.
   Copyright 2001 Free Software Foundation, Inc.
   Contributed by Ziemowit Laski  <zlaski@apple.com>

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
#include "system.h"
#include "tree.h"
/* APPLE LOCAL begin Objective-C++ */
#ifdef OBJCPLUS
#include "cp-tree.h"
#else
#include "c-tree.h"
#endif
/* APPLE LOCAL end Objective-C++ */
#include "c-common.h"
#include "toplev.h"
#include "objc-act.h"
#include "langhooks.h"
#include "langhooks-def.h"

/* APPLE LOCAL PFE */
#ifdef PFE
#include "pfe/pfe.h"
#include "pfe/objc-freeze-thaw.h"
#endif

static void objc_init_options                   PARAMS ((void));
static void objc_post_options                   PARAMS ((void));

/* APPLE LOCAL begin Objective-C++  */
#ifdef OBJCPLUS
#include "cp-lang.h"
#undef LANG_HOOKS_NAME
#define LANG_HOOKS_NAME "GNU Objective-C++"
#else
#include "c-lang.h"
#undef LANG_HOOKS_NAME
#define LANG_HOOKS_NAME "GNU Objective-C"
#endif
/* APPLE LOCAL end Objective-C++ */

#undef LANG_HOOKS_INIT
#define LANG_HOOKS_INIT objc_init
#undef LANG_HOOKS_FINISH
#define LANG_HOOKS_FINISH c_common_finish
/* APPLE LOCAL begin objc finish file */
#undef LANG_HOOKS_FINISH_FILE
#define LANG_HOOKS_FINISH_FILE objc_finish_file
/* APPLE LOCAL end objc finish file */
#undef LANG_HOOKS_INIT_OPTIONS
#define LANG_HOOKS_INIT_OPTIONS objc_init_options
#undef LANG_HOOKS_DECODE_OPTION
#define LANG_HOOKS_DECODE_OPTION objc_decode_option
#undef LANG_HOOKS_POST_OPTIONS
#define LANG_HOOKS_POST_OPTIONS objc_post_options

/* APPLE LOCAL Objective-C++ */
/* Don't define LANG_HOOKS_STATICP etc here, get from c{p}-lang.h.  */

/* APPLE LOCAL begin PFE */
#ifdef PFE
#undef LANG_HOOKS_PFE_LANG_INIT
#define LANG_HOOKS_PFE_LANG_INIT objc_pfe_lang_init
#undef LANG_HOOKS_PFE_FREEZE_THAW_COMPILER_STATE
#define LANG_HOOKS_PFE_FREEZE_THAW_COMPILER_STATE objc_freeze_thaw_compiler_state
#undef LANG_HOOKS_PFE_FREEZE_THAW_DECL
#define LANG_HOOKS_PFE_FREEZE_THAW_DECL	objc_pfe_freeze_thaw_decl
#undef LANG_HOOKS_PFE_FREEZE_THAW_TYPE
#define LANG_HOOKS_PFE_FREEZE_THAW_TYPE	objc_pfe_freeze_thaw_type
#undef LANG_HOOKS_PFE_FREEZE_THAW_SPECIAL
#define LANG_HOOKS_PFE_FREEZE_THAW_SPECIAL objc_pfe_freeze_thaw_special
#undef LANG_HOOKS_PFE_CHECK_ALL_STRUCT_SIZES
#define LANG_HOOKS_PFE_CHECK_ALL_STRUCT_SIZES objc_pfe_check_all_struct_sizes
#undef LANG_HOOKS_PFE_CHECK_SETTINGS
#define LANG_HOOKS_PFE_CHECK_SETTINGS objc_pfe_check_settings
#endif
/* APPLE LOCAL end PFE */

/* Each front end provides its own hooks, for toplev.c.  */
const struct lang_hooks lang_hooks = LANG_HOOKS_INITIALIZER;

static void 
objc_init_options ()
{
  /* APPLE LOCAL compiling_objc  */
  compiling_objc = 1;
#ifdef OBJCPLUS
  cxx_init_options (clk_cplusplus);
#else
  c_common_init_options (clk_c);
#endif  
} 

/* Post-switch processing.  */

static void
objc_post_options ()
{
  c_common_post_options ();
}


