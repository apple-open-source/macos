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

#ifndef GCC_PFE_H
#define GCC_PFE_H

#ifdef PFE

#include <stdio.h>

/* Filename and FILE variable for the specified load/dump file. */
extern const char *pfe_name;
extern FILE *pfe_file;

/* Structs referenced as pointers.  */
union tree_node;		/* in tree.h			*/
struct language_function;	/* in c-common.h   		*/
struct binding_level;		/* in cp/decl.c or c-decl.c 	*/
struct named_label_use_list;	/* in cp/decl.c    		*/
struct named_label_list;	/* in cp/decl.c    		*/
struct unparsed_text;		/* in cp/spew.c    		*/
struct rtx_def;			/* in rtl.h			*/
struct rtvec_def;		/* in rtl.h			*/
struct pfe_compiler_state;	/* in pfe/pfe-header.h		*/
struct varray_head_tag;		/* in varray.h			*/
struct cpp_token;		/* in cpplib.h			*/
struct cpp_hashnode;		/* in cpplib.h			*/
struct function;		/* in function.h		*/
struct rtx_def;			/* in rtl.h			*/
struct pfe_lang_compiler_state;	/* in pfe/cp-freeze-thaw.c	*/

/* Magic number to identify a PFE dump file.  */
#define PFE_MAGIC_NUMBER 0x0ABACAB0

/* The version number of the current dump file format.  */
#define PFE_FORMAT_VERSION 1

/* Operations performed by the load/dump mechanism.  These are
   passed as arguments to pfe_init to indicate whether a load
   or dump is being performed in the current compilation.  */
enum pfe_action {
  PFE_NOT_INITIALIZED,	/* Identifies if PFE has been init'ed.	*/
  PFE_NOP,		/* Use when not loading or dumping.	*/
  PFE_DUMP,		/* Use when performing a dump.		*/
  PFE_LOAD		/* Use when performing a load.		*/
};

/* Identify whether the current operation is a load, dump, or neither,
   and whether the PFE system has been initialized.  */
extern enum pfe_action pfe_operation;

/* In the context of freezing/thawing, the following macros make
   things a little easier to read when we cannot do an operation
   that can be handled by a function handling both freezing and
   thawing.  */
#define PFE_FREEZING (pfe_operation == PFE_DUMP)
#define PFE_THAWING  (pfe_operation == PFE_LOAD)

/* Pointer to the PFE header with the global compiler state to be
   saved when dumping and restored when loading.  It contains the
   "roots" of various compiler data structures.  This pointer is
   set by pfe_dump_compiler_state when performing a dump, before
   the pfe_freeze_compiler_state is called to fill in the header.  
   When loading, the pointer is set by pfe_load_compiler_state
   before pfe_thaw_compiler_state is called.  */
extern struct pfe_compiler_state *pfe_compiler_state_ptr;

/* Initialize the PFE.  Specify whether we are doing a dump, load, or
   neither.  The intent is that pfe_init should be called once per
   compilation.  In the case of a dump, a memory "zone" will be 
   allocated for the PFE memory management routines, so that 
   allocations of compiler data structures can be routed to the PFE 
   memory zone so that the structures can be dumped at the end of the 
   compilation.  
   
   This function allocates the compiler state header which gets used 
   by the function freezing the compiler state.  Allocating the header
   early here allows other parts of the compiler to access and fill in
   the header.
   
   Note: the PFE memory management routines can be used when performing
   a dump, load, or neither.  The routines will try to do the right
   thing for the operation involved.  */
extern void pfe_init 			PARAMS ((enum pfe_action));

/* Shut down the PFE.  */
extern void pfe_term 			PARAMS ((void));

/* Create and open a pfe file for dumping or open a pfe file for
   loading.  */
extern void pfe_open_pfe_file 		PARAMS ((char *, char *, int));

/* Close a currently opened pfe file and optionally delete it.  */
extern void pfe_close_pfe_file 		PARAMS ((int));

/* Print out a message for an error and quit.  */
extern void error                       PARAMS ((const char *, ...));
/* Print out a message for an internal error and quit.  */
extern void  pfe_internal_error		PARAMS ((const char *));

/* Write out a precompiled header file for the current compiler state
   to the specified file.  A non-zero return code indicates a failure
   during the dump.
   
   The parameters to pfe_dump are the file in which to write the
   dump data, a "freezing" function for the dumper to call to freeze
   all the pointers in the dumped data structures, a pointer to the
   header containing the "roots" of the dumped data structures, and
   the size of that header.
   
   The header passed to the pfe_dump should have the "roots" of
   (i.e., pointers to) all of the compiler data structures in the
   load/dump file.  This data structure is used as the starting
   point for walking all of the compiler structures, "freezing"
   pointers (converting them to file offsets) during a dump, and
   "thawing" them (converting them back to pointers) during a load.
   Freezing takes place in the middle of a dump, after all of the
   memory ranges to be dumped have been identified and assigned
   file offset.  The header should be allocated in non-PFE memory
   (using malloc instead of pfe_malloc) because it will be written
   at a known place (the beginning) in the dump file.  (If it were
   allocated via pfe_malloc its location would be dependent on the
   implementation of the underlying memory manager.)  When the
   freezing function is called, it is passed a pointer to the
   header with the roots.  
   
   A refinement of the dump/load mechanism is the "no thaw load" 
   which assumes that the load file will be loaded at a specific
   address.  This is possible if the OS provides a call so that
   memory can be allocated at a requested address (in some address
   range that is distinguishable for the usual addresses returned
   by malloc).  When using this scheme, the dump file is frozen
   so that pointers are adjusted to this assumed address range.
   Frozen pointers are then identified by the fact that they lie
   in this range (above the assumed load address on Mac OS X),
   rather than by the usual method of turning on the low order bit
   of the pointer.  At load time, if the file can be loaded at
   the requested address, the thawing step can be skipped because
   the pointers will not require any fixing up.  If the file cannot
   be loaded at the assumed address, the load can continue with
   a thawing process that identifies frozen pointers by the fact
   that they lie in a range of addresses outside of the usual
   malloc range.
   
   Note: pfe_dump is a generic form of the dumping mechanism that
   can be called independently of the compiler for testing purposes.
   The pfe_dump_compiler_state function should be called to dump
   the compiler state.  This function assumes the compiler state
   is kept in the pfe_compiler_state record and that the
   pfe_freeze_compiler_state function is used to freeze the state
   information.  
   
   Note: the PFE allocation routines cannot be called after the
   freezing process of a dump has been started because allocated
   memory chunks are assigned dump file offsets before pointers
   can be frozen.  */
extern int pfe_dump 			PARAMS ((FILE *, void (*) (void *), void *, size_t));

/* Variant of pfe_dump which assumes that pfe_compiler_state_ptr is the
   pointer to the state header to be saved and that pfe_freeze_compiler_state
   is the function to be called to save (and "freeze") the compiler state.  
   This function allocates the compiler state header which gets filled
   in by the function freezing the compiler state.  */
extern int pfe_dump_compiler_state 	PARAMS ((FILE *));

/* Read in the precompiled header from the specified file.  Return a
   pointer to the header with compiler data structure "roots".  The
   data structures just loaded should then be "thawed" by walking
   all of the data structures pointed to by the roots and thawing
   all pointers.
   
   This is the generic function to do a load, and can be used to test
   the load/dump mechanism independent of the compiler.  The 
   pfe_load_compiler_state calls this function to load the compiler 
   state.  */
extern void *pfe_load 			PARAMS ((FILE *));

/* Read in the precompiled header from the specified file and restore
   the compiler state.  The pfe_thaw_compiler_state function will be
   called to thaw the data in the compiler state header and copy
   the appropriate values back to compiler globals.  */
extern void pfe_load_compiler_state 	PARAMS ((FILE *));

/* A "no thaw load" is possible if we can load the pre-compiled 
   header information at a pre-determined address, so that no thawing
   is required when loading.  */
#define PFE_NO_THAW_LOAD 1
#if PFE_NO_THAW_LOAD
#define PFE_NO_THAW_LOAD_ADDR 0x30000000
#endif

/* Determine whether a pointer has been frozen.  This assumes
   that there are no valid pointers to odd addresses.  */
#if PFE_NO_THAW_LOAD
#define PFE_IS_FROZEN(p) pfe_is_frozen (p)
extern int pfe_is_frozen		PARAMS ((void *));
#else
#define PFE_IS_FROZEN(p) ((int)(p) & 1)
#endif

/* Determine whether a pointer points into PFE allocated memory.  This
   routine will return a non-zero result if the pointer is to memory
   in the area managed by the PFE when dumping or in the load memory
   area when loading.  Otherwise zero will be returned, including 
   when the pointer is to memory allocated by the PFE during a load
   but outside of the load memory area (i.e., for stuff not in the
   load file) and for memory allocated via PFE calls when not doing
   a load or dump.  */
extern int pfe_is_pfe_mem 		PARAMS ((void *));

/* Convert a pointer to an offset so that it can be saved to a file.  
   The parameter is a pointer to the pointer in question.  Returns
   the original pointer before freezing.  */
extern void *pfe_freeze_ptr 		PARAMS ((void *));

/* Convert an offset to a pointer after it has been restored from a 
   file.  The parameter is a pointer to the offset that will be
   converted to a pointer.  Returns the thawed pointer.  */
extern void *pfe_thaw_ptr 		PARAMS ((void *));

/* If pfe_freeze_ptr() or pfe_thaw_ptr() is passed as a function
   pointer then they ore of the following function pointer type.  */
typedef void *(*pfe_freeze_thaw_ptr_t)(void *);

/* Function pointer to either pfe_freeze_ptr() or pfe_thaw_ptr()
   as determined by pfe_operation.  */
extern pfe_freeze_thaw_ptr_t pfe_freeze_thaw_ptr_fp;

/* Freeze/thaw a pointer (passed as a pointer to that pointer).
   Freezing or thawing is indicated by the setting of pfe_operation
   declared above.  */
extern void *pfe_freeze_thaw_ptr        PARAMS ((void **));

/* Macro used to call pfe_freeze_thaw_ptr().  */
#define PFE_FREEZE_THAW_PTR(p) pfe_freeze_thaw_ptr ((void **)(p))

/* Variant of pfe_freeze_thaw_ptr which allows ptrs to be slightly
   beyond the upper bound of a memory range.  The parameter n 
   specifies how far beyond the range a pointer may be to still be
   considered to be a part of that range.  
   
   In a few GCC structures there are pointers that point to the
   "limit" of the structure, which happens to be one byte beyond
   the end of the structure.  In some cases the pointer can end up
   pointing outside of PFE memory if the structure in question is
   allocated at the very end of a PFE memory range.  This means
   special consideration must be given to such pointers when they
   are being frozen and thawed to account for the fact that they
   would normally not appear to belong to any range.  For specific
   pointers known to have this property, this special variant of 
   pfe_freeze_thaw_ptr is called which allows small deviations from 
   the upper bound of memory ranges when freezing.  */

extern void *pfe_freeze_thaw_ptr_with_variance PARAMS ((void **, unsigned long));
#define PFE_FREEZE_THAW_PTR_WITH_VARIANCE(p, n) \
  pfe_freeze_thaw_ptr_with_variance ((void **)(p), n)

/* Convert what might be a frozen pointer to a real pointer.  Returns 
   non-frozen pointers as is.  This can be called during a load or a 
   dump.  (For internal diagnostic purposes.)  */
extern void *pfe_real_ptr 		PARAMS ((void *));

/* A pithy macro to access pfe_real_ptr().  (I question whether we
   want this macro in the global compiler namespace. -ff)  */
#define RP(p) (pfe_real_ptr (p))

/* Identify temporaries not in PFE memory being using during the
   freezing/thawing process.  Identifying the temporary currently
   in use will suppress diagnostics about trying to freeze/thaw
   a pointer not in PFE memory.  */
extern void pfe_using_temp_ptr		PARAMS ((void **));

/* PFE's malloc: allocates memory in a "zone" controlled by the PFE when 
   performing a dump, otherwise it calls the normal malloc.  */
extern void *pfe_malloc 		PARAMS ((size_t));

/* PFE's calloc: allocates and zeros memory for n objects in a "zone" 
   controlled by the PFE when performing a dump, otherwise it calls
   the normal calloc.  */
extern void *pfe_calloc 		PARAMS ((size_t, size_t));

/* PFE's realloc: reallocates memory for pointer in a "zone" 
   controlled by the PFE when performing a dump, otherwise it calls
   the normal realloc.  */
extern void *pfe_realloc 		PARAMS ((void *, size_t));

/* PFE's free: free memory in a "zone" controlled by the PFE when 
   performing a dump, otherwise it calls the normal free except
   when the memory is in the load memory area.  */
extern void pfe_free 			PARAMS ((void *));

#if PFE_MALLOC_STATS
/* A cover routine for PFE's malloc that keeps statistics about
   malloc allocations by kind and size.  */
extern void *pfe_s_malloc 		PARAMS ((size_t, enum pfe_alloc_object_kinds));

/* A cover routine for PFE's calloc that keeps statistics about
   calloc allocations by kind and size.  */
extern void *pfe_s_calloc 		PARAMS ((size_t, size_t, enum pfe_alloc_object_kinds));

/* A cover routine for PFE's realloc that keeps statistics about
   realloc allocations by kind and size.  */
extern void *pfe_s_realloc 		PARAMS ((void *, size_t, enum pfe_alloc_object_kinds));
#endif

/* When doing a dump, allocate a copy of the specified string using 
   pfe_malloc.  When not doing a dump, just return the original string.
   
   Caution: We use a hash table to ensure that there is only one copy
   of each unique string.  So don't use this to allocate strings that
   are not const.  */
extern void *pfe_savestring 		PARAMS ((char *));
#undef PFE_SAVESTRING
#define PFE_SAVESTRING(s) (pfe_savestring ((char *)(s)))

/* Freeze/thaw the compiler state.  */	
extern void pfe_freeze_compiler_state   PARAMS ((void *));
extern void pfe_thaw_compiler_state     PARAMS ((struct pfe_compiler_state *));

#define PFE_NEW_TREE_WALK 1
#if PFE_NEW_TREE_WALK
/* Add a tree node to the stack of nodes needing freezing/thawing.  */
extern void pfe_freeze_thaw_tree_push   PARAMS ((union tree_node **));

/* Freeze/thaw all of the tree nodes that have been pushed by
   pfe_freeze_thaw_tree_push and their descendents (i.e., walk the
   trees).  */
extern void pfe_freeze_thaw_tree_walk   PARAMS ((void));
#else
/* Freeze/thaw tree node and its direct descendents (i.e., walk tree).  */
extern void pfe_freeze_thaw_tree_walk   PARAMS ((union tree_node **));
#endif

#if PFE_NEW_TREE_WALK
/* Macro to call freeze_thaw_tree_push().  Note that the macro is 
   passed just the node, not the address of the node.  The address 
   is taken here.  */
#define PFE_FREEZE_THAW_WALK(node) pfe_freeze_thaw_tree_push(&(node))
#else
/* Macro to make calling freeze_thaw_tree_walk().  Note that the
   macro is passed just the node, not the address of the node.  The
   address is taken here.  */
#define PFE_FREEZE_THAW_WALK(node) pfe_freeze_thaw_tree_walk(&(node))
#endif

/* Freeze/thaw a rtl node.  */
extern void pfe_freeze_thaw_rtx		PARAMS ((struct rtx_def **));

/*                       *** CAUTION/WARNING ***
   Like anything else that is frozen/thawed, the strings for XSTR and
   XTMPL rtx's must be allocated in pfe memory.  While this is taken
   care if on the compiler, care must be taken of how XSTR's are
   created in target dependent code (i.e., code in the gcc/config
   directory).  Using ggc_alloc_string() is fine.  But allocation
   any other way must involve one of the pfe allocators (e.g.,
   pfe_malloc).  Currently we have no good way to cover such cases.  */
   
/* Macro to make calling pfe_freeze_thaw_rtx().  Note that the macro
   is passed just the rtx (ptr to rtl entry), not the address of the
   entry.  The address is taken here.  */
#define PFE_FREEZE_THAW_RTX(rtx) pfe_freeze_thaw_rtx (&(rtx))

/* Freeze/thaw a rtvec node.  */
extern void pfe_freeze_thaw_rtvec	PARAMS ((struct rtvec_def **));

/* Freeze/thaw various data objects.  */

/* pfe/c-common-freeze-thaw.c  */
extern void pfe_freeze_thaw_common_language_function PARAMS ((struct language_function *));

/* c-decl.c or cp-freeze-thaw.c  */
extern void pfe_freeze_thaw_language_function PARAMS ((struct language_function **));

/* cp/decl.c or c-decl.c  */
extern void pfe_freeze_thaw_binding_level PARAMS ((struct binding_level **));

/* cp/decl.c  */
extern void pfe_freeze_thaw_named_label_use_list PARAMS ((struct named_label_use_list **));
extern void pfe_freeze_thaw_named_label_list     PARAMS ((struct named_label_list **));

/* cp/spew.c  */
extern void pfe_freeze_thaw_unparsed_text PARAMS ((struct unparsed_text **));

/* function.c - freeze/thaw a  pointer to a struct function.  */
extern void pfe_freeze_thaw_function	         PARAMS ((struct function **));

/* Macros to make it a  little "easier" (?) to move globals between the pfe
   headedr and compiler globals.  */
   
#define PFE_GLOBAL_TO_HDR_IF_FREEZING(g) if (PFE_FREEZING) hdr->g = g
#define PFE_HDR_TO_GLOBAL_IF_THAWING(g)  if (PFE_THAWING)  g = hdr->g

#define PFE_FREEZE_THAW_GLOBAL_TREE(g) \
  do { \
    PFE_GLOBAL_TO_HDR_IF_FREEZING (g); \
    PFE_FREEZE_THAW_WALK (hdr->g); \
    PFE_HDR_TO_GLOBAL_IF_THAWING (g); \
  } while (0)

#define PFE_GLOBAL_TREE_ARRAY_TO_HDR(g, n) \
  if (PFE_FREEZING) memcpy (hdr->g, g, (int)(n) * sizeof (tree))
#define PFE_HDR_TO_GLOBAL_TREE_ARRAY(g, n) \
  if (PFE_THAWING) memcpy (g, hdr->g, (int)(n) * sizeof (tree))
  
#define PFE_FREEZE_THAW_GLOBAL_TREE_ARRAY(g, n) \
  do { \
    PFE_GLOBAL_TREE_ARRAY_TO_HDR(g, n); \
    for (i = 0; i < (int)(n); ++i) PFE_FREEZE_THAW_WALK (hdr->g[i]); \
    PFE_HDR_TO_GLOBAL_TREE_ARRAY(g, n); \
  } while (0)

#define PFE_FREEZE_THAW_GLOBAL_RTX(g) \
  do { \
    PFE_GLOBAL_TO_HDR_IF_FREEZING (g); \
    PFE_FREEZE_THAW_RTX (hdr->g); \
    PFE_HDR_TO_GLOBAL_IF_THAWING (g); \
  } while (0)

#define PFE_GLOBAL_RTX_ARRAY_TO_HDR(g, n) \
  if (PFE_FREEZING) memcpy (hdr->g, g, (int)(n) * sizeof (struct rtx_def *))
#define PFE_HDR_TO_GLOBAL_RTX_ARRAY(g, n) \
  if (PFE_THAWING) memcpy (g, hdr->g, (int)(n) * sizeof (struct rtx_def *))
  
#define PFE_FREEZE_THAW_GLOBAL_RTX_ARRAY(g, n) \
  do { \
    PFE_GLOBAL_TREE_ARRAY_TO_HDR(g, n); \
    for (i = 0; i < (int)(n); ++i) PFE_FREEZE_THAW_RTX (hdr->g[i]); \
    PFE_HDR_TO_GLOBAL_TREE_ARRAY(g, n); \
  } while (0)
  
#ifndef DMP_TREE /* don't define these in the xx-dmp-tree.[ch] routines. */
/* The following overrides any uses of VARRAY_FREE defined in varray.h.  This
   allows us to decide which memory the varray_type was allocated in.  This
   also means that this header MUST follow varray.h in any file which includes
   both these headers.  Note, the referenced routines are all in varray.c.  */
#undef VARRAY_FREE
#define VARRAY_FREE(vp) \
  do { if (vp) { pfe_varray_free (vp); vp = (varray_type)0; } } while (0)
extern void pfe_varray_free 	        PARAMS ((struct varray_head_tag *));
extern void pfe_freeze_thaw_varray_tree PARAMS ((struct varray_head_tag **));
#endif

/* command line macro validation */
enum {
  PFE_MACRO_NOT_FOUND,  /* Not found in pfe header identifier hashtable */
  PFE_MACRO_FOUND, 
  PFE_MACRO_CMDLN, 	/* Found macro. It is command line macro.  */ 
  PFE_MACRO_DIFFERENT,  /* Found macro, but it's value is different */
  PFE_MACRO_IDENTICAL
};

/* Macro validatin flags */
#define PFE_MACRO_FOUND	  (1 << 0)	/* Found macro in PFE identifier hash table.  */
#define PFE_MACRO_CMD_LN  (1 << 1)	/* It is a command line macro.  */


/* Turn On/Off pfe macro validation.  */
extern int pfe_macro_validation;
extern int pfe_cmd_ln_macro_count;
extern int pfe_macro_status;
/* Set/Reset the flag to indicate that command line macro processing 
   is in progress.  */
extern void pfe_set_cmd_ln_processing   PARAMS ((void));
extern void pfe_reset_cmd_ln_processing PARAMS ((void));
/* Return 1 if command line macro processing is in progress.  */
extern int pfe_is_cmd_ln_processing     PARAMS ((void));

#endif /* PFE */
#endif /* GCC_PFE_H */
