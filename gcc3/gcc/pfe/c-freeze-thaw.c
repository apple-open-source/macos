/* APPLE LOCAL PFE */
/* Freeze/thaw C-specific trees and other data.
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

#include "c-tree.h"

#include "pfe.h"
#include "pfe-header.h"

#undef VARRAY_FREE
#include "c-freeze-thaw.h"

/*-------------------------------------------------------------------*/

/* Initialize language specific compiler state.  */
void 
c_pfe_lang_init (lang)
  int lang;
{
  if (pfe_operation == PFE_DUMP)
    pfe_set_lang ((enum pfe_lang) lang == PFE_LANG_UNKNOWN ?
		  PFE_LANG_C : lang);
  else if (pfe_operation == PFE_LOAD)
    pfe_check_lang ((enum pfe_lang) lang == PFE_LANG_UNKNOWN ?
		    PFE_LANG_C : lang);

  /* Initialize the language specific compiler state */
  if (pfe_operation == PFE_DUMP)
    pfe_compiler_state_ptr->lang_specific  = (struct pfe_lang_compiler_state *)
      pfe_calloc (1, sizeof (struct pfe_lang_compiler_state));
}

/* Freeze/thaw language specific compiler state.  */
void 
c_freeze_thaw_compiler_state (pp)
     struct pfe_lang_compiler_state **pp;
{
  struct pfe_lang_compiler_state *hdr;
  
  hdr = (struct pfe_lang_compiler_state *)pfe_freeze_thaw_ptr_fp (pp);
  
  pfe_freeze_thaw_c_lang_globals (hdr);
}

/* Check language-specific compiler options.  */
void 
c_pfe_check_settings (lang_specific)
    struct pfe_lang_compiler_state *lang_specific ATTRIBUTE_UNUSED;
{
}

/* See freeze-thaw.c for documentation of these routines.  */

/* Handle 'd' nodes */
int 
c_pfe_freeze_thaw_decl (node)
     tree node;
{  
  if (DECL_LANG_SPECIFIC (node))
    PFE_FREEZE_THAW_WALK (DECL_LANG_SPECIFIC (node)->pending_sizes);
  
  switch (TREE_CODE (node))
    {
      case FUNCTION_DECL:
      	#if 0
      	/* Moved to common code in freeze-thaw.c  */
        if (DECL_LANG_SPECIFIC (node))
          {
      	    PFE_FREEZE_THAW_WALK (DECL_SAVED_TREE (node));
      	  }
      	#endif
  	return 0; /* let normal processing continue */
  	
      default:
  	return 0; /* let normal processing continue */
    }
}

/* Handle 't' nodes */
int 
c_pfe_freeze_thaw_type (node)
     tree node ATTRIBUTE_UNUSED;
{
  /* no TYPE_LANG_SPECIFIC (node) for C */
  /* FIXME?: c-tree.h has struct lang_type but it appears to be unused  */
    
  #if 0
  switch (TREE_CODE (node))
    {
      default:
        break;
    }
  #endif
  
  return 0; /* let normal processing continue */
}

/* Handle 'c' and 'x' nodes */
int 
c_pfe_freeze_thaw_special (node)
     tree node;
{
  switch (TREE_CODE (node))
    {
      case IDENTIFIER_NODE:
	PFE_FREEZE_THAW_WALK (IDENTIFIER_GLOBAL_VALUE (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_LOCAL_VALUE (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_LABEL_VALUE (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_IMPLICIT_DECL (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_ERROR_LOCUS (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_LIMBO_VALUE (node));
  	return 0; /* let normal processing continue */
      
      default:
  	return 0; /* let normal processing continue */
    }
    
}

/*-------------------------------------------------------------------*/

/* This code below is to contol the checking of the size of various
   c structs we freeze/thaw.  The reason for this is to attempt
   to verify that no fields of these structs are deleted or new ones
   added when each new merge is done with the fsf.  */

#define GCC_C_STRUCTS
#define DEFCHECKSTRUCT(name, assumed_size) \
  extern void CONCAT2(check_struct_, name) PARAMS ((int));
#include "structs-to-check.def"
#undef DEFCHECKSTRUCT

#define DEFCHECKSTRUCT(name, assumed_size) CONCAT2(check_struct_, name),
static pfe_check_struct_t struct_check_functions[] =
{
#include "structs-to-check.def"
  NULL
};
#undef DEFCHECKSTRUCT

#define DEFCHECKSTRUCT(name, assumed_size)  assumed_size,
static int assumed_struct_size[] =
{
#include "structs-to-check.def"
  0
};
#undef DEFCHECKSTRUCT
#undef GCC_C_STRUCTS

/* Called from pfe_check_all_struct_sizes() to check the c struct
   sizes of the c-specific structs we freeze/thaw.  */
void
c_pfe_check_all_struct_sizes ()
{
  int i;
  
  for (i = 0; i < (int)ARRAY_SIZE (struct_check_functions) - 1; ++i)
    (*struct_check_functions[i]) (assumed_struct_size[i]);
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
  c-freeze-thaw.c -o ~/tmp.o -w 
  
#endif
