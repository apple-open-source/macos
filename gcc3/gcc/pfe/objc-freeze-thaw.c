/* APPLE LOCAL PFE */
/* Walk ObjC/ObjC++-specific structs to freeze or thaw them.
   Copyright (C) 2001  Free Software Foundation, Inc.
   Contributed by Ziemowit Laski (zlaski@apple.com)

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

#ifdef PFE  /* Should be defined via Makefile or command-line. */

#include "config.h"
#include "system.h"

#include "pfe.h"
#include "pfe-header.h"

#ifdef OBJCPLUS
#define C_OR_CXX_(func) CONCAT2 (cxx_, func)
#else
#define C_OR_CXX_(func) CONCAT2 (c_, func)
#endif
#include "objc-freeze-thaw.h"

/* APPLE LOCAL new tree dump */
#ifdef ENABLE_DMP_TREE
#include "dmp-tree.h"
extern int C_OR_CXX_(dump_tree_p)	PARAMS ((FILE *, const char *, tree, int));
extern int objc_dump_tree_p	PARAMS ((FILE *, const char *, tree, int));
extern lang_dump_tree_p_t	C_OR_CXX_(prev_lang_dump_tree_p);
extern lang_dump_tree_p_t	objc_prev_lang_dump_tree_p;
#endif

static void freeze_thaw_hash_entry     PARAMS ((hash **));
static void freeze_thaw_hash_attribute PARAMS ((attr *));

/* Initialize language specific compiler state.  */
void
objc_pfe_lang_init (lang)
  int lang ATTRIBUTE_UNUSED;
{
  /* Initialize the base language.  */
#ifdef OBJCPLUS
  cxx_pfe_lang_init ((int) PFE_LANG_OBJCXX);
#else
  c_pfe_lang_init ((int) PFE_LANG_OBJC);
#endif

  /* Initialize the language specific compiler state, and prepend it to the
     base language state.  */
  if (pfe_operation == PFE_DUMP)
  {
    struct pfe_base_lang_compiler_state *base_lang 
      = (struct pfe_base_lang_compiler_state *)pfe_compiler_state_ptr->lang_specific;
    /* Displace the lang_specific slot with our stuff...  */
    pfe_compiler_state_ptr->lang_specific  = (struct pfe_lang_compiler_state *)
                               pfe_calloc (1, sizeof (struct pfe_lang_compiler_state));
    /* ...but don't forget about the base language.  */			       
    ((struct pfe_lang_compiler_state *)pfe_compiler_state_ptr->lang_specific)->base_lang = base_lang;
  }  			       
#if 0
/* APPLE LOCAL new tree dump */
#ifdef ENABLE_DMP_TREE
  if (!objc_prev_lang_dump_tree_p)
    {
      C_OR_CXX_(prev_lang_dump_tree_p) = set_dump_tree_p (C_OR_CXX_(dump_tree_p));
      objc_prev_lang_dump_tree_p = set_dump_tree_p (objc_dump_tree_p); 
    }  
  SET_MAX_DMP_TREE_CODE (LAST_OBJC_TREE_CODE);
#endif
  if (pfe_operation == PFE_LOAD)
    { 
      add_c_tree_codes ();  
      add_objc_tree_codes ();
    }
#endif
}

/* Freeze/thaw language specific compiler state.  */
void 
objc_freeze_thaw_compiler_state (pp)
      struct pfe_lang_compiler_state **pp;
{
  struct pfe_lang_compiler_state *hdr;
  int i;
 
  hdr = (struct pfe_lang_compiler_state *)pfe_freeze_thaw_ptr_fp (pp);

  /* Freeze-thaw the state for the base language.  */
  C_OR_CXX_(freeze_thaw_compiler_state) (&hdr->base_lang);
  
  /* Now freeze-thaw our stuff.  */
  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (objc_global_trees, OCTI_MAX);
  PFE_FREEZE_THAW_GLOBAL_HASH_ENTRY (nst_method_hash_list);
  PFE_FREEZE_THAW_GLOBAL_HASH_ENTRY (cls_method_hash_list);

  pfe_freeze_thaw_objc_act_globals (hdr);
}  

/* Check language-specific compiler options.  */
void 
objc_pfe_check_settings (lang_specific)
    struct pfe_lang_compiler_state *lang_specific;
{
  C_OR_CXX_(pfe_check_settings) (lang_specific->base_lang);
}

/* See freeze-thaw.c for documentation of these routines.  */

int 
objc_pfe_freeze_thaw_decl (node)
     tree node;
{
  switch (TREE_CODE (node))
    {
      case CLASS_METHOD_DECL:
      case INSTANCE_METHOD_DECL:
        /* NB: METHOD_SEL_NAME, METHOD_SEL_ARGS, METHOD_DEFINITION
	       and METHOD_ENCODING are already handled
	       by language-independent routines.  */
	PFE_FREEZE_THAW_WALK (METHOD_ADD_ARGS (node));
	return 1;  /* No further processing needed.  */
      case KEYWORD_DECL:
        /* NB: KEYWORD_KEY_NAME and KEYWORD_ARG_NAME are already handled
	       by language-independent routines.  */
	return 1;  /* No further processing needed.  */
      default:
  	return C_OR_CXX_(pfe_freeze_thaw_decl) (node);
    }
}

int 
objc_pfe_freeze_thaw_type (node)
     tree node;
{
  switch (TREE_CODE (node))
    {
      case CLASS_INTERFACE_TYPE:
      case CLASS_IMPLEMENTATION_TYPE:
      case CATEGORY_INTERFACE_TYPE:
      case CATEGORY_IMPLEMENTATION_TYPE:
        /* NB: CLASS_NAME and CLASS_SUPER_NAME are already handled
	       by language-independent routines.  */
	PFE_FREEZE_THAW_WALK (CLASS_NST_METHODS (node));
	PFE_FREEZE_THAW_WALK (CLASS_CLS_METHODS (node));
	/* NB: TYPE_BINFO vector contains CLASS_IVARS, 
	       CLASS_RAW_IVARS, CLASS_STATIC_TEMPLATE, 
	       CLASS_CATEGORY_LIST, CLASS_PROTOCOL_LIST
	       and CLASS_OWN_IVARS.  */
	PFE_FREEZE_THAW_WALK (TYPE_BINFO (node));
	return 1;  /* No further processing needed.  */
      case PROTOCOL_INTERFACE_TYPE:
        /* NB: PROTOCOL_NAME is already handled
	       by language-independent routines.  */
	PFE_FREEZE_THAW_WALK (PROTOCOL_NST_METHODS (node));
	PFE_FREEZE_THAW_WALK (PROTOCOL_CLS_METHODS (node));
	PFE_FREEZE_THAW_WALK (TYPE_PROTOCOL_LIST (node));
	/* NB: TYPE_BINFO vector contains PROTOCOL_LIST 
	       and PROTOCOL_FORWARD_DECL.  */
	PFE_FREEZE_THAW_WALK (TYPE_BINFO (node));
	return 1;  /* No further processing needed.  */
      default:
  	return C_OR_CXX_(pfe_freeze_thaw_type) (node);
    }
}

int 
objc_pfe_freeze_thaw_special (node)
     tree node;
{
  switch (TREE_CODE (node))
    {
      default:
  	return C_OR_CXX_(pfe_freeze_thaw_special) (node);
    }
}

/*-------------------------------------------------------------------*/

/* Freeze/thaw struct hash_entry and struct hash_attribute.  */

static void
freeze_thaw_hash_entry (pp)
     hash **pp;
{
  hash *p = (hash *)PFE_FREEZE_THAW_PTR (pp);
  int pi;

  if (!p)
    return;

  for (pi = 0; pi < SIZEHASHTABLE; pi++)
    {
      hash e = (hash)PFE_FREEZE_THAW_PTR (&p[pi]);
      
      while (e) 
	{
	  PFE_FREEZE_THAW_WALK (e->key);
	  freeze_thaw_hash_attribute (&e->list);
	  e = (hash)PFE_FREEZE_THAW_PTR (&e->next);
	}
    }
}

static void
freeze_thaw_hash_attribute (pp)
     attr *pp;
{
  attr p = (attr)PFE_FREEZE_THAW_PTR (pp);

  while (p) 
    {
      PFE_FREEZE_THAW_WALK (p->value);
      p = (attr)PFE_FREEZE_THAW_PTR (&p->next);
    }
}

/*-------------------------------------------------------------------*/

/* This code below is to contol the checking of the size of various
   objc structs we freeze/thaw.  The reason for this is to attempt
   to verify that no fields of these structs are deleted or new ones
   added when each new merge is done with the fsf.  */

#define GCC_OBJC_STRUCTS
#define DEFCHECKSTRUCT(name, assumed_size) \
  extern void CONCAT2(check_struct_, name) PARAMS ((int));
#include "structs-to-check.def"
#undef DEFCHECKSTRUCT

#define DEFCHECKSTRUCT(name, assumed_size) CONCAT2(check_struct_, name),
static pfe_check_struct_t objc_struct_check_functions[] =
{
#include "structs-to-check.def"
  NULL
};
#undef DEFCHECKSTRUCT

#define DEFCHECKSTRUCT(name, assumed_size)  assumed_size,
static int objc_assumed_struct_size[] =
{
#include "structs-to-check.def"
  0
};
#undef DEFCHECKSTRUCT
#undef GCC_OBJC_STRUCTS

/* Called from pfe_check_all_struct_sizes() to check the objc struct
   sizes of the obj-specific structs we freeze/thaw.  */
void
objc_pfe_check_all_struct_sizes ()
{
  int i;
  
  C_OR_CXX_(pfe_check_all_struct_sizes) ();
  
  for (i = 0; i < (int)ARRAY_SIZE (objc_struct_check_functions) - 1; ++i)
    (*objc_struct_check_functions[i]) (objc_assumed_struct_size[i]);
}

/*-------------------------------------------------------------------*/

/* objc-act.h */
DEFINE_CHECK_STRUCT_FUNCTION (hashed_attribute)
DEFINE_CHECK_STRUCT_FUNCTION (hashed_entry)

/*-------------------------------------------------------------------*/
#endif /* PFE */
