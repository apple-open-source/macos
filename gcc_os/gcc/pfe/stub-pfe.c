/* APPLE LOCAL PFE */
/* Stub pfe.c to satisfy links for cpp0 and fix-header.
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
#include "pfe.h"
#include "pfe-header.h"

/* This file provides the dummy stubs for the pfe routines.
   These stubs are referenced by pfe code that is part of files
   which may also be linked in with stand-alone tools like cpp0
   and fix-header, or with front ends that do not support pfe,
   e.g., Fortran.  */

/*-------------------------------------------------------------------*/

enum pfe_action pfe_operation = PFE_NOT_INITIALIZED;
pfe_freeze_thaw_ptr_t pfe_freeze_thaw_ptr_fp = NULL;
struct pfe_compiler_state *pfe_compiler_state_ptr = NULL;
int pfe_display_precomp_headers = 0;
FILE *pfe_file;
const char *pfe_name;
tree pfe_predefined_global_names;

void 
pfe_init (action)
     enum pfe_action action ATTRIBUTE_UNUSED;
{
  pfe_operation = PFE_NOP;
}

void
pfe_c_init_decl_processing ()
{
}

void
pfe_cxx_init_decl_processing ()
{
}

void 
pfe_term ()
{
}

void 
pfe_open_pfe_file (pfe_dir, arch, fdump)
     char *pfe_dir ATTRIBUTE_UNUSED;
     char *arch ATTRIBUTE_UNUSED;
     int fdump ATTRIBUTE_UNUSED;
{
}

void
pfe_close_pfe_file (delete_it)
     int delete_it ATTRIBUTE_UNUSED;
{
}

void * 
pfe_malloc (size)
     size_t size;
{
  return xmalloc (size);
}

#if PFE_MALLOC_STATS
void * 
pfe_s_malloc (size, kind)
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  return xmalloc (size);
}

void * 
pfe_s_calloc (nobj, size, kind)
     size_t nobj;
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  return xcalloc (nobj, size);
}

void * 
pfe_s_realloc (p, size, kind)
     void *p;
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  return xrealloc (p, size);
}
#endif /* PFE_MALLOC_STATS  */

void * 
pfe_calloc (nobj, size)
     size_t nobj;
     size_t size; 
{
  return xcalloc (nobj, size);
}

void * 
pfe_realloc (p, size)
     void *p;
     size_t size; 
{
  return xrealloc (p, size);
}

void *
pfe_savestring (str)
     char *str;
{ 
   return str;   
}

void 
pfe_free (ptr)
     void *ptr;
{
  if (ptr)
    free (ptr);
}

void 
pfe_using_temp_ptr (p)
     void **p ATTRIBUTE_UNUSED;
{
}

int
pfe_check_header (fname, timestamp, inode)
     const char *fname ATTRIBUTE_UNUSED;
     time_t timestamp ATTRIBUTE_UNUSED;
     ino_t inode ATTRIBUTE_UNUSED;
{
  return 0;
}

void 
pfe_add_header_name (name, timestamp, inode)
     const char *name ATTRIBUTE_UNUSED;
     time_t timestamp ATTRIBUTE_UNUSED;
     ino_t inode ATTRIBUTE_UNUSED;
{
}

void 
pfe_check_lang (lang)
     enum pfe_lang lang ATTRIBUTE_UNUSED;
{
}

void 
pfe_set_lang (lang)
     enum pfe_lang lang ATTRIBUTE_UNUSED;
{
}

int 
pfe_is_pfe_mem (f)
     void *f ATTRIBUTE_UNUSED;
{
  return 0;
}

void *
pfe_freeze_ptr (p)
     void *p ATTRIBUTE_UNUSED;
{
  return NULL;
}
            
void *
pfe_thaw_ptr (p)
     void *p ATTRIBUTE_UNUSED;
{
  return NULL;
}
            
void *
pfe_freeze_thaw_ptr (pp)
     void **pp ATTRIBUTE_UNUSED;
{
  return NULL;
}

int
pfe_is_frozen (ptr)
     void *ptr ATTRIBUTE_UNUSED;
{
  return 0;
}  

void 
pfe_freeze_thaw_cpp_hashnode (hnp)
     struct cpp_hashnode **hnp ATTRIBUTE_UNUSED;
{
}

void
pfe_freeze_thaw_cpp_token (tp)
     struct cpp_token **tp ATTRIBUTE_UNUSED;
{
}

void 
pfe_freeze_thaw_rtx (r)
     struct rtx_def **r ATTRIBUTE_UNUSED;
{
}

void 
pfe_freeze_thaw_language_function (pp)
     struct language_function **pp ATTRIBUTE_UNUSED;
{
}

void 
pfe_freeze_thaw_rtvec (v)
     struct rtvec_def **v ATTRIBUTE_UNUSED;
{
}
          
void *
pfe_real_ptr (p)
     void *p ATTRIBUTE_UNUSED;
{
  return NULL;
}

#if PFE_NEW_TREE_WALK
void
pfe_freeze_thaw_tree_push (node)
     tree *node ATTRIBUTE_UNUSED;
{
}

void
pfe_freeze_thaw_tree_walk (void)
{
}
#else
void
pfe_freeze_thaw_tree_walk (node)
     tree *node ATTRIBUTE_UNUSED;
{
}
#endif

int 
pfe_dump_compiler_state (file)
     FILE *file ATTRIBUTE_UNUSED;
{
  return 0;
}
     
void 
pfe_load_compiler_state (file)
     FILE *file ATTRIBUTE_UNUSED;
{
}

void
pfe_setdecls (names)
     tree names;
{
}

void 
pfe_decode_dbgpfe_options (str)
     const char *str;
{
}

void
pfe_check_struct_size (actual_size, assumed_size, name)
     int actual_size ATTRIBUTE_UNUSED;
     int assumed_size ATTRIBUTE_UNUSED;
     const char *name ATTRIBUTE_UNUSED;
{
}

void
pfe_check_compiler ()
{
}

int pfe_macro_validation = 0;
int pfe_cmd_ln_macro_count = 0;
int pfe_macro_status = 0;

void
pfe_set_cmd_ln_processing ()
{
}

void
pfe_reset_cmd_ln_processing ()
{
}

int
pfe_is_cmd_ln_processing ()
{
  return 0;
 }

 void
 pfe_check_cmd_ln_macros ()
 {
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
  stub-pfe.c -o ~/tmp.o -w 

#endif
