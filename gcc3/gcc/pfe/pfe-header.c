/* APPLE LOCAL PFE */
/* Persistent Front End (PFE) for the GNU compiler.
   Copyright (C) 2001
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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
#include "toplev.h"

#include <stdio.h>
#include <assert.h>

#ifdef PFE

#include "pfe.h"
#include "pfe-header.h"

#include "tm_p.h"
#include "langhooks.h"

struct obstack;
struct _obstack_chunk;

/* List of all predefined names, i.e., global_binding_level->names
   before any source has been parsed.  This is set during a dump
   by compile_file() and saved in the dump file.  It is restored
   by a load and used to temporarily replace global_binding_level->names
   during compile_file()'s initialization.  That way the names list
   looks the same as it did during that time when it was dumped.
   If we didn't do this then global_binding_level->names would
   be a list of ALL globals (predefined plust the globals created
   for the dump file) since the previous dump was done at the end of
   compilation after all symbols have been seen.  
   
   FIXME: Eventually remove when we settle on how dbxout_init()
   is going to behave with respect to the globals.  */
tree pfe_predefined_global_names = NULL;

extern struct cpp_reader* parse_in;  
extern int const_labelno;
extern int var_labelno;
extern int pfe_display_precomp_headers;

/* Ptr to function to freeze/thaw language specific compiler state or NULL.  */
void (*pfe_lang_freeze_thaw_compiler_state) PARAMS ((struct pfe_lang_compiler_state **));

static void pfe_freeze_thaw_include_header  PARAMS ((struct ht_identifier **));
static void pfe_check_format		    PARAMS ((pfe_compiler_state *));
static void pfe_check_compiler_version      PARAMS ((void));
static void pfe_freeze_thaw_compiler_state  PARAMS ((pfe_compiler_state *));
static void pfe_freeze_thaw_hashtable       PARAMS ((struct ht **, void (*) (struct ht_identifier **)));
static void pfe_freeze_thaw_ht_identifier   PARAMS ((struct ht_identifier **));
static void pfe_freeze_thaw_obstack_chunk   PARAMS ((struct _obstack_chunk **));
static void pfe_freeze_thaw_obstack 	    PARAMS ((struct obstack *));
static void pfe_check_compiler_settings	    PARAMS ((void));
void pfe_check_cmd_ln_macros	            PARAMS ((void));
static char * pfe_absolute_path_name        PARAMS ((char *));

/*-------------------------------------------------------------------*/

/* Set the language. It is used to verify during the load.  */
void
pfe_set_lang (lang)
     enum pfe_lang lang;
{
  pfe_compiler_state_ptr->lang = lang;
}

/* Check the language of a pre-compiled header file to see if it is
   compatible with the current version of the compiler.  */
void
pfe_check_lang (lang)
     enum pfe_lang lang;
{
  if (pfe_compiler_state_ptr->lang != lang)
    fatal_error ("Pre-compiled header for wrong language: \"%s\".", pfe_name);
}

/* Freeze/thaw struct pfe_include_header.  
   Note that this routine requires that the identifier string is part 
   of a struct ht_identifier which must be the first field of a 
   struct pfe_include_header.  */
static void
pfe_freeze_thaw_include_header (pp)
     struct ht_identifier **pp;
{
  struct pfe_include_header *p = (struct pfe_include_header *)PFE_FREEZE_THAW_PTR (pp);
  
  if (!p)
    return;
    
  pfe_freeze_thaw_ptr_fp (&p->include_name.str);
}

/* Determine whether the include/import file has already been included
   in a pre-compiled header.  If it has already been load return 1, 
   else return 0.  */
int
pfe_check_header (fname, timestamp, inode)
     const char *fname;
     time_t timestamp;
     ino_t inode;
{
  pfe_include_header *header;
  char *full_name;

  if (!fname)
    return 0;

  full_name = pfe_absolute_path_name (fname);
  /* Determine if it is already loaded as a part of pre-compiled 
     header.  */
  header = (pfe_include_header *) ht_lookup (pfe_compiler_state_ptr->include_hash, 
		       (const unsigned char *) full_name, 
                       strlen (full_name), HT_NO_INSERT);
  if (!header)
    {
      if (pfe_display_precomp_headers)
	fprintf (stderr, "PFE: full_name = %s is not available in precompiled header.\n", full_name);
      pfe_free (full_name);
      return 0;
    }

  /* If found name but its not for the expecterd inode then we assume
      it is not a match.  */
  if (header->inode != inode)
    {
      error ("Precompiled header is invalid; header file %s is now different.", full_name);
      pfe_free (full_name);
      return 0;
    }
  
  /* Both name and inode match so we assume this header is in the PFE
     file.  But it still could be a newer version than what we have in the
     PFE file.  */
  if (timestamp > header->timestamp)
    {
      error ("Precompiled header is out of date; header file %s is newer.", full_name);
      pfe_free (full_name);
      return 0;
    }
    
  if (pfe_display_precomp_headers)
    fprintf (stderr, "PFE: full_name = %s is loaded from precompiled header.\n", full_name);
  
  pfe_free (full_name);
  return 1;
}

/* Add the include/import in the list of headers included in current
   precompiled header.  */
void 
pfe_add_header_name (fname, timestamp, inode)
     const char *fname;
     time_t timestamp;
     ino_t inode;
{  
  pfe_include_header *header;
  char *full_name;

  if (!fname)
    return;

  full_name = pfe_absolute_path_name (fname);
  /* Include header list is updated only during PFE_DUMP.  */
  if (pfe_operation != PFE_DUMP)
    fatal_error ("pfe_add_header_name can be used only with PFE_DUMP\n");

  header = (pfe_include_header *) ht_lookup (pfe_compiler_state_ptr->include_hash, 
					     (const unsigned char *) full_name,
					     strlen (full_name), HT_ALLOC);
  header->inode     = inode;
  header->timestamp = timestamp;

  if (pfe_display_precomp_headers)
    fprintf (stderr, "PFE: full_name = %s is added in precompiled header.\n", full_name);

  pfe_free(full_name);
}  

/* Allocates memory. 
   Return absolute path name for the given input filename.  */
static char *
pfe_absolute_path_name (input_name)
     char *input_name;
{   
  char *name;
  if (input_name[0] != '/') 
    {
      /* Append current pwd. We need absolute path.  */
      int alen = MAXPATHLEN + strlen (input_name) + 2;
      name = (char *) pfe_malloc (sizeof (char) * alen);
      name = getcwd(name, alen);
      strcat (name, "/");
      strcat (name, input_name);
    }
  else
    {
      name = (char *) pfe_malloc (strlen (input_name) + 1);
      strcpy (name, input_name);
    }
  return name;
}   

/* Check the format of a pre-compiled header file to see if it is
   compatible with the current version of the compiler.  */
   
static void
pfe_check_format (hdr)
     pfe_compiler_state *hdr;
{
  if (hdr->magic_number != PFE_MAGIC_NUMBER)
    fatal_error ("Unrecognizable format in pre-compiled header: \"%s\".", pfe_name);
  if (hdr->format_version != PFE_FORMAT_VERSION)
    fatal_error ("Incompatible format version pre-compiled header: \"%s\".", pfe_name);
}

/* Check compiler version */
static void
pfe_check_compiler_version ()
{
  int valid_compiler;
  char current_version [16];

  strncpy (current_version, apple_version_str, 15);

  valid_compiler = (strcmp (pfe_compiler_state_ptr->compiler_version,
		 	   current_version) == 0);
  
  if (!valid_compiler)
    {
      /* Compiler versions are not identical.
	 Now is the time to check the range of valid versions.
	 It depends on the gcc3 versioning scheme, which is
	 not yet finalized. FIXME */
    }

  if (!valid_compiler)
    fatal_error ("Incompatible compiler version in pre-compiled header: \"%s\".",
    		 pfe_name);
}

/* Check compiler settings */
static void
pfe_check_compiler_settings ()
{
  /* Check language specific compiler settings.  Note that we pass the
     pointer to the lang_specific data since it's the settings in there
     we want to check.  */
  (*lang_hooks.pfe_check_settings) (pfe_compiler_state_ptr->lang_specific);
#ifdef PFE_CHECK_TARGET_SETTINGS
  PFE_CHECK_TARGET_SETTINGS ();
#endif
}

/* Check command line macros */
void
pfe_check_cmd_ln_macros ()
{
  /* INCOMPLETE : FIXME */
}

/* Validate the pre-compiled header. This function is called after the 
   command line options have been processed.
   - Check language and language flavors.
   - Check command line options
*/
void
pfe_check_compiler ()
{
  /* Check that the pre-compiled header was built using the compatible
     compiler.  */
  pfe_check_compiler_version ();

  /* Check that the pre-compiled header was built using the same 
     language dialect as the compiler being run.  */
  //pfe_check_lang ();  now done by <lang>_pfe_lang_init

  /* Make sure that appropriate compiler settings are consistent.  */
  pfe_check_compiler_settings ();

  /* Check command line macros which were defined (and undefined) 
     when the pre-compiled header was built are consistent with 
     current macro definitions.  */
  pfe_check_cmd_ln_macros ();
}

/* Save and "freeze" the compiler state by copying global variable
   values into the compiler state header.  All pointers should be
   "frozen" with a call to pfe_freeze_<type> of the appropriate type.

   Note, this is called near the end of compilation to save the
   global compiler state prior to dumping the file to the disk.  The
   calling sequence is as follows:
   
     compile_file() -->
       pfe_dump_compiler_state() [via pfe_dump()] -->
         pfe_freeze_compiler_state()
*/
void
pfe_freeze_compiler_state (hdr)
     void *hdr;
{
  ((pfe_compiler_state *)hdr)->magic_number   = PFE_MAGIC_NUMBER;
  ((pfe_compiler_state *)hdr)->format_version = PFE_FORMAT_VERSION;
  strncpy (((pfe_compiler_state *)hdr)->compiler_version,
	   apple_version_str, 15);
   
  pfe_freeze_thaw_compiler_state ((pfe_compiler_state *)hdr);
}

/* Restore and "thaw" the compiler state by copying values from the
   compiler state header back to the appropriate global variables.  
   All pointers should be "thawed" with a call to pfe_thaw_<type> of 
   the appropriate type.

   Note, this is called near the start of compilation to load the
   global compiler state prior to compiling the file.  The
   calling sequence is as follows:
   
     -fload=file detected (decode_f_option) -->
       pfe_load_compiler_state() -->
	 pfe_thaw_compiler_state()
*/
void
pfe_thaw_compiler_state (hdr)
     pfe_compiler_state *hdr;
{
  pfe_check_format (hdr);
  
  /* Init language specific compiler state (and check language).  */
  (*lang_hooks.pfe_lang_init) (0);
  
  pfe_freeze_thaw_compiler_state (hdr);
}

extern tree machopic_non_lazy_pointers;
extern tree machopic_stubs;

/* Common code for pfe_freeze_compiler_state() and pfe_thaw_compiler_state().
   This handles all the common state data that is always frozen or thawed.
   "Common" here means stuff that is common to c, c++, and objc.   */
static void
pfe_freeze_thaw_compiler_state (hdr)
    pfe_compiler_state *hdr;
{
  int i, n;
  
  /* hdr->progname set during initialization.  */
  /* The hdr->progname is only used for verification. */
  pfe_freeze_thaw_ptr_fp (&hdr->progname);
  
  PFE_GLOBAL_TO_HDR_IF_FREEZING (pfe_predefined_global_names);
  pfe_freeze_thaw_ptr_fp (&hdr->pfe_predefined_global_names);
  PFE_HDR_TO_GLOBAL_IF_THAWING (pfe_predefined_global_names);

  //pfe_freeze_thaw_tree_walk (&hdr->global_namespace);
  pfe_freeze_thaw_hashtable (&hdr->include_hash, pfe_freeze_thaw_include_header);
  
  /* Identifier hash table.  Note that the elements of this table are
     cpp_hashnode's (which begin with ht_identifier's).  */
  pfe_freeze_thaw_binding_level (NULL);
  PFE_GLOBAL_TO_HDR_IF_FREEZING (ident_hash);
  pfe_freeze_thaw_hashtable (&hdr->ident_hash, 
  			     (void (*) (struct ht_identifier **))
  			     pfe_freeze_thaw_cpp_hashnode);
  PFE_HDR_TO_GLOBAL_IF_THAWING (ident_hash);
  
  /* Root for all the declarations.  Note that global_binding_level
     is static so that pfe_freeze_thaw_binding_level() handles the
     moving of global_binding_level to and from the pfe header.  */
  //pfe_freeze_thaw_binding_level (NULL);
 
  /* Restore global trees */
  /* Save global trees (tree.c) */
  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (global_trees, TI_MAX);
 
  /* Save global tree nodes used to represent various integer types. (tree.c) */
  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (integer_types, itk_none);

  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (sizetype_tab, TYPE_KIND_LAST);

  /* Global trees from c-common.c.  */
  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (c_global_trees, CTI_MAX);
  
  /* __builtin_xxxx's from builtins.def  */
#if 0 /* built_in_names needed because it appears to be constant */
  if (PFE_FREEZING)
    memcpy (hdr->built_in_names, built_in_names,
  	    (int)(END_BUILTINS) * sizeof (char *));
  for (i = 0; i < (int)(END_BUILTINS); ++i)
    PFE_FREEZE_THAW_PTR (&hdr->built_in_names[i]);
  if (PFE_THAWING)
    memcpy (built_in_names, hdr->built_in_names,
  	    (int)(END_BUILTINS) * sizeof (char *));
#endif
  PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY (built_in_decls, END_BUILTINS);
  
  /* rtl globals from rtl.h.  */
  PFE_FREEZE_THAW_GLOBAL_RTX_ARRAY (global_rtl, GR_MAX);
  n = MAX_SAVED_CONST_INT * 2 + 1;
  PFE_FREEZE_THAW_GLOBAL_RTX_ARRAY (const_int_rtx, n);
  for (n = 0; n <= 2; ++n)
    PFE_FREEZE_THAW_GLOBAL_RTX_ARRAY (const_tiny_rtx[n], MAX_MACHINE_MODE);

  /* Globals from varasm.c.  */
  PFE_GLOBAL_TO_HDR_IF_FREEZING (const_labelno);
  PFE_HDR_TO_GLOBAL_IF_THAWING (const_labelno);
  PFE_GLOBAL_TO_HDR_IF_FREEZING (var_labelno);
  PFE_HDR_TO_GLOBAL_IF_THAWING (var_labelno);

  PFE_FREEZE_THAW_GLOBAL_RTX (const_true_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (return_address_pointer_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (struct_value_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (struct_value_incoming_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (static_chain_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (static_chain_incoming_rtx);
  PFE_FREEZE_THAW_GLOBAL_RTX (pic_offset_table_rtx);

  /* Handle globals from emit-rtl.c.  */
  pfe_freeze_thaw_emitrtl (hdr);
    
  /* Garbage collector globals in ggc-page.c.  */
  pfe_freeze_thaw_ggc (&hdr->ggc_globals);
  
  /* Handle all dbxout data from within dbxout.c.  */
  pfe_freeze_thaw_dbxout (hdr);
  
#ifdef PFE_TARGET_ADDITIONS
  /* Handle target-specific additions to the header.  */
  if (hdr->pfe_target_additions)
    PFE_TARGET_ADDITIONS (&hdr->pfe_target_additions);
#endif

  /* Handle language-specific compiler state information.  */
  if (hdr->lang_specific)
    (*lang_hooks.pfe_freeze_thaw_compiler_state) (&hdr->lang_specific);
  
#if PFE_NEW_TREE_WALK
  /* Do the actual freezing/thawing of tree nodes (and their
     descendants) that have been reached from the various "roots"
     above.  Caution, no more tree freeze/thawing beyond this
     point.  */
  pfe_freeze_thaw_tree_walk ();
#endif
}

/*-------------------------------------------------------------------*/

/* Freeze/thaw a pointer to and a cpp_hashnode struct (defined in 
   cpplib.h).  */
void 
pfe_freeze_thaw_cpp_hashnode (hnp)
  struct cpp_hashnode **hnp;
{
  /* We save a copy of original cpp_hashnode pointer, which, on the
     load side of things will be frozen (at least the first time we
     reach this structure).  We need this frozen pointer to create
     the temp pointer we will use to thaw the embedded ht_identifier.
     On the load size, we need the still-frozen pointer for the temp
     pointer, because if it is not frozen, the routine to thaw the
     ht_identifier will think that it has already been thawed.  */
  struct cpp_hashnode *hn_orig = *hnp;
  struct cpp_hashnode *hn = PFE_FREEZE_THAW_PTR (hnp);
  struct ht_identifier *temp_ident;
  tree		       temp_tree;
  
  if (!hn)
    return;
  
#if 0
  /* ??? FF_diagnostic  */
  if (pfe_operation == PFE_DUMP)
    {
      printf ("pfe_freeze_thaw_cpp_hashnode: 0x%x\n", (unsigned int)hn);
      if (hn->ident.str != NULL)
        printf ("  hn->ident.str: 0x%x '%s'\n", 
        	(unsigned int)hn->ident.str,
        	pfe_real_ptr(hn->ident.str));
      if (hn->type == NT_MACRO)
        printf ("  hn->type: NT_MACRO\n");
      else if (hn->type == NT_VOID)
        printf ("  hn->type: NT_VOID\n");
      else
        printf ("  hn->type: ???\n");
      if (hn->value.macro != NULL)
        printf ("  hn->value.macro: 0x%x\n", (unsigned int)hn->value.macro);
    }
#endif
  
#if 0
  /* If the first field has been frozen, then return because the
     whole structure is either frozen or in the process of being
     frozen.  */
  /* FIXME: We may want this optimization.  */
  if (PFE_IS_FROZEN (hn->ident.str))
    return;
#endif
  if (pfe_operation == PFE_DUMP && hn->type == NT_MACRO)
    {
      if (! ustrncmp (pfe_real_ptr (hn->ident.str), DSC ("__STDC_")))
        {
          if (hn->flags & NODE_WARN)
            hn->flags ^= NODE_WARN;
          else
            hn->flags &= NODE_WARN;
        }
    }
  
  pfe_using_temp_ptr ((void **)&temp_ident);
  temp_ident = (struct ht_identifier *) hn_orig;
  pfe_freeze_thaw_ht_identifier (&temp_ident);
  
  switch (hn->type) {
    case NT_VOID:
      break;
    case NT_MACRO:
      if (! (hn->flags & NODE_BUILTIN))
        pfe_freeze_thaw_cpp_macro (&hn->value.macro);
      break;
    case NT_ASSERTION:
      pfe_freeze_thaw_answer (&hn->value.answers);
      break;
  }
  
  /* Make sure the IDENTIFIER_NODE is also frozen/thawed.  */
  pfe_using_temp_ptr ((void **)&temp_tree);
  temp_tree = HT_IDENT_TO_GCC_IDENT (hn);
  PFE_FREEZE_THAW_WALK (temp_tree);
  
#if 0
  /* ??? FF_diagnostic  */
  if (pfe_operation == PFE_LOAD)
    {
      printf ("pfe_freeze_thaw_cpp_hashnode: 0x%x\n", (unsigned)hn);
      if (hn->ident.str != NULL)
        printf ("  hn->ident.str: 0x%x '%s'\n", 
        	(unsigned int)hn->ident.str,
        	pfe_real_ptr(hn->ident.str));
      if (hn->type == NT_MACRO)
        printf ("  hn->type: NT_MACRO\n");
      else if (hn->type == NT_VOID)
        printf ("  hn->type: NT_VOID\n");
      else
        printf ("  hn->type: ???\n");
      if (hn->value.macro != NULL)
        printf ("  hn->value.macro: 0x%x\n", (unsigned)hn->value.macro);
    }
#endif
}

/* The routines below here are all to handle freezing/thawing of data
   common to all languages that are not trees (although tree fields
   could point to this stuff).  */

/* Freeze/thaw an obstack_chunk (obstack.h) and all of the chunks in
   its "prev" list.  */
static void
pfe_freeze_thaw_obstack_chunk (pp)
     struct _obstack_chunk **pp;
{
  struct _obstack_chunk *p = (struct _obstack_chunk *)PFE_FREEZE_THAW_PTR (pp);

  /* Freeze/thaw this chunk and any chunks it might point to 
     (avoiding unneeded recursion).  */
  while (p)
    {
      PFE_FREEZE_THAW_PTR_WITH_VARIANCE (&p->limit, 1);
      p = PFE_FREEZE_THAW_PTR (&p->prev);
     /* The "contents" of the chunk are located at the end of the 
        chunk so nothing needs to be done to freeze or thaw them.  */
    }
}

/* Freeze/thaw an obstack.  */
static void
pfe_freeze_thaw_obstack (p)
     struct obstack *p;
{
  pfe_freeze_thaw_obstack_chunk (&p->chunk);
  pfe_freeze_thaw_ptr_fp (&p->object_base);
  pfe_freeze_thaw_ptr_fp (&p->next_free);
  PFE_FREEZE_THAW_PTR_WITH_VARIANCE (&p->chunk_limit, 1);
  
  /* When loading, restore the function pointers used to allocate
     and free obstack memory.  */
  if (PFE_THAWING)
    {
      p->chunkfun = (struct _obstack_chunk *(*)(void *, long)) pfe_malloc;
      p->freefun  = (void (*) (void *, struct _obstack_chunk *))pfe_free;
    }
}

/* Freeze/thaw struct ht_identifier.  */
static void
pfe_freeze_thaw_ht_identifier (pp)
     struct ht_identifier **pp;
{
  struct ht_identifier *p = (struct ht_identifier *)PFE_FREEZE_THAW_PTR (pp);
  
  if (!p)
    return;
    
#if 0
  /* ??? FF_diagnostic  */
  if (pfe_operation == PFE_DUMP && !PFE_IS_FROZEN(p->str))
    printf ("pfe_freeze_thaw_ht_identifier: 0x%x '%s'\n", 
    	    (unsigned)p, pfe_real_ptr(p->str));
#endif

  pfe_freeze_thaw_ptr_fp (&p->str);

#if 0
  /* ??? FF_diagnostic  */
  if (pfe_operation == PFE_LOAD)
    printf ("pfe_freeze_thaw_ht_identifier: 0x%x '%s'\n", 
    	    (unsigned)p, pfe_real_ptr(p->str));
#endif
}

/* Freeze/thaw struct ht or hash_table.  */
static void
pfe_freeze_thaw_hashtable (pp, ff)
     struct ht **pp;
     void (*ff) (struct ht_identifier **);
{
  hashnode *q;
  unsigned int  i;
  struct ht *p;
  
  typedef hashnode (*alloc_node_fp)(hash_table *);
  extern alloc_node_fp pfe_get_cpphash_alloc_node PARAMS ((void));

  /* pfe_freeze_thaw_hashtable() is only called to freeze/thaw
     hashtables which will never be null.  So we don't want to
     use PFE_FREEZE_THAW_PTR to freeze/thaw pp.  This is because
     we know *pp should not be null and when PFE_NO_THAW_LOAD is
     1 PFE_FREEZE_THAW_PTR would alwsys return NULL when thawing.
     
     We need to execute at least the code that reestablishes our
     function pointers in the struct ht and also the obstack.  */
     
  p = (struct ht *)pfe_freeze_thaw_ptr_fp (pp);
  assert (p);

  /* When loading, restore the pointer to the function for
     allocating hash table nodes.  */
  if (PFE_THAWING)
    p->alloc_node = pfe_get_cpphash_alloc_node ();
  
  /* Freeze/thaw the obstack embedded in the hash table.  */
  pfe_freeze_thaw_obstack (&p->stack);

  q = PFE_FREEZE_THAW_PTR (&p->entries);
  
  if (q)
    for (i = 0; i < p->nslots; ++i, ++q)
      if (*q)
	(*ff) ((struct ht_identifier **)q);
  
  /* Don't bother to thaw the "pfile" ptr. The comment in the header
     says it's for the benefit of cpplib.  */
}

/*-------------------------------------------------------------------*/

/* Freeze/thaw a pointer to and a cpp_token struct (defined in 
   cpplib.h).  */
void 
pfe_freeze_thaw_cpp_token (tp)
  struct cpp_token **tp;
{
  struct cpp_token *t = PFE_FREEZE_THAW_PTR (tp);
  
  if (!t)
    return;
  
#if 0
  /* ??? FF_diagnostic  */
  printf ("pfe_freeze_thaw_cpp_token: 0x%x\n", (unsigned) t);
#endif

  switch (t->type) {
    case CPP_NAME:
      pfe_freeze_thaw_cpp_hashnode (&t->val.node);
      break;
    case CPP_NUMBER:
    case CPP_STRING:
    case CPP_CHAR:
    case CPP_WCHAR:
    case CPP_WSTRING:
    case CPP_HEADER_NAME:
    case CPP_COMMENT:
#if 0
      /* ??? FF_diagnostic  */
      if (pfe_operation == PFE_DUMP && !pfe_is_pfe_mem(t->val.str.text))
        {
          printf ("pfe_freeze_thaw_cpp_token: token->val.str.text not in PFE memory: 0x%x\n", 
          	  (unsigned) t->val.str.text);
          switch (t->type) {
	    case CPP_NUMBER:
	      printf("  t->type = %s '%s'\n", "CPP_NUMBER", t->val.str.text); break;
	    case CPP_STRING:
	      printf("  t->type = %s\n", "CPP_STRING"); break;
	    case CPP_CHAR:
	      printf("  t->type = %s\n", "CPP_CHAR"); break;
	    case CPP_WCHAR:
	      printf("  t->type = %s\n", "CPP_WCHAR"); break;
	    case CPP_WSTRING:
	      printf("  t->type = %s\n", "CPP_WSTRING"); break;
	    case CPP_HEADER_NAME:
	      printf("  t->type = %s\n", "CPP_HEADER_NAME"); break;
	    case CPP_COMMENT:
	      printf("  t->type = %s\n", "CPP_COMMENT"); break;
          }
        }
#endif
      pfe_freeze_thaw_ptr_fp (&t->val.str.text);
#if 0
      /* ??? FF_diagnostic  */
      if (pfe_operation == PFE_LOAD && !pfe_is_pfe_mem(t->val.str.text))
        {
          printf ("pfe_freeze_thaw_cpp_token: token->val.str.text not in PFE memory: 0x%x\n", 
          	  (unsigned) t->val.str.text);
          switch (t->type) {
	    case CPP_NUMBER:
	      printf("  t->type = %s\n", "CPP_NUMBER"); break;
	    case CPP_STRING:
	      printf("  t->type = %s\n", "CPP_STRING"); break;
	    case CPP_CHAR:
	      printf("  t->type = %s\n", "CPP_CHAR"); break;
	    case CPP_WCHAR:
	      printf("  t->type = %s\n", "CPP_WCHAR"); break;
	    case CPP_WSTRING:
	      printf("  t->type = %s\n", "CPP_WSTRING"); break;
	    case CPP_HEADER_NAME:
	      printf("  t->type = %s\n", "CPP_HEADER_NAME"); break;
	    case CPP_COMMENT:
	      printf("  t->type = %s\n", "CPP_COMMENT"); break;
          }
        }
#endif
      break;
    default:
      break;
  }
}

/*-------------------------------------------------------------------*/

/* The following declarations are used to allow us to control a sizeof
   check for each struct we freeze/thaw.  We do this by a call to 
   pfe_check_structs() when -fpfedbg=check-structs is specified.  The
   reason for this is to attempt to verify that no fields of these
   structs are deleted or new ones added when each new merge is done
   with the fsf.
   
   Deleted fields probably will result in an compilation error if its
   a field wer'e freeze/thawing.  Additional fields could cause us to
   miss something that now needs freezeing/thawing if the addional
   field are pointers.  The sizeof check will pick these up.
   
   Of course if the size doesn't change, there still no guarantee
   hasn't changed.  But hopefully it will again cause a compile error
   if we reference the changed field in it's old form.  */
   
#include "tree.h"
#include "rtl.h"
#include "varray.h"
#include "cpplib.h"

#define GCC_STRUCTS
#define DEFCHECKSTRUCT(name, assumed_size) \
  extern void CONCAT2(check_struct_, name) PARAMS ((int));
#include "structs-to-check.def"
#undef DEFCHECKSTRUCT

#define DEFCHECKSTRUCT(name, n) CONCAT2(check_struct_, name),
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
#undef GCC_STRUCTS

/* Call each function in the struct_check_functions[] array to 
   check the size of their associated struct.  */
void 
pfe_check_all_struct_sizes ()
{
  int i;
  
  for (i = 0; i < (int)ARRAY_SIZE (struct_check_functions) - 1; ++i)
    (*struct_check_functions[i]) (assumed_struct_size[i]);
  
  (*lang_hooks.pfe_check_all_struct_sizes) ();
}

/* Each struct is check by a function defined using the
   DEFINE_CHECK_STRUCT_FUNCTION macro.  it results in a call to
   here were we do the check and print a message with the
   actual size if it is not the expected size.  */
void
pfe_check_struct_size (actual_size, assumed_size, name)
     int actual_size;
     int assumed_size;
     const char *name;
{
  if (actual_size != assumed_size)
    fprintf (stderr, "### struct %s: was %d, now %d\n",
	     name, assumed_size, actual_size);
}

/*-------------------------------------------------------------------*/

struct rtunion_def_union {
   union rtunion_def u;
};

struct tree_node_union {
   union tree_node u;
};

struct varray_data_union {
   union varray_data_tag u;
};

/* rtl.h */
DEFINE_CHECK_STRUCT_FUNCTION (rtunion_def_union)
DEFINE_CHECK_STRUCT_FUNCTION (rtx_def)
DEFINE_CHECK_STRUCT_FUNCTION (rtvec_def)

/* tree.h */
DEFINE_CHECK_STRUCT_FUNCTION (tree_node_union)
DEFINE_CHECK_STRUCT_FUNCTION (tree_common)
DEFINE_CHECK_STRUCT_FUNCTION (tree_int_cst)
DEFINE_CHECK_STRUCT_FUNCTION (tree_real_cst)
DEFINE_CHECK_STRUCT_FUNCTION (tree_string)
DEFINE_CHECK_STRUCT_FUNCTION (tree_complex)
DEFINE_CHECK_STRUCT_FUNCTION (tree_vector)
DEFINE_CHECK_STRUCT_FUNCTION (tree_identifier)
DEFINE_CHECK_STRUCT_FUNCTION (tree_decl)
DEFINE_CHECK_STRUCT_FUNCTION (tree_type)
DEFINE_CHECK_STRUCT_FUNCTION (tree_list)
DEFINE_CHECK_STRUCT_FUNCTION (tree_vec)
DEFINE_CHECK_STRUCT_FUNCTION (tree_exp)
DEFINE_CHECK_STRUCT_FUNCTION (tree_block)

/* varray.h */
DEFINE_CHECK_STRUCT_FUNCTION (varray_data_union)
DEFINE_CHECK_STRUCT_FUNCTION (varray_head_tag)

/* cpplib.h */
DEFINE_CHECK_STRUCT_FUNCTION(cpp_token)
DEFINE_CHECK_STRUCT_FUNCTION(cpp_hashnode)

/* c-common.h */
DEFINE_CHECK_STRUCT_FUNCTION (language_function)
DEFINE_CHECK_STRUCT_FUNCTION (stmt_tree_s)

#endif /* PFE */


