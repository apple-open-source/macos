/* APPLE LOCAL PFE */
/* Freeze/thaw C++-specific trees and other data.
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
#include "toplev.h"
#include "c-common.h"
#include "hashtab.h"
#include "varray.h"
#include <string.h>

#include "cp/cp-tree.h"

#include "pfe.h"
#include "pfe-header.h"
#include "cp-freeze-thaw.h"

/*-------------------------------------------------------------------*/

/* Initialize language specific compiler state.  */
void
cxx_pfe_lang_init (lang)
  int lang;
{
  if (pfe_operation == PFE_DUMP)
    pfe_set_lang ((enum pfe_lang) lang == PFE_LANG_UNKNOWN ?
		  PFE_LANG_CXX : lang);
  else if (pfe_operation == PFE_LOAD)
    pfe_check_lang ((enum pfe_lang) lang == PFE_LANG_UNKNOWN ?
		    PFE_LANG_CXX : lang);
  
  /* Initialize the language specific compiler state */
  if (pfe_operation == PFE_DUMP)
    pfe_compiler_state_ptr->lang_specific  = (struct pfe_lang_compiler_state *)
      pfe_calloc (1, sizeof (struct pfe_lang_compiler_state));
}

/* Freeze/thaw language specific compiler state.  */
void 
cxx_freeze_thaw_compiler_state (pp)
     struct pfe_lang_compiler_state **pp;
{
  struct pfe_lang_compiler_state *hdr;
  int i;
 
  /* This is only called once from pfe_freeze_thaw_compiler_state()
     and only when *p is not NULL.  We cannot use PFE_FREEZE_THAW_PTR
     to freeze/thaw the pointer since we will always get NULL when
     thawing when PFE_NO_THAW_LOAD is 1.  Since we know *p is not NULL
     and since we know were only called once we don't need to use
     PFE_FREEZE_THAW_PTR anyhow.  */
     
  hdr = (struct pfe_lang_compiler_state *)pfe_freeze_thaw_ptr_fp (pp);
  
  PFE_GLOBAL_TO_HDR_IF_FREEZING (global_namespace);
  PFE_FREEZE_THAW_WALK (hdr->global_namespace);
  PFE_HDR_TO_GLOBAL_IF_THAWING (global_namespace);

  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (cp_global_trees, CPTI_MAX);

  pfe_freeze_thaw_decl2_globals (hdr);

  /* cp/decl2.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (static_ctors);
  PFE_FREEZE_THAW_WALK (hdr->static_ctors);
  PFE_HDR_TO_GLOBAL_IF_THAWING (static_ctors);

  /* cp/decl2.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (static_dtors);
  PFE_FREEZE_THAW_WALK (hdr->static_dtors);
  PFE_HDR_TO_GLOBAL_IF_THAWING (static_dtors);
  
  /* We only freeze this because we are going to validate the
     consistency of the setting of this option on a load.  */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (flag_apple_kext);

  /* cp/class.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (local_classes);
  pfe_freeze_thaw_varray_tree (&hdr->local_classes);
  PFE_HDR_TO_GLOBAL_IF_THAWING (local_classes);

  pfe_freeze_thaw_decl_globals (hdr);

  /* cp/decl.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (integer_two_node);
  PFE_FREEZE_THAW_WALK (hdr->integer_two_node);
  PFE_HDR_TO_GLOBAL_IF_THAWING (integer_two_node);

  /* cp/decl.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (integer_three_node);
  PFE_FREEZE_THAW_WALK (hdr->integer_three_node);
  PFE_HDR_TO_GLOBAL_IF_THAWING (integer_three_node);

  /* cp/decl.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (function_depth);
  PFE_HDR_TO_GLOBAL_IF_THAWING (function_depth);

  /* cp/decl.c */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (scope_chain);
  pfe_freeze_thaw_saved_scope (&hdr->scope_chain);
  PFE_HDR_TO_GLOBAL_IF_THAWING (scope_chain);
  
  /* cp/decl.c */
  if (PFE_FREEZING)
    hdr->pfe_anon_cnt = pfe_get_anon_cnt ();
  else
    pfe_set_anon_cnt (hdr->pfe_anon_cnt);

  /* cp/pt.c.  */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (processing_template_parmlist);
  PFE_HDR_TO_GLOBAL_IF_THAWING (processing_template_parmlist);

  pfe_freeze_thaw_pt_globals (hdr);

  /* cp/lex.c.  */
  for (i = 0; i < (int)LAST_CPLUS_TREE_CODE; ++i)
    {
       PFE_GLOBAL_TO_HDR_IF_FREEZING (operator_name_info[i]);
       pfe_freeze_thaw_operator_name_info (&hdr->operator_name_info[i]);
       PFE_HDR_TO_GLOBAL_IF_THAWING (operator_name_info[i]);

       PFE_GLOBAL_TO_HDR_IF_FREEZING (assignment_operator_name_info[i]);
       pfe_freeze_thaw_operator_name_info (&hdr->assignment_operator_name_info[i]);
       PFE_HDR_TO_GLOBAL_IF_THAWING (assignment_operator_name_info[i]);
    }
}

/* Check language-specific compiler options.  */
void 
cxx_pfe_check_settings (lang_specific)
    struct pfe_lang_compiler_state *lang_specific;
{
  if (pfe_operation == PFE_LOAD)
    if (lang_specific->flag_apple_kext != flag_apple_kext)
      fatal_error ("Inconsistent setting of -fapple-kext on pre-compiled header dump and load.");
}

/*-------------------------------------------------------------------*/

/* See freeze-thaw.c for documentation of these routines.  */

/* Handle 'd' nodes */
int 
cxx_pfe_freeze_thaw_decl (node)
     tree node;
{
  struct lang_decl *ld = DECL_LANG_SPECIFIC (node);
  
  if (ld
      && !DECL_GLOBAL_CTOR_P (node)
      && !DECL_GLOBAL_DTOR_P (node)
      && !DECL_THUNK_P (node)
      && !DECL_DISCRIMINATOR_P (node))
      PFE_FREEZE_THAW_WALK (DECL_ACCESS (node));

  switch (TREE_CODE (node))
    {
      case NAMESPACE_DECL:
        if (ld) 
	  pfe_freeze_thaw_binding_level (&NAMESPACE_LEVEL (node));
      	PFE_FREEZE_THAW_WALK (DECL_NAMESPACE_USING (node));
      	PFE_FREEZE_THAW_WALK (DECL_NAMESPACE_USERS (node));
      	return 1;
      
      case TYPE_DECL:
        if (ld)
          {
      	    PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INFO (node));
      	    PFE_FREEZE_THAW_WALK (DECL_SORTED_FIELDS (node)); /* NEEDED? */
      	  }
        return 0; /* let normal processing continue */
      	  
      case FUNCTION_DECL:
        if (ld)
          {
      	    PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INFO (node));
      	    if (DECL_THUNK_P (node))
      	      PFE_FREEZE_THAW_WALK (THUNK_VCALL_OFFSET (node));
      	    PFE_FREEZE_THAW_WALK (DECL_BEFRIENDING_CLASSES (node));
      	    PFE_FREEZE_THAW_WALK (ld->context);
      	    if (DECL_CLONED_FUNCTION_P (node))
      	      PFE_FREEZE_THAW_WALK (DECL_CLONED_FUNCTION (node));
      	    if (DECL_PENDING_INLINE_P (node))
      	      pfe_freeze_thaw_unparsed_text (&DECL_PENDING_INLINE_INFO (node));
      	    else
      	      pfe_freeze_thaw_language_function
      	      	((struct language_function **)&DECL_SAVED_FUNCTION_DATA (node));
      	  }
        return 0; /* let normal processing continue */

      case VAR_DECL:
        if (ld)
      	  PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INFO (node));
        return 0; /* let normal processing continue */

      case TEMPLATE_DECL:
        PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_PARMS (node));
        PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INSTANTIATIONS (node));
        PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_SPECIALIZATIONS (node));
        if (ld)
      	  {
            if (DECL_TEMPLATE_INFO (node))
              {
                PFE_FREEZE_THAW_WALK (DECL_TI_TEMPLATE (node));
                PFE_FREEZE_THAW_WALK (DECL_TI_ARGS (node));
              }
      	    PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INFO (node));
      	  }
      	PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_RESULT (node));
        PFE_FREEZE_THAW_WALK (DECL_TEMPLATE_INSTANTIATIONS (node));
        PFE_FREEZE_THAW_WALK (DECL_INITIAL (node));
      	PFE_FREEZE_THAW_WALK (DECL_BEFRIENDING_CLASSES (node));
      	if (DECL_CLONED_FUNCTION_P (node))
      	  PFE_FREEZE_THAW_WALK (DECL_CLONED_FUNCTION (node));
        return 0; /* let normal processing continue */
      
      default:
	return 0; /* let normal processing continue */
    }
}

/* Handle 't' nodes */
int 
cxx_pfe_freeze_thaw_type (node)
     tree node;
{
  /* The qualification on the following code is patterned after the
     test done in cp/decl.c:lang_mark_tree().  */
     
  if (TYPE_LANG_SPECIFIC (node) &&
      !(TREE_CODE (node) == POINTER_TYPE
	&& TREE_CODE (TREE_TYPE (node)) == METHOD_TYPE))
    {
      PFE_FREEZE_THAW_WALK (CLASSTYPE_PRIMARY_BINFO (node)); 		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_VFIELDS (node));			/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_VBASECLASSES (node));		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_TAGS (node));			/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_SIZE (node));			/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_SIZE_UNIT (node));		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_PURE_VIRTUALS (node));		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_FRIEND_CLASSES (node));		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_RTTI (node));			/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_METHOD_VEC (node));		/* NEEDED? */
      if (TREE_CODE (node) == RECORD_TYPE
          || TREE_CODE (node) == UNION_TYPE)
        PFE_FREEZE_THAW_WALK (CLASSTYPE_TEMPLATE_INFO (node));		/* NEEDED? */
      PFE_FREEZE_THAW_WALK (CLASSTYPE_BEFRIENDING_CLASSES (node));	/* NEEDED? */
    }
  
  switch (TREE_CODE (node))
    {      
      case TEMPLATE_TYPE_PARM:
         PFE_FREEZE_THAW_WALK (TEMPLATE_TYPE_PARM_INDEX (node));
         return 1;
   
      case FUNCTION_TYPE:
      case METHOD_TYPE:
         PFE_FREEZE_THAW_WALK (TYPE_RAISES_EXCEPTIONS (node));
         return 0;

      case TYPENAME_TYPE:
         PFE_FREEZE_THAW_WALK (TYPE_CONTEXT (node));
         PFE_FREEZE_THAW_WALK (TYPE_NAME (node));
         PFE_FREEZE_THAW_WALK (TYPENAME_TYPE_FULLNAME (node));
         PFE_FREEZE_THAW_WALK (TREE_TYPE (node));
         return 0; /* let normal processing continue */

      case TEMPLATE_TEMPLATE_PARM:
         PFE_FREEZE_THAW_WALK (TYPE_NAME (node));
         PFE_FREEZE_THAW_WALK (TEMPLATE_TYPE_PARM_INDEX (node));
         return 0; /* let normal processing continue */

      case BOUND_TEMPLATE_TEMPLATE_PARM:
         PFE_FREEZE_THAW_WALK (TEMPLATE_TYPE_PARM_INDEX (node));
         PFE_FREEZE_THAW_WALK (TEMPLATE_TEMPLATE_PARM_TEMPLATE_INFO (node));
         PFE_FREEZE_THAW_WALK (TYPE_NAME (node));
         if (PFE_FREEZING)
	   {
	    if (!PFE_IS_FROZEN (TYPE_TEMPLATE_INFO(node)))
	       {
		 PFE_FREEZE_THAW_WALK (TYPE_TI_TEMPLATE (node));
		 PFE_FREEZE_THAW_WALK (TYPE_TI_ARGS (node));
	       }
           }
         else if (PFE_IS_FROZEN (TYPE_TEMPLATE_INFO(node)))
           {
             PFE_FREEZE_THAW_WALK (TYPE_TI_TEMPLATE (node));
             PFE_FREEZE_THAW_WALK (TYPE_TI_ARGS (node));
           }
         return 0; /* let normal processing continue */

      case TYPEOF_TYPE:
         PFE_FREEZE_THAW_WALK (TYPE_FIELDS (node));
         return 0; /* let normal processing continue */

      case ENUMERAL_TYPE:
         PFE_FREEZE_THAW_WALK (ENUM_TEMPLATE_INFO (node));
         return 0; /* let normal processing continue */

      default:
  	 return 0; /* let normal processing continue */
    }
  
  return 0; /* let normal processing continue */
}

/* Handle 'c' and 'x' nodes */
int 
cxx_pfe_freeze_thaw_special (node)
     tree node;
{
  switch (TREE_CODE (node))
    {
      case IDENTIFIER_NODE:
	PFE_FREEZE_THAW_WALK (IDENTIFIER_NAMESPACE_BINDINGS (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_CLASS_VALUE (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_BINDING (node));
	PFE_FREEZE_THAW_WALK (IDENTIFIER_TEMPLATE (node));
	PFE_FREEZE_THAW_WALK (REAL_IDENTIFIER_TYPE_VALUE (node));
	
	/* The PFE_FREEZE_THAW_WALK() macro is defined to take the 
	   address of its argument.  IDENTIFIER_LABEL_VALUE,
	   IDENTIFIER_IMPLICIT_DECL, and IDENTIFIER_ERROR_LOCUS 
	   all use LANG_ID_FIELD to access their field.  But
	   LANG_ID_FIELD is defined as a ?: operation so we 
	   cannot just use the accessor macros as a argument to
	   PFE_FREEZE_THAW_WALK.  Thus we need to do what LANG_ID_FIELD
	   is trying to do explicitly here.  Sigh :-(  */
	   
	if (LANG_IDENTIFIER_CAST (node)->x)
	  {
	    PFE_FREEZE_THAW_WALK (LANG_IDENTIFIER_CAST (node)->x->label_value);
	    PFE_FREEZE_THAW_WALK (LANG_IDENTIFIER_CAST (node)->x->implicit_decl);
	    PFE_FREEZE_THAW_WALK (LANG_IDENTIFIER_CAST (node)->x->error_locus);
	  }
  	 return 0; /* let normal processing continue */
       
       case TEMPLATE_PARM_INDEX:
         PFE_FREEZE_THAW_WALK (TEMPLATE_PARM_DECL (node));	/* NEEDED? */
         PFE_FREEZE_THAW_WALK (TEMPLATE_PARM_DESCENDANTS (node)); /* NEEDED? */
         return 1;
       
       case PTRMEM_CST:
         PFE_FREEZE_THAW_WALK (PTRMEM_CST_MEMBER (node));	/* NEEDED? */
         return 1;
       
       case CPLUS_BINDING:
         if (BINDING_HAS_LEVEL_P (node))
           pfe_freeze_thaw_binding_level (&(((struct tree_binding*)node)->scope.level));
         else
           PFE_FREEZE_THAW_WALK (BINDING_SCOPE (node));		/* NEEDED? */
         PFE_FREEZE_THAW_WALK (BINDING_VALUE (node));		/* NEEDED? */
         PFE_FREEZE_THAW_WALK (TREE_CHAIN (node));
         return 1;
       
       case OVERLOAD:
         PFE_FREEZE_THAW_WALK (OVL_FUNCTION (node));		/* NEEDED? */
         PFE_FREEZE_THAW_WALK (OVL_CHAIN (node));		/* NEEDED? */
         return 1;
        
       case WRAPPER:
         /* FIXME?:ÊHOW DO I HANDLE WRAPPER_PTR VS. WRAPPER_INT ? */
         /* Currently it appears that WRAPPER_INT is never used in C++
            nor is routine that creates it -- build_int_wrapper().
            Therefore I will unconditionally always assume we have
            the pointer variant.  */
         pfe_freeze_thaw_ptr_fp (&WRAPPER_PTR (node));
         return 1;
       
       case SRCLOC:
         pfe_freeze_thaw_ptr_fp (&SRCLOC_FILE (node));
         return 1;
         
       default:
  	 return 0; /* let normal processing continue */
   }
}

/*-------------------------------------------------------------------*/

/* The routines below here are all to handle freezing/thawing of data
   specific to C++ that are not trees (although tree fields could
   point to this stuff).  */

/* Freeze/thaw cp_language_function defined in cp/cp-tree.h */
void 
pfe_freeze_thaw_language_function (pp)
     struct language_function **pp;
{
  tree *node;
  struct cp_language_function *p;
  
  p = (struct cp_language_function *)PFE_FREEZE_THAW_PTR (pp);
  if (!p)
    return;
    
  pfe_freeze_thaw_common_language_function (&p->base);
  PFE_FREEZE_THAW_WALK (p->x_dtor_label);
  PFE_FREEZE_THAW_WALK (p->x_current_class_ptr);
  PFE_FREEZE_THAW_WALK (p->x_current_class_ref);
  PFE_FREEZE_THAW_WALK (p->x_eh_spec_block);
  PFE_FREEZE_THAW_WALK (p->x_in_charge_parm);
  PFE_FREEZE_THAW_WALK (p->x_vtt_parm);
  PFE_FREEZE_THAW_WALK (p->x_return_value);
  
  node = (tree *)PFE_FREEZE_THAW_PTR (&p->x_vcalls_possible_p);
  PFE_FREEZE_THAW_WALK (*node);
  
  pfe_freeze_thaw_named_label_use_list (&p->x_named_label_uses);
  pfe_freeze_thaw_named_label_list (&p->x_named_labels);
  pfe_freeze_thaw_binding_level (&p->bindings);
  pfe_freeze_thaw_varray_tree (&p->x_local_names);
  pfe_freeze_thaw_ptr_fp (&p->cannot_inline);
}

/* Freeze/thaw saved_scope defined in cp/cp-tree.h */
void
pfe_freeze_thaw_saved_scope (pp)
     struct saved_scope **pp;
{
  struct saved_scope *p = (struct saved_scope *)PFE_FREEZE_THAW_PTR (pp);
  if (!p)
    return;

  PFE_FREEZE_THAW_WALK (p->old_bindings);
  PFE_FREEZE_THAW_WALK (p->old_namespace);
  PFE_FREEZE_THAW_WALK (p->decl_ns_list);
  PFE_FREEZE_THAW_WALK (p->class_name);
  PFE_FREEZE_THAW_WALK (p->class_type);
  PFE_FREEZE_THAW_WALK (p->access_specifier);
  PFE_FREEZE_THAW_WALK (p->function_decl);
  pfe_freeze_thaw_varray_tree (&p->lang_base);
  PFE_FREEZE_THAW_WALK (p->lang_name);
  PFE_FREEZE_THAW_WALK (p->template_parms);
  PFE_FREEZE_THAW_WALK (p->x_previous_class_type);
  PFE_FREEZE_THAW_WALK (p->x_previous_class_values);
  PFE_FREEZE_THAW_WALK (p->x_saved_tree);
  PFE_FREEZE_THAW_WALK (p->lookups);
  /* struct stmt_tree_s x_stmt_tree; Needed ??? */
  pfe_freeze_thaw_binding_level (&p->class_bindings);
  pfe_freeze_thaw_binding_level (&p->bindings);

  pfe_freeze_thaw_saved_scope (&p->prev);
 
}

/* Freeze/thaw operator_name_info arrays from lex.c  */
void
pfe_freeze_thaw_operator_name_info (p)
     struct operator_name_info_t *p;
{
  if (!p)
    return;

  PFE_FREEZE_THAW_WALK (p->identifier);
  pfe_freeze_thaw_ptr_fp (&p->name);
  pfe_freeze_thaw_ptr_fp (&p->mangled_name);
}

/*-------------------------------------------------------------------*/

/* This code below is to contol the checking of the size of various
   cp structs we freeze/thaw.  The reason for this is to attempt
   to verify that no fields of these structs are deleted or new ones
   added when each new merge is done with the fsf.  */

#define GCC_CP_STRUCTS
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
#undef GCC_CP_STRUCTS

/* Called from pfe_check_all_struct_sizes() to check the cp struct
   sizes of the cxx-specific structs we freeze/thaw.  */
void 
cxx_pfe_check_all_struct_sizes ()
{
  int i;
  
  for (i = 0; i < (int)ARRAY_SIZE (struct_check_functions) - 1; ++i)
    (*struct_check_functions[i]) (assumed_struct_size[i]);
}

/*-------------------------------------------------------------------*/

DEFINE_CHECK_STRUCT_FUNCTION (cp_language_function)
DEFINE_CHECK_STRUCT_FUNCTION (saved_scope)
DEFINE_CHECK_STRUCT_FUNCTION (operator_name_info_t)

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
  cp-freeze-thaw.c -o ~/tmp.o -w 
  
#endif
