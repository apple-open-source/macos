/* APPLE LOCAL PFE */
/* Persistent Front End (PFE) low-level and common routines.
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

/* Flag whether we are using the Macintosh OS X scalable_malloc
   implementation for the PFE's memory management functions.  */
#ifndef USE_APPLE_SCALABLE_MALLOC
#define USE_APPLE_SCALABLE_MALLOC 0
#endif

/* The GENERATOR_FILE is used to suppress some includes in config.h
   such as insn-flags.h and insn-flags.h which we don't want nor 
   need for the files that use pfe-header.h.  Unfortunately this
   macro also controls an additional enum value in machmode.def
   which is used by machmode.h.  That enum determines the value
   of MAX_MACHINE_MODE which we reference in the pfe_compiler_state.
   Since most everything is built using the default build rules
   determined by the Makefile, and since those default rules do
   NOT define GENERATOR_FILE, then we need to keep machmode.h
   from generating that additional enum value and thus making
   MAX_MACHINE_MODE inconsistant.
   
   By undefining GENERATOR_FILE before we do our includes we
   suppress the extra enum value.  The machmode.h header is
   included by tree.h, rtl.h, and varray.h.  */
   
#undef GENERATOR_FILE

/* The scalable_malloc.h header must be included BEFORE any gcc headers.
   This is necessary because system.h "poisons" the use of malloc, calloc,
   and realloc when building the compiler itself with a 3.x version or
   greater.  The scalable_malloc.h references malloc so it cannot be
   poisoned.  And since the header is really independent of the gcc
   anyway it doesn't need any of its headers so we have no problem
   including it first.  */
    
#if USE_APPLE_SCALABLE_MALLOC
#include "scalable_malloc.h"
#endif

#include "config.h"
#include "system.h"
#include "timevar.h"
#include "hashtab.h"
#include "machmode.h"
#include "tm_p.h"
#include "langhooks.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>

#if !USE_APPLE_SCALABLE_MALLOC
#include "pfe-mem.h"
#endif

#include "pfe.h"
#include "pfe-header.h"

#if PFE_NO_THAW_LOAD
#define PFE_NO_THAW_LOAD_USING_MMAP 1
#if PFE_NO_THAW_LOAD_USING_MMAP
#include <sys/types.h>
#include <sys/mman.h>
#else
#include <mach/mach.h>
#include <mach/mach_error.h>
#endif
#define PFE_NO_THAW_LOAD_DIAGNOSTICS 0
#endif

/* Filename and FILE variable for the specified load/dump file.  */
const char *pfe_name = NULL;
FILE       *pfe_file = NULL;

/* Identify whether the current operation is a load, dump, or neither,
   and whether the PFE system has been initialized.  */
enum pfe_action pfe_operation = PFE_NOT_INITIALIZED;

/* Pointer to the PFE header with the global compiler state to be
   saved when dumping and restored when loading.  It contains the
   "roots" of various compiler data structures.  This pointer is
   set by pfe_dump_compiler_state when performing a dump, before
   the pfe_freeze_compiler_state is called to fill in the header.  
   When loading, the pointer is set by pfe_load_compiler_state
   before pfe_thaw_compiler_state is called.  */
pfe_compiler_state *pfe_compiler_state_ptr = 0;

/* Pointer to either pfe_freeze_ptr() or pfe_thaw_ptr() as 
  determined by pfe_operation.  */
pfe_freeze_thaw_ptr_t pfe_freeze_thaw_ptr_fp = NULL;

/*-------------------------------------------------------------------*/

/* Control debugging output related to the PFE's handling of memory 
   ranges: identifying them, using them to freeze pointers, and
   writing them out.  */
#ifndef PFE_DEBUG_MEMMGR_RANGES
#define PFE_DEBUG_MEMMGR_RANGES 0
#endif

/* Control extra checking for correct use of and arguments for PFE
   routines.  (The intent is to provide diagnostics for use during
   development.)  */
#ifndef PFE_DEBUG_EXTRA_CHECKING
#define PFE_DEBUG_EXTRA_CHECKING 1
#endif

/* Control whether we collect statistics on PFE memory utilization.  */
#ifndef PFE_MEMORY_STATS
#define PFE_MEMORY_STATS 1
#endif

/* Set the intial size of the table of memory ranges to be dumped.
   The size will be doubled as needed starting with this size.  */
#define PFE_INITIAL_DUMP_RANGE_TABLE_SIZE 100
#define PFE_DUMP_RANGE_TABLE_INCR 50

/* Structure for entry in table of PFE memory ranges to be written into
   a "dump" file.  */
struct pfe_dump_memory_range {
  unsigned long start_addr;
  unsigned long end_addr;	/* Actually points to byte past the end.  */
  unsigned long file_offset;
};

typedef struct pfe_dump_memory_range pfe_dump_memory_range;

/* Pointer to buffer in which precompiled header information was
   loaded.  */
static char *pfe_load_buffer_ptr = 0;

/* Size of file loaded in load operation.  */
static unsigned int pfe_load_buffer_size = 0;

#if USE_APPLE_SCALABLE_MALLOC
/* Memory "zone" in which the compiler structures the PFE will "dump"
  are  allocated.  */
static malloc_zone_t *pfe_memory_zone = 0;
#endif

/* Table with entries for each memory range to be "dump"ed.  */
static pfe_dump_memory_range *pfe_dump_range_table;

/* Current size of the dump range table.  */
static int pfe_dump_range_table_size = 0;

/* Number of items currently in the dump range table.  */
static int pfe_dump_range_table_nitems = 0;

/* In a few GCC structures there are pointers that point to the
   "limit" of the structure, which happens to be one byte beyond
   the end of the structure.  In some cases the pointer can end up
   pointing outside of PFE memory if the structure in question is
   allocated at the very end of a PFE memory range.  This means
   special consideration must be given to such pointers when they
   are being frozen and thawed to account for the fact that they
   would normally not appear to belong to any range.  For specific
   pointers known to have this property, a special variant of 
   pfe_freeze_thaw_ptr is called which adjusts the variable below
   to permit small deviations from the upper bound of memory ranges
   when freezing.  */
static unsigned long pfe_allowed_ptr_variance = 0;

/* Flag indicating PFE memory is locked while in the freezing
   phase of a dump.  */
static int pfe_memory_locked_for_dump = 0;

/* Address of a temporary being used during the freezing/thawing
   process.  Suppress messages about freezing/thawing pointers
   not in PFE memory when the pointers are in this temp.  */
static void **pfe_temp_ptr = 0;

#if PFE_NO_THAW_LOAD
/* Address assumed for load area if we are trying to do a "no thaw
   load".  This will be zero if we were not able to assume such
   an address.  */
static unsigned long pfe_assumed_load_address = 0;

/* Flag indicating that we are doing a "no thaw load".  This will
   happen if we were able to assume a load address when doing the
   dump and secure that address when doing the load.  */
static int pfe_no_thaw_load = 0;

/* Value by which to adjust pointers that are being thawed.
  If we are doing a load in which we assumed a load address
   that we were not able to secure, we need to adjust pointers
   being thawed by an amount that takes into account the
   assumed load address and the actual load address.  
   Otherwise the adjustment is just for the actual load
   address.  */
static unsigned long pfe_load_offset_adjust;
#endif

/* Variables to collect statistics on PFE memory utilization.   */
#if PFE_MEMORY_STATS
extern int pfe_display_memory_stats;
static size_t pfe_malloc_total_size = 0;
static int pfe_mallocs = 0;
static int pfe_callocs = 0;
static int pfe_reallocs = 0;
static int pfe_frees = 0;
static int pfe_frees_from_load_area = 0;
static int pfe_savestrings = 0;
static int pfe_freeze_ptrs = 0;
static int pfe_thaw_ptrs = 0;
#endif

/* Control to enable extra checking in PFE for correct usage.  This 
   should be turned off when producing a production version or a 
   version to be used for timing tests since the extra checking may
   be expensive.   */
#if PFE_DEBUG_EXTRA_CHECKING
static int pfe_extra_checking = 1;
#endif

/* A hash table is used to ensure that there is only one instance of
   strings going into PFE memory via pfe_savestring.  */
static struct ht *pfe_str_hash_table;

/* Flag to indicate if command macro line processing is in progress
   or not.  */
static int pfe_cmd_ln_processing = 0;

#if PFE_MALLOC_STATS
/* Set the intial size of the table used to keep track of malloc
   allocations by kind and size.  */
#define PFE_S_MALLOC_TABLE_SIZE 100
#define PFE_S_MALLOC_TABLE_INCR 50

/* Structure for entry in table for keeping statistics on malloc 
   allocations by kind.  */
struct pfe_s_malloc_entry {
  enum pfe_alloc_object_kinds kind;
  unsigned long size;
  unsigned long count;
};

typedef struct pfe_s_malloc_entry pfe_s_malloc_entry;

/* Table with entries for each memory range to be "dump"ed.  */
static pfe_s_malloc_entry *pfe_s_malloc_table;

/* Current size of the dump range table.  */
static int pfe_s_malloc_table_size = 0;

/* Number of items currently in the dump range table.  */
static int pfe_s_malloc_table_nitems = 0;

#define DEF_PFE_ALLOC(sym, str) str,

/* Names of different kinds of malloc allocations.  */
char *pfe_alloc_object_kinds_names[PFE_ALLOC_NBR_OF_KINDS] = {
#include "pfe-config.def"
};

#undef DEF_PFE_ALLOC
#endif /* PFE_MALLOC_STATS */

/* Switch to turn On/Off macro validation.  
   Command line option --validate-pch enables macro validation,
   if it is disabled by default.  */
int pfe_macro_validation = 0;
int pfe_cmd_ln_macro_count = 0;
int pfe_macro_status = PFE_MACRO_NOT_FOUND;
/* Prototypes for static functions.  */

static void pfe_assign_dump_range_offsets   PARAMS ((int));
static void pfe_add_dump_range 		    PARAMS ((unsigned long, unsigned long));

static hashnode pfe_alloc_str_node  	    PARAMS ((hash_table *));
static hashnode pfe_alloc_include_hash_node PARAMS ((hash_table *));

#if PFE_MALLOC_STATS
static void pfe_s_display_malloc_stats	    PARAMS ((void));
#endif

/* To insure that the dir containing the pfe file does not
   have its mod date changed unless we have a successfull pfe
   file created we need to remember its mod date at the time
   the pfe file is opened.  So we stat the dir at that at
   that time then use it's mod date info to restore the dir's
   times if need be.  */
static struct stat pfe_file_stat;
static char *pfe_dirname;
static int pfe_dir_was_empty;
static int pfe_was_opened_for_dump;

/*-------------------------------------------------------------------*/

/* For PFE internal errors since the compiler's useless internal error 
   routine doesn't print the string you pass it if there were previous
   errors.  */
void
pfe_internal_error(msg)
     const char *msg;
{
  fprintf (stderr, "PFE internal error: %s.\n", msg);
  internal_error ("");
}

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

void 
pfe_init (action)
     enum pfe_action action;
{
  if (pfe_operation != PFE_NOT_INITIALIZED)
    pfe_internal_error ("pfe_init called again before termination");
  
  pfe_operation = action;
  if (pfe_operation == PFE_DUMP)
    {
      enum pfe_action saved_pfe_operation;
      
#if USE_APPLE_SCALABLE_MALLOC
      pfe_memory_zone = create_scalable_zone (0, 0);
#else
      pfem_init ();
#endif
      pfe_dump_range_table_size = PFE_INITIAL_DUMP_RANGE_TABLE_SIZE;
      pfe_dump_range_table = (pfe_dump_memory_range *) 
      			     xmalloc (sizeof (pfe_dump_memory_range) 
                                      * pfe_dump_range_table_size);
      pfe_compiler_state_ptr = (pfe_compiler_state *) 
      			       xcalloc (1, sizeof (pfe_compiler_state));
#if PFE_MALLOC_STATS
      pfe_s_malloc_table_size = PFE_S_MALLOC_TABLE_SIZE;
      pfe_s_malloc_table = (pfe_s_malloc_entry *) 
      			   xmalloc (sizeof (pfe_s_malloc_entry) 
                                    * pfe_s_malloc_table_size);
#endif
  
      pfe_cmd_ln_macro_count = 0;
      /* Init language specific compiler state (and set language).
         Note there is an asymetry of when we call pfe_lang_init
         between loading and dumping.  We do it here for dumping
         since language specific stuff is defined using the just
         created pfe_compiler_state_ptr.  But for loading we don't
         have a pfe_compiler_state_ptr until after we read the PFE
         file.  So we must wait until that happens before calling
         pfe_lang_init for the load side.  */
      (*lang_hooks.pfe_lang_init) (0);
      
      /* Initialize the space for the target specific additions.
         At this point pfe_target_additions is NULL which tells
         the routine defined by the macro PFE_TARGET_ADDITIONS to
         allocate the pfe space for its additions.  */
#ifdef PFE_TARGET_ADDITIONS
      PFE_TARGET_ADDITIONS (&pfe_compiler_state_ptr->pfe_target_additions);
#endif

      /* Create the hash table used by pfe_savestring to create
         a single instance of each unique string.  We temporarily
         pretend we are not doing a dump when we create this hash
         table so that it is not allocated in PFE memory because
         we don't want the table to go into the dump file.  */
      saved_pfe_operation = pfe_operation;
      pfe_str_hash_table = ht_create (13);
      /* FIXME: For the moment we will continue to create the hash 
         table in PFE memory because we need the obstack it uses 
         for hash table names needs to be in PFE memory.  Currently 
         there does not appear to be an easy way to get only the 
         obstack in PFE memory.  */
      pfe_operation = saved_pfe_operation;
      pfe_str_hash_table->alloc_node = pfe_alloc_str_node;

      /* Create hash table (in PFE memory) to keep list of included 
         headers.  */
      pfe_compiler_state_ptr->include_hash = ht_create (10); /* 2 ^ 10 = 1k entries.  */
      pfe_compiler_state_ptr->include_hash->alloc_node = pfe_alloc_include_hash_node;
      
      pfe_freeze_thaw_ptr_fp = pfe_freeze_ptr;
    }
  else if (pfe_operation == PFE_LOAD)
    {
      /* Nothing special required for loading at this time.  */
#if USE_APPLE_SCALABLE_MALLOC
      pfe_memory_zone = 0;
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
#endif
      pfe_freeze_thaw_ptr_fp = pfe_thaw_ptr;
    }
  else if (pfe_operation == PFE_NOP)
    {
      /* Not doing a load or dump.  The memory management routines will
         call the usual malloc and friends.  */
      pfe_freeze_thaw_ptr_fp = (pfe_freeze_thaw_ptr_t)NULL;
    }
  else
    pfe_internal_error ("pfe_init must specify PFE_DUMP, PFE_LOAD, "
    			"or PFE_NOP");
}

/* Shut down the PFE.  */

void 
pfe_term ()
{
#if PFE_MEMORY_STATS
  if (pfe_display_memory_stats  
      && ((pfe_operation == PFE_LOAD)
          || (pfe_operation == PFE_DUMP)))
    {
      printf ("\nPFE memory utilization summary:\n");
      printf ("pfe_operation               = %s\n", 
              (pfe_operation == PFE_LOAD) ? "PFE_LOAD" : "PFE_DUMP");
      printf ("pfe_malloc_total_size       = %d\n", pfe_malloc_total_size);
      printf ("pfe_load_buffer_size        = %d\n", pfe_load_buffer_size);
      printf ("pfe_mallocs                 = %d\n", pfe_mallocs);
      printf ("pfe_callocs                 = %d\n", pfe_callocs);
      printf ("pfe_reallocs                = %d\n", pfe_reallocs);
      printf ("pfe_frees                   = %d\n", pfe_frees);
      if (pfe_operation == PFE_DUMP)
        {
	  printf ("pfe_savestrings             = %d\n", 
	  	  pfe_savestrings);
	  printf ("pfe_freeze_ptrs             = %d\n", 
	  	  pfe_freeze_ptrs);
	  printf ("pfe_dump_range_table_nitems = %d\n", 
		  pfe_dump_range_table_nitems);
	  if (pfe_load_buffer_size <= pfe_malloc_total_size)
	    printf ("%% overhead in dump file     = ~0%%\n");
	  else
	    printf ("%% overhead in dump file     = %d%%\n", 
		    ((pfe_load_buffer_size - pfe_malloc_total_size) * 100)
		    / pfe_malloc_total_size);
        }
      else
        {
	  printf ("pfe_frees_from_load_area    = %d\n", 
	  	  pfe_frees_from_load_area);
	  printf ("pfe_thaw_ptrs               = %d\n", 
	  	  pfe_thaw_ptrs);
	  printf ("pfe_load_buffer             = 0x%x..0x%x\n", 
		  pfe_load_buffer_ptr, 
		  pfe_load_buffer_ptr + pfe_load_buffer_size - 1);
        }
#if PFE_MALLOC_STATS
      pfe_s_display_malloc_stats ();
#endif
    }
#endif

  if (pfe_operation == PFE_NOT_INITIALIZED)
    pfe_internal_error ("pfe_term called before pfe_init");
  else if (pfe_operation == PFE_DUMP)
    {
#if PFE_DEBUG_MEMMGR_RANGES
      int range_idx;
      printf ("\n");
      for (range_idx = 0; range_idx < pfe_dump_range_table_nitems; range_idx++)
        printf ("pfe_term: range: @0x%08X - 0x%08X [0x%08X] %d kb\n", 
                 pfe_dump_range_table[range_idx].start_addr, 
                 pfe_dump_range_table[range_idx].end_addr,
                 pfe_dump_range_table[range_idx].file_offset,
                 ((pfe_dump_range_table[range_idx].end_addr 
                   - pfe_dump_range_table[range_idx].start_addr 
                   + 512) / 1024));
#endif
#if USE_APPLE_SCALABLE_MALLOC
      /* We don't need to make the following call to destroy the
         pfe_memory_zone since this will be taken care of by the usual
         process termination clean up.  Besides, the scalable_malloc
         implementation of this call currently has a bug.  */
      //pfe_memory_zone->destroy (pfe_memory_zone);
      pfe_memory_zone = 0;
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
      pfem_term ();
#endif
      free (pfe_compiler_state_ptr);
    }
  else if (pfe_operation == PFE_LOAD)
    {
#if PFE_NO_THAW_LOAD
      if ((unsigned)pfe_load_buffer_ptr == PFE_NO_THAW_LOAD_ADDR)
        {
          /* FIXME: Do we need to do anything to deallocate this memory?  */
        }
      else
        free (pfe_load_buffer_ptr);
#else
      free (pfe_load_buffer_ptr);
#endif
      pfe_load_buffer_ptr = 0;
    }
  else
    {
      /* Not doing a load or dump.  Nothing to do.  */
    }

  pfe_operation = PFE_NOT_INITIALIZED;
  pfe_freeze_thaw_ptr_fp = (pfe_freeze_thaw_ptr_t)NULL;
}

/* When an -fload=pfe_dir or -fdump=pfe_dir option is detected
   this routine is called to open a PFE file in the specified
   directory.  For fdump=1 the pfe_dir is created if necessary.
   The PFE file opened is then pfe_dir/<lang>_<arch>.pfe,
   where arch is "ppc" or "i386" (passed to this routine) and
   lang is the language (c, c++, objc, or objc++) determined
   from lang_hooks.name.
   
   The function sets the globals pfe_name and pfe_file to the
   resulting PFE file pathname and the opened FILE stream
   respectively.  For dumping pfe_file_stat is also set with
   the stat info of the containing directory (pfe_dir).  */
void
pfe_open_pfe_file (pfe_dir, arch, fdump)
     char *pfe_dir, *arch;
     int fdump;
{
  int len = strlen (pfe_dir);
  char *pfe_basename, *pathname;
  
  pfe_term (); /* turns off PFE_NOP mode */
  pfe_init (fdump ? PFE_DUMP : PFE_LOAD);
  
  pfe_dirname = strcpy (xmalloc (len + 2), pfe_dir);
  if (pfe_dirname[len-1] == DIR_SEPARATOR)
    pfe_dirname[--len] = '\0';
  
  /* Create the PFE directory if it doesn't already exist.  Remember
     whether we are creating it now so that we may delete it if
     we need to delete the pfe file we create in it.  */
     
  if (fdump) /* mkdir -p */
    {
      int mk_status = mkdir (pfe_dirname, 0777);
      
      pfe_dir_was_empty = (mk_status == 0);
      pfe_was_opened_for_dump = 1;
      
      if (mk_status != 0 && errno != EEXIST)
        fatal_error ("Cannot create pre-compiled header directory: \"%s\" (%s)",
          	     pfe_dirname, xstrerror (errno));
    }
  else
    pfe_dir_was_empty = pfe_was_opened_for_dump = 0;
    
  pfe_dirname[len]   = DIR_SEPARATOR;
  pfe_dirname[len+1] = '\0';
  
  if (fdump && stat (pfe_dirname, &pfe_file_stat) != 0)
    fatal_error ("Cannot get mod time for pre-compiled header directory: \"%s\" (%s)",
		 pfe_dirname, xstrerror (errno));
  
  pfe_basename = xmalloc (11 + 4 + 4 + 1);
  if (strcmp (lang_hooks.name, "GNU C") == 0)
    strcat (strcpy (pfe_basename, "cc1_"), arch);
  else if (strcmp (lang_hooks.name, "GNU C++") == 0) 
    strcat (strcpy (pfe_basename, "cc1plus_"), arch);
  else if (strcmp (lang_hooks.name, "GNU Objective-C") == 0) 
    strcat (strcpy (pfe_basename, "cc1obj_"), arch);
  else if (strcmp (lang_hooks.name, "GNU Objective-C++") == 0) 
    strcat (strcpy (pfe_basename, "cc1objplus_"), arch);
                              /*   12345678901   */
  strcat (pfe_basename, ".pfe");
  pathname = strcat (strcpy (xmalloc (strlen (pfe_dirname) + strlen (pfe_basename) + 1),
                             pfe_dirname), pfe_basename);
  
  pfe_file = fopen (pathname, fdump ? "w" : "r");
    
  free (pfe_basename);
  
  pfe_name = pathname;
}

/* Close a currently opened pfe file and optionally delete it.
   The only time it would get deleted is for a incomplete compilation
   (e.g., the compilation is interrupted) when doing a dump.   We
   don't want partially created dump file lying around.  */
void
pfe_close_pfe_file (int delete_it)
{
  if (pfe_file)
    {
      fclose (pfe_file);
      pfe_file = NULL;
      
      /* If we just closed a pfe dump file that we just created then
         we want to update the containing directory's mod date unless
         we terminated abnormally (e.g., errors or an interrupt), in
         which case delete_it is set.  For the abnormal terminations
         we either set the containing directory mod date back to its
         original value or delete the directory if we created it
         because this is the first dump file in that directory.  
         
         Note we use pfe_was_opened_for_dump to determine whether
         we're dumping rather than pfe_operation since pfe_term() is
         usually called prior to calling pfe_close_pfe_file().  */

      if (pfe_was_opened_for_dump && pfe_name && *pfe_name)
        {
	  if (delete_it)
	    {
	      struct timeval tval[2];

	      unlink (pfe_name);
	      
	      if (pfe_dir_was_empty)
	        rmdir (pfe_dirname);
	      else
	        {
		  TIMESPEC_TO_TIMEVAL (&tval[0], &pfe_file_stat.st_atimespec);
		  TIMESPEC_TO_TIMEVAL (&tval[1], &pfe_file_stat.st_mtimespec);
		  if (utimes (pfe_dirname, tval) != 0)
		    fatal_error ("Cannot reset mod time for pre-compiled header directory: \"%s\" (%s)",
				 pfe_dirname, xstrerror (errno));
	      }
	    }
	  else if (utimes (pfe_dirname, NULL) != 0)
	    fatal_error ("Cannot set mod time for pre-compiled header directory: \"%s\" (%s)",
			 pfe_dirname, xstrerror (errno));
	}
    }
}

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

int 
pfe_dump (the_file, the_freezer, the_header, the_header_size)
     FILE *the_file;
     void (*the_freezer) (void *);
     void *the_header;
     size_t the_header_size;
{
  int range_idx, range_size, i;
  long bytes_written;
  int pad_bytes;

  timevar_push (TV_PFE_WRITE);
    
  if (pfe_operation != PFE_DUMP)
    pfe_internal_error ("pfe_dump called when not initialized for dumping");
    
#if 0
  printf ("include_hash hash table statistices:\n");
  ht_dump_statistics (pfe_compiler_state_ptr->include_hash);
#endif

  /* Prevent PFE memory allocations after we have assigned disk
     offsets for the memory chunks we will write out.  */
    
  pfe_memory_locked_for_dump = 1;
    
  /* Phase 1: Find the "chunks" allocated by the memory manager;
     assign them offsets in the dump file, and create a table with
     <location, size, file offset> information for each chunk.  */
    
#if USE_APPLE_SCALABLE_MALLOC
  scalable_zone_find_ranges (pfe_memory_zone, pfe_add_dump_range);
#else
  /* FIXME: Provide portable implementation for FSF someday.  */
  pfem_id_ranges (pfe_add_dump_range);
#endif
  pfe_assign_dump_range_offsets (the_header_size);

#if PFE_NO_THAW_LOAD
  /* Validate the assumption that all of the memory that we are going
     to freeze lies below the address at which we will attempt to
     load it.  Otherwise default to the usual 0-based file offsets.  */
  if (pfe_dump_range_table [pfe_dump_range_table_nitems - 1].end_addr
      < PFE_NO_THAW_LOAD_ADDR)
    pfe_assumed_load_address = PFE_NO_THAW_LOAD_ADDR;
  ((pfe_compiler_state *)the_header)->pfe_assumed_load_address 
    = pfe_assumed_load_address;
#endif

  
  /* Phase 2: Walk the compiler data structures and "freeze" all
     pointers by converting them to offsets within the dump file.  */
  
  if (the_freezer == 0)
    pfe_internal_error ("pfe_dump: dump freezer function not specified");
    
  if (the_header == 0)
    pfe_internal_error ("pfe_dump: dump header not specified");
    
  timevar_push (TV_PFE_FREEZE);
  (*the_freezer) (the_header);
  timevar_pop (TV_PFE_FREEZE);
  
  /* Phase 3: Walk the table of memory manager ranges and write
     them into the dump file.  The header is written at the
     beginning of the file.  */

#if PFE_DEBUG_MEMMGR_RANGES
  printf ("\npfe_dump: writing header\n");
#endif
  fwrite (the_header, the_header_size, 1, the_file);
  bytes_written = the_header_size;
  for (range_idx = 0; range_idx < pfe_dump_range_table_nitems; range_idx++)
    {
      pad_bytes = pfe_dump_range_table[range_idx].file_offset - bytes_written;
#if PFE_DEBUG_MEMMGR_RANGES
      if (pad_bytes) printf ("pfe_dump: writing %d pad bytes\n", pad_bytes);
#endif
      for (i = 0; i < pad_bytes; i++)
        fputc (0, the_file);
      bytes_written += pad_bytes;
#if PFE_DEBUG_MEMMGR_RANGES
      printf ("pfe_dump: writing range %d: 0x%08X - 0x%08X [0x%08X]\n", 
              range_idx,
              pfe_dump_range_table[range_idx].start_addr,
              pfe_dump_range_table[range_idx].end_addr,
              pfe_dump_range_table[range_idx].file_offset);
#endif
      range_size = (pfe_dump_range_table[range_idx].end_addr 
                    - pfe_dump_range_table[range_idx].start_addr);
      fwrite ((char *)pfe_dump_range_table[range_idx].start_addr,
              range_size, 1, the_file);
      bytes_written += range_size;
    }
  
  pfe_memory_locked_for_dump = 0;
  
  timevar_pop (TV_PFE_WRITE);

  return 0;
}

/* Variant of pfe_dump which assumes that pfe_compiler_state_ptr is the
   pointer to the state header to be saved and that pfe_freeze_compiler_state
   is the function to be called to save (and "freeze") the compiler state.  */
   
int 
pfe_dump_compiler_state (the_file)
     FILE *the_file;
{
  return pfe_dump (the_file, pfe_freeze_compiler_state, 
  		   pfe_compiler_state_ptr, sizeof (pfe_compiler_state));
}

/* Read in the precompiled header from the specified file.  Return a
   pointer to the header with compiler data structure "roots".  The
   data structures just loaded should then be "thawed" by walking
   all of the data structures pointed to by the roots and thawing
   all pointers.
   
   This is the generic function to do a load, and can be used to test
   the load/dump mechanism independent of the compiler.  The 
   pfe_load_compiler_state calls this function to load the compiler 
   state.  */

void * 
pfe_load (the_file)
     FILE *the_file;
{
  struct stat file_info;
  
  if (pfe_operation != PFE_LOAD)
    pfe_internal_error ("pfe_load called when not initialized for loading");
    
  if (fstat (fileno (the_file), &file_info) != 0)
    fatal_error ("PFE: unable to get file information for load file");
  pfe_load_buffer_size = file_info.st_size;
#if PFE_NO_THAW_LOAD
  {
    /* Try to allocate the load buffer area at the specific address
       (PFE_NO_THAW_LOAD_ADDR) assumed by the "no thaw load" mechanism.
       If this fails, use xmalloc.  */
#if PFE_NO_THAW_LOAD_USING_MMAP
    caddr_t taddr;

    taddr = (caddr_t)PFE_NO_THAW_LOAD_ADDR;
    taddr = mmap (taddr, 
    		  pfe_load_buffer_size,
    		  PROT_READ | PROT_WRITE,
    		  MAP_FILE | MAP_FIXED | MAP_PRIVATE,
    		  fileno (the_file),
    		  (off_t)0);
    if (taddr == (caddr_t)PFE_NO_THAW_LOAD_ADDR)
      {
#if PFE_NO_THAW_LOAD_DIAGNOSTICS
        printf ("pfe_load: mmap succeeded.\n");
#endif
        pfe_load_buffer_ptr = (char *)PFE_NO_THAW_LOAD_ADDR;
      }
    else
      {
#if PFE_NO_THAW_LOAD_DIAGNOSTICS
        printf ("pfe_load: mmap failed (returned &d; errno = %d); "
        	"retrying using malloc.\n",
        	(int)taddr, errno);
#endif
        pfe_load_buffer_ptr = (char *) xmalloc (pfe_load_buffer_size);
      }
#else
    int return_code;
    unsigned int *taddr;
    
    taddr = (unsigned int *)PFE_NO_THAW_LOAD_ADDR;
    return_code = vm_allocate (mach_task_self (), 
    			       (vm_address_t *)&taddr,
    			       pfe_load_buffer_size,
    			       0);
    if (return_code == KERN_SUCCESS)
      pfe_load_buffer_ptr = (char *)PFE_NO_THAW_LOAD_ADDR;
    else
      {
#if PFE_NO_THAW_LOAD_DIAGNOSTICS
        printf ("pfe_load: vm_allocate failed; retrying using malloc.\n");
#endif
        pfe_load_buffer_ptr = (char *) xmalloc (pfe_load_buffer_size);
      }
#endif
  }
#else
  pfe_load_buffer_ptr = (char *) xmalloc (pfe_load_buffer_size);
#endif
  if (pfe_load_buffer_ptr == 0)
    fatal_error ("PFE: unable to allocate memory (%d bytes) for load file", 
             	   pfe_load_buffer_size);
#if PFE_NO_THAW_LOAD
#if PFE_NO_THAW_LOAD_USING_MMAP
#else
  if ((fread (pfe_load_buffer_ptr, 1, pfe_load_buffer_size, the_file) 
       != pfe_load_buffer_size))
    fatal_io_error ("PFE: unable to read load file");
#endif
#else
  if ((fread (pfe_load_buffer_ptr, 1, pfe_load_buffer_size, the_file) 
       != pfe_load_buffer_size))
    fatal_io_error ("PFE: unable to read load file");
#endif

  return pfe_load_buffer_ptr;
}

/* Read in the precompiled header from the specified file and restore
   the compiler state.  The pfe_thaw_compiler_state function will be
   called to thaw the data in the compiler state header and copy
   the appropriate values back to compiler globals.  */

void 
pfe_load_compiler_state (the_file)
     FILE *the_file;
{
  timevar_push (TV_PFE_READ);
  pfe_compiler_state_ptr = pfe_load (the_file);
  timevar_pop (TV_PFE_READ);
  
#if PFE_NO_THAW_LOAD
  /* If the load file was created with a particular assumed load
     address, see whether we were able to secure that address.  If
     so, then set the "pfe_no_thaw_load" flag.  Otherwise we will
     have to go through the normal thawing process.  If we will be
     thawing, we compute the pointer adjustment required at this
     time.  */
  pfe_assumed_load_address = pfe_compiler_state_ptr->pfe_assumed_load_address;
  if (pfe_assumed_load_address != 0)
    {
      if ((unsigned)pfe_load_buffer_ptr == pfe_assumed_load_address)
        pfe_no_thaw_load = 1;
      else if ((unsigned)pfe_load_buffer_ptr + pfe_load_buffer_size 
	       > PFE_NO_THAW_LOAD_ADDR)
        fatal_error ("PFE: unable allocate load area at or below assumed load address");
      else
      	pfe_load_offset_adjust = (unsigned)pfe_load_buffer_ptr 
      				 - pfe_assumed_load_address;
    }
  else
    pfe_load_offset_adjust = (unsigned)pfe_load_buffer_ptr;
#endif

  timevar_push (TV_PFE_THAW);
  pfe_thaw_compiler_state (pfe_compiler_state_ptr);
  timevar_pop (TV_PFE_THAW);
}

#if PFE_NO_THAW_LOAD
/* Determine whether a pointer is frozen.  */
int
pfe_is_frozen (ptr)
     void *ptr;
{
  if (pfe_no_thaw_load)
    return 0;
  else if (pfe_assumed_load_address == 0)
    return (unsigned)ptr & 1;
  else
    return (unsigned)ptr > PFE_NO_THAW_LOAD_ADDR;
}
#endif

/* Determine whether a pointer points into PFE allocated memory.  This
   routine will return a non-zero result if the pointer is to memory
   in the area managed by the PFE when dumping or in the load memory
   area when loading.  Otherwise zero will be returned, including 
   when the pointer is to memory allocated by the PFE during a load
   but outside of the load memory area (i.e., for stuff not in the
   load file) and for memory allocated via PFE calls when not doing
   a load or dump.  */

int
pfe_is_pfe_mem (ptr)
     void *ptr;
{
  if (pfe_operation == PFE_DUMP)
    {
#if USE_APPLE_SCALABLE_MALLOC
      return scalable_zone_ptr_is_in_zone (pfe_memory_zone, ptr);
#else
      return pfem_is_pfe_mem (ptr);
#endif
    }
  else if (pfe_operation == PFE_LOAD)
    {
      if (((unsigned)ptr >= (unsigned)pfe_load_buffer_ptr) 
	  && ((unsigned)ptr < (unsigned)(pfe_load_buffer_ptr 
	  				 + pfe_load_buffer_size)))
    	return 1;
      else
	return 0;
    }
  return 0;
}

/* Convert a pointer to an offset so that it can be saved to a file.  
   The parameter is a pointer to the pointer in question.  Returns
   the original pointer before freezing.  */

void * 
pfe_freeze_ptr (pp)
     void *pp;
{
  char *p;
  unsigned long range_file_offset = 0;
  unsigned long offset_in_range = 0;
    
  p = *(char **)pp;
  if (!p)
    return NULL;
    
  if (!PFE_IS_FROZEN(p))
    {
#if PFE_MEMORY_STATS
      pfe_freeze_ptrs++;
#endif

      /* Determine what memory manager fragment the pointer points
         to.  The file offset to return is the file offset of that
         memory manager fragment plus the offset of the pointer
         within the fragment.  */
      {
        int lower_idx = 0;
        int upper_idx = pfe_dump_range_table_nitems - 1;
        int middle_idx;
        
        while (lower_idx <= upper_idx)
          {
            middle_idx = (lower_idx + upper_idx) / 2;
            if ((unsigned)p < pfe_dump_range_table[middle_idx].start_addr)
              upper_idx = middle_idx - 1;
            else if ((unsigned)p >= pfe_dump_range_table[middle_idx].end_addr
            			    + pfe_allowed_ptr_variance)
              lower_idx = middle_idx + 1;
            else
              {
		range_file_offset = pfe_dump_range_table[middle_idx].file_offset;
		offset_in_range = (unsigned)p 
				  - pfe_dump_range_table[middle_idx].start_addr;
		break;
              }
          }
      }
      
      if ((range_file_offset == 0) && (offset_in_range == 0))
   	{
          /* We were asked to freeze a pointer that does not point to memory
             we allocated.  Return the pointer as is.  Write out a diagnostic
             message since this should not happen.  */
          fprintf (stderr, 
                   "pfe_freeze_ptr: 0x%08X @ 0x%08X is not a pointer to PFE memory.\n", 
                   p, pp);
          return (void *)p; 
        }
      else
        {
#if PFE_DEBUG_MEMMGR_RANGES && 0
          printf ("pfe_freeze_ptr: 0x%08X @ 0x%08X -> 0x%08X\n", p, pp, 
                  (range_file_offset + offset_in_range));
#endif
        }
#if PFE_NO_THAW_LOAD
      /* If we are assuming a load address, the pointer is marked as
         as frozen by the fact that it is offset by the load address.
         Otherwise it is frozen by making it odd.  */
      if (pfe_assumed_load_address == 0)
        *(unsigned long *)pp = (range_file_offset + offset_in_range) | 1;
      else
        *(unsigned long *)pp = range_file_offset + offset_in_range 
        		       + pfe_assumed_load_address;
#else
      *(unsigned long *)pp = (range_file_offset + offset_in_range) | 1;
#endif
    }
  else
    {
#if PFE_DEBUG_MEMMGR_RANGES
      printf ("pfe_freeze_ptr: 0x%08X @ 0x%08X already frozen\n", p, pp);
#endif
    }
    
  return (void *)p;
}

/* Convert an offset to a pointer after it has been restored from a 
   file.  The parameter is a pointer to the offset that will be
   converted to a pointer.  Returns the thawed pointer.  */

void * 
pfe_thaw_ptr (pp)
     void *pp;
{
  unsigned long offset;
  
#if PFE_DEBUG_EXTRA_CHECKING
  /* We should not be thawing a pointer that is not itself located
     in the area of memory for the load file.  */
  if ((pfe_extra_checking)
      && (((unsigned)pp < (unsigned)pfe_load_buffer_ptr) 
          || ((unsigned)pp 
              >= (unsigned)(pfe_load_buffer_ptr 
              		    + pfe_load_buffer_size
              		    + pfe_allowed_ptr_variance)))
      && (pp != pfe_temp_ptr))
    fprintf (stderr, 
	     "pfe_thaw_ptr: trying to thaw a pointer @ 0x%08X that"
	     " is not in the load file.\n", 
	     pp);
#endif
    
#if PFE_NO_THAW_LOAD
  /* We don't do anything to thaw a pointer in the "no thaw load"
     scheme if we were able to assume load address and secure it
     at load time.  */
  if (pfe_no_thaw_load)
    return *(void **)pp;
#endif

  offset = *(unsigned long *)pp;

  if (PFE_IS_FROZEN ((void *)offset))
    {
#if PFE_MEMORY_STATS
      pfe_thaw_ptrs++;
#endif
#if PFE_DEBUG_EXTRA_CHECKING
      /* We have a problem if what is supposed to be a file offset is 
         bigger than the size of the file.  */
#if PFE_NO_THAW_LOAD
      if (pfe_extra_checking 
          && (offset - pfe_assumed_load_address) 
              > (pfe_load_buffer_size + pfe_allowed_ptr_variance))
#else
      if (pfe_extra_checking 
          && (offset & 0xFFFFFFFE) > (pfe_load_buffer_size 
          			      + pfe_allowed_ptr_variance))
#endif
	fprintf (stderr, 
		 "pfe_thaw_ptr: 0x%08X @ 0x%08X is not a pointer to PFE memory.\n", 
		 offset & 0xFFFFFFFE, pp);
#endif
#if PFE_NO_THAW_LOAD
      *(void **)pp = (void *)((offset & 0xFFFFFFFE)
        		      + pfe_load_offset_adjust);
#else
      *(void **)pp = (void *)((unsigned)pfe_load_buffer_ptr 
     			      + (offset & 0xFFFFFFFE));
#endif
    }

  return *(void **)pp;
}

/* Freeze/thaw a pointer (passed as a pointer to that pointer) as
   specified by the enum pfe_action.  Return original pointer as
   the function result or NULL if the pointer if already frozen
   or thawed (depending on enum pfe_action).  */

void *
pfe_freeze_thaw_ptr (pp)
     void **pp;
{
  void *p;
  
  if (pfe_operation == PFE_DUMP)
    {
      p = *pp;
      if (p == NULL || PFE_IS_FROZEN (p))
        return NULL;
      pfe_freeze_ptr (pp);
    }
  else
    {
      if (!PFE_IS_FROZEN ((void *)(*(unsigned long *)pp)))
        return NULL;
      p = pfe_thaw_ptr (pp);
    }
    
  return p;
}


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

void *
pfe_freeze_thaw_ptr_with_variance (pp, n)
     void **pp;
     unsigned long n;
{
  void *p;
  
  pfe_allowed_ptr_variance = n;
  
  if (pfe_operation == PFE_DUMP)
    {
      p = *pp;
      if (p == NULL || PFE_IS_FROZEN (p))
        return NULL;
      pfe_freeze_ptr (pp);
    }
  else
    {
      if (!PFE_IS_FROZEN ((void *)(*(unsigned long *)pp)))
        return NULL;
      p = pfe_thaw_ptr (pp);
    }
  
  pfe_allowed_ptr_variance = 0;
    
  return p;
}

/* Convert what might be a frozen pointer to a real pointer.  Returns 
   non-frozen pointers as is.  This can be called during a load or a 
   dump.  (For internal diagnostic purposes.)  */

void *
pfe_real_ptr (p)
     void *p;
{
  unsigned long offset;
  
  if (pfe_operation == PFE_DUMP)
    {
      if (PFE_IS_FROZEN (p))
	{
	  int lower_idx = 0;
	  int upper_idx = pfe_dump_range_table_nitems - 1;
	  int middle_idx;
	  
#if PFE_NO_THAW_LOAD
	  if (pfe_assumed_load_address)
	    offset = (unsigned)p - pfe_assumed_load_address;
	  else
#endif	    
	    offset = (unsigned)p & 0xFFFFFFFE;
	  
	  while (lower_idx <= upper_idx)
	    {
	      middle_idx = (lower_idx + upper_idx) / 2;
	      if (offset < pfe_dump_range_table[middle_idx].file_offset)
		upper_idx = middle_idx - 1;
	      else if (offset >= (pfe_dump_range_table[middle_idx].end_addr
				  - pfe_dump_range_table[middle_idx].start_addr
				  + pfe_dump_range_table[middle_idx].file_offset))
		lower_idx = middle_idx + 1;
	      else
		return (void *)(offset 
				- pfe_dump_range_table[middle_idx].file_offset
				+ pfe_dump_range_table[middle_idx].start_addr);
	    }
	  pfe_internal_error ("pfe_real_ptr could not thaw frozen ptr");
	}
    }
  else if (pfe_operation == PFE_LOAD)
    {
      if (PFE_IS_FROZEN (p))
	{
	  offset = (unsigned)p & 0xFFFFFFFE;
#if PFE_NO_THAW_LOAD
	  return (void *)((unsigned)pfe_load_offset_adjust + offset);
#else
	  return (void *)((unsigned)pfe_load_buffer_ptr + offset);
#endif	    
	}
    }
    
  return p;
}

/* Identify temporaries not in PFE memory being using during the
   freezing/thawing process.  Identifying the temporary currently
   in use will suppress diagnostics about trying to freeze/thaw
   a pointer not in PFE memory.  */
void 
pfe_using_temp_ptr (p)
     void **p;
{
  pfe_temp_ptr = p;
}


/* PFE's malloc: allocates memory in a "zone" controlled by the PFE when 
   performing a dump, otherwise it calls the normal malloc.  */

void * 
pfe_malloc (size)
     size_t size;
{
  void *p;

#if PFE_MEMORY_STATS
  pfe_malloc_total_size += size;
  pfe_mallocs++;
#endif

  if (pfe_operation == PFE_DUMP)
    {
#if PFE_DEBUG_EXTRA_CHECKING
      if (pfe_memory_locked_for_dump)
    	pfe_internal_error ("pfe_malloc: allocation routine called "
    			    "when freezing");
#endif
#if USE_APPLE_SCALABLE_MALLOC
      /* really_call_malloc is actual a macro to generate malloc to 
         get around the "poisoning" of malloc in system.h.  */
      p =  pfe_memory_zone->really_call_malloc (pfe_memory_zone, size);
#if 0
      printf ("pfe_malloc: 0x%08X size %d\n", p, size);
#endif
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
      p =  pfem_malloc (size);
#endif
      if (!p)
    	fatal_error ("PFE: memory exhausted");
    }
  else
    p = xmalloc (size);
  
  return p;
}

/* PFE's calloc: allocates and zeros memory for n objects in a "zone" 
   controlled by the PFE when performing a dump, otherwise it calls
   the normal calloc.  */

void * 
pfe_calloc (nobj, size)
     size_t nobj;
     size_t size;
{
  void *p;
  
#if PFE_MEMORY_STATS
  pfe_malloc_total_size += (size * nobj);
  pfe_callocs++;
#endif

  if (pfe_operation == PFE_DUMP)
    {
#if PFE_DEBUG_EXTRA_CHECKING
      if (pfe_memory_locked_for_dump)
    	pfe_internal_error ("pfe_calloc: allocation routine called "
    			    "when freezing");
#endif
#if USE_APPLE_SCALABLE_MALLOC
      /* really_call_calloc is actual a macro to generate calloc to 
         get around the "poisoning" of calloc in system.h.  */
      p = pfe_memory_zone->really_call_calloc (pfe_memory_zone, nobj, size);
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
      p = pfem_calloc (nobj, size);
#endif
      if (!p)
    	fatal_error ("PFE: memory exhausted");
    }
  else
    p = xcalloc (nobj, size);
  
  return p;
}

/* PFE's realloc: reallocates a pointer in a "zone" controlled by the
   PFE when performing a dump, otherwise it calls the normal realloc.  */

void * 
pfe_realloc (p, size)
     void *p;
     size_t size;
{
#if PFE_MEMORY_STATS
  pfe_malloc_total_size += size;
  pfe_reallocs++;
#endif

  if (pfe_operation == PFE_DUMP)
    {
#if PFE_DEBUG_EXTRA_CHECKING
      if (pfe_memory_locked_for_dump)
    	pfe_internal_error ("pfe_realloc: allocation routine called "
    			    "when freezing");
#endif
#if USE_APPLE_SCALABLE_MALLOC
      /* really_call_realloc is actual a macro to generate calloc to 
         get around the "poisoning" of realloc in system.h.  */
      p = pfe_memory_zone->really_call_realloc (pfe_memory_zone, p, size);
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
      p = pfem_realloc (p, size);
#endif
      if (!p)
    	fatal_error ("PFE: memory exhausted");
    }
  else if (pfe_operation == PFE_LOAD && pfe_is_pfe_mem (p))
    {
      /* We can't do a xrealloc if the memory being realloc'ed is in
         PFE load memory because the pointer will not correspond to
         anything allocated by malloc.  So we do a normal [x]malloc,
         memcpy the contents, and do no free because the object in
         PFE memory cannot be freed.  The memcpy is also tricky 
         because we don't know the size of the original allocation
         in PFE memory, so we have to do a copy that is the size of
         the new allocation (limited in the case where the original
         is closer than that size to the end of PFE memory).  */
      char *newp;
#if PFE_DEBUG_EXTRA_CHECKING && 0
      printf ("pfe_realloc: realloc from PFE memory: 0x%08X; "
      	      "new size %d\n", p, size);
#endif
      newp = xmalloc (size);
      if (((unsigned)p + size) > ((unsigned)pfe_load_buffer_ptr 
                                  + pfe_load_buffer_size))
        p = memcpy (newp, p, (size_t)((unsigned)pfe_load_buffer_ptr 
        		     	      + pfe_load_buffer_size 
        		     	      - (unsigned)p));
      else
      	p = memcpy (newp, p, size);
    }
  else
    p = xrealloc (p, size);
  
  return p;
}

/* PFE's free: free memory in a "zone" controlled by the PFE when 
   performing a dump, otherwise it calls the normal free except
   when the memory is in the load memory area.  */

void 
pfe_free (ptr)
     void *ptr;
{
  if (!ptr) return;

#if PFE_MEMORY_STATS
  pfe_frees++;
#endif

  if (pfe_operation == PFE_DUMP)
    {
#if USE_APPLE_SCALABLE_MALLOC
      pfe_memory_zone->free (pfe_memory_zone, ptr);
#else
      /* FIXME: Provide portable implementation for FSF someday.  */
      pfem_free (ptr);
#endif
    }
  else if (pfe_operation == PFE_LOAD)
    {
      /* Don't do frees out of PFE load memory because it was all 
         allocated with a single malloc call.  */
      if (((unsigned)ptr < (unsigned)pfe_load_buffer_ptr) 
	  || ((unsigned)ptr >= ((unsigned)pfe_load_buffer_ptr 
	  			+ pfe_load_buffer_size)))
        free (ptr);
#if PFE_MEMORY_STATS
      else
        pfe_frees_from_load_area++;
#endif
    }
  else
    {
      free (ptr);
    }
}

#if PFE_MALLOC_STATS
/* Keep track of malloc allocations by kind and size.  */

void
pfe_s_track_mallocs (size, kind)
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  int lower_idx, upper_idx, middle_idx, new_entry_sorts_higher;

  /* Check whether we need to grow the size of the malloc table.  */
  if (pfe_s_malloc_table_nitems >= pfe_s_malloc_table_size)
    {
      pfe_s_malloc_table_size += PFE_S_MALLOC_TABLE_INCR;
      pfe_s_malloc_table = (pfe_s_malloc_entry *) 
                             xrealloc (pfe_s_malloc_table, 
                                       sizeof (pfe_s_malloc_entry) 
                                       * pfe_s_malloc_table_size);
    }
  if (pfe_s_malloc_table_nitems == 0)
    middle_idx = 0;
  else
    {
      /* Entries in the malloc table need to be sorted by kind and size.  
	 Perform an insertion sort.  Find where the entry goes, shift the 
	 rest of the table up, and fill in the range entry.  */
      lower_idx = 0;
      upper_idx = pfe_s_malloc_table_nitems - 1;
      
      while (lower_idx <= upper_idx)
	{
	  new_entry_sorts_higher = 0;
	  middle_idx = (lower_idx + upper_idx) / 2;
	  if (kind < pfe_s_malloc_table[middle_idx].kind)
	    upper_idx = middle_idx - 1;
	  else if (kind > pfe_s_malloc_table[middle_idx].kind)
	    {
	      lower_idx = middle_idx + 1;
	      new_entry_sorts_higher = 1;
	    }
	  else
	    {
	      // The kinds are the same, now sort on the size.
	      if (size < pfe_s_malloc_table[middle_idx].size)
		upper_idx = middle_idx - 1;
	      else if (size > pfe_s_malloc_table[middle_idx].size)
		{
		  lower_idx = middle_idx + 1;
		  new_entry_sorts_higher = 1;
		}
	      else
		{
		  pfe_s_malloc_table[middle_idx].count++;
		  return;
		}
	    }
	}
      
      /* If we get here there was no existing entry with the same kind and
	 size, so we have to create a new entry.  This may involve moving
	 part of the table up.  */
      
      if (new_entry_sorts_higher)
        middle_idx++;
      
      if (middle_idx < pfe_s_malloc_table_nitems)
	memmove (&pfe_s_malloc_table[middle_idx + 1], 
		 &pfe_s_malloc_table[middle_idx], 
		 sizeof (pfe_s_malloc_entry)
		 * (pfe_s_malloc_table_nitems - middle_idx));
    }

  /* Add a new entry to the table.  */
  
  pfe_s_malloc_table[middle_idx].size = size;
  pfe_s_malloc_table[middle_idx].kind = kind;
  pfe_s_malloc_table[middle_idx].count = 1;
  pfe_s_malloc_table_nitems++; 
}

/* Cover routine for tracking malloc calls by kind.  */

void * 
pfe_s_malloc (size, kind)
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  if (pfe_display_memory_stats)
    pfe_s_track_mallocs (size, kind);
  return pfe_malloc (size);
}

void * 
pfe_s_calloc (nobj, size, kind)
     size_t nobj;
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  if (pfe_display_memory_stats)
    pfe_s_track_mallocs (nobj * size, kind);
  return pfe_calloc (nobj, size);
}

void * 
pfe_s_realloc (p, size, kind)
     void *p;
     size_t size;
     enum pfe_alloc_object_kinds kind;
{
  if (pfe_display_memory_stats)
    pfe_s_track_mallocs (size, kind);
  return pfe_realloc (p, size);
}

static void
pfe_s_display_malloc_stats(void)
{
  int idx;
  
  printf ("\nPFE malloc usage statistics:\n");
  for (idx = 0; idx < pfe_s_malloc_table_nitems; idx++)
    printf ("%-30s: size(%6d): count(%5d)\n", 
            pfe_alloc_object_kinds_names[pfe_s_malloc_table[idx].kind], 
            pfe_s_malloc_table[idx].size, 
            pfe_s_malloc_table[idx].count); 
}

#endif /* PFE_MALLOC_STATS  */

/* Allocate a hash table node for a string.  The node is not allocated
   in PFE memory because the hash table itself is only used while doing
   a dump.  When loading, we only need the strings themselves.  */

static hashnode
pfe_alloc_str_node (table)
     hash_table *table ATTRIBUTE_UNUSED;
{
  return xmalloc (sizeof (hashnode));
}

/* Allocate a hash table node for an include entry in PFE memory. */
static hashnode 
pfe_alloc_include_hash_node (table)
     hash_table *table ATTRIBUTE_UNUSED;
{    
  return (hashnode) pfe_malloc (sizeof (pfe_include_header));
} 

/* When doing a dump, allocate a copy of the specified string using 
   pfe_malloc.  When not doing a dump, just return the original string.
   
   Caution: We use a hash table to ensure that there is only one copy
   of each unique string.  So don't use this to allocate strings that
   are not const.  */

void *
pfe_savestring (s)
     char *s;
{
  if (!s)
    return NULL;

  if (pfe_operation == PFE_DUMP)
    {
      hashnode ht_node;

      /* PFE_TARGET_MAYBE_SAVESTRING, if defined, is an additional
	 target-specific condition we may impose on whether to have
	 pfe_savestring() allocate the PFE memory space for the string
	 or not.  */
#ifdef PFE_TARGET_MAYBE_SAVESTRING
     if (! PFE_TARGET_MAYBE_SAVESTRING (s))
       return s;
#endif
  
      ht_node = ht_lookup (pfe_str_hash_table,
			   (const unsigned char *) s,
			   strlen (s), HT_ALLOC);
#if PFE_MEMORY_STATS
      pfe_savestrings++;
#endif
      /* ht_node can't be NULL here.  */
      return (void *)ht_node->str;
    }
  else
    return s;
}

/* Call-back routine for PFE memory manager to identify ranges of
   memory in use and to be saved by "dump".  This routine fills the
   pfe_dump_range_table with entries for each memory range.  */

static void 
pfe_add_dump_range (start_addr, end_addr)
     unsigned long start_addr;
     unsigned long end_addr;
{
  int range_idx;

#if PFE_DEBUG_MEMMGR_RANGES
  printf ("pfe_add_dump_range: @0x%08X - 0x%08X\n", start_addr, end_addr);
#endif
  /* Check whether we need to grow the size of the dump range table.  */
  if (pfe_dump_range_table_nitems >= pfe_dump_range_table_size)
    {
      pfe_dump_range_table_size += PFE_DUMP_RANGE_TABLE_INCR;
      pfe_dump_range_table = (pfe_dump_memory_range *) 
                             xrealloc (pfe_dump_range_table, 
                                       sizeof (pfe_dump_memory_range) 
                                       * pfe_dump_range_table_size);
    }
  /* Entries in the dump range table need to be sorted by address.  Perform
     an insertion sort.  Find where the entry goes, shift the rest of the
     table up, and fill in the range entry.  */
  for (range_idx = 0; range_idx < pfe_dump_range_table_nitems; range_idx++)
    if (start_addr < pfe_dump_range_table[range_idx].start_addr)
      {
        memmove (&pfe_dump_range_table[range_idx + 1], 
                 &pfe_dump_range_table[range_idx], 
                 sizeof (pfe_dump_memory_range) 
                 * (pfe_dump_range_table_nitems - range_idx));
        break;
      }
  pfe_dump_range_table[range_idx].start_addr = start_addr;
  pfe_dump_range_table[range_idx].end_addr = end_addr;
  pfe_dump_range_table_nitems++;
}

/* Assign the file offsets for the memory ranges that will be written to
   the dump file.  */

static void
pfe_assign_dump_range_offsets (int header_size)
{
  int range_idx;
  long dump_file_offset = header_size;
  
  for (range_idx = 0; range_idx < pfe_dump_range_table_nitems; range_idx++)
    {
#if 0 /* no sense doing this with GC disabled */
      /* Align ranges on page (4096-byte) boundaries.  */
      dump_file_offset = (dump_file_offset + 4095) & 0xFFFFF000;
#else
      /* Align ranges on 16-byte boundaries.  */
      dump_file_offset = (dump_file_offset + 15) & 0xFFFFFFF0;
#endif
      pfe_dump_range_table[range_idx].file_offset = dump_file_offset;
      dump_file_offset += (pfe_dump_range_table[range_idx].end_addr 
                           - pfe_dump_range_table[range_idx].start_addr);
    }
#if PFE_MEMORY_STATS
  pfe_load_buffer_size = dump_file_offset;
#endif
}

/* Set the flag to indicate that command line macro processing is
   in progress.  */
void
pfe_set_cmd_ln_processing ()
{
  pfe_cmd_ln_processing = 1;
}

/* Reset the flag to indicate that command line macro processing is
   in progress.  */
void
pfe_reset_cmd_ln_processing ()
{
  pfe_cmd_ln_processing = 0;
}

/* Return 1 if command line macro processing is in progress.  */
int
pfe_is_cmd_ln_processing ()
{
  return (pfe_cmd_ln_processing == 1);
}

