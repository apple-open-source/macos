/* APPLE LOCAL file Darwin */
/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#if defined (TARGET_POWERPC)
#include "ppc-macosx-thread-status.h"
#include "ppc-macosx-regs.h"
#elif defined (TARGET_I386)
#include "i386-macosx-thread-status.h"
#include "i386-macosx-tdep.h"
#elif defined (TARGET_ARM)
#include "arm-macosx-thread-status.h"
#include "arm-macosx-tdep.h"
#else
#error "unsupported architecture"
#endif

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "exec.h"
#include "readline/readline.h"
#include "mach-o.h"
/* APPLE LOCAL - subroutine inlining  */
#include "inlining.h"
#include "gdb_assert.h"
#include "macosx-nat-inferior.h"
#ifdef MACOSX_DYLD
#include "macosx-nat-dyld.h"
#endif
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

struct target_ops macho_core_ops;

static struct bfd_section *
lookup_section (bfd *abfd, unsigned int n)
{
  struct bfd_section *sect = NULL;

  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (n < bfd_count_sections (abfd));

  sect = abfd->sections;

  while (n-- > 0)
    sect = sect->next;

  return sect;
}

static void
check_thread (bfd *abfd, asection *asect, unsigned int num)
{
  const char *sname = bfd_section_name (abfd, asect);
  unsigned int i;
  const char *expected = NULL;
#if defined (TARGET_POWERPC)
  const char *names[] = 
    {
      "LC_THREAD.PPC_THREAD_STATE.",
      "LC_THREAD.PPC_THREAD_STATE_64."
    };
#elif defined (TARGET_I386)
  const char *names[] = 
    {
      "LC_THREAD.i386_THREAD_STATE.",
      "LC_THREAD.x86_THREAD_STATE.",
      "LC_THREAD.x86_THREAD_STATE64."
    };
#elif defined (TARGET_ARM)
  const char *names[] = 
    {
      "LC_THREAD.ARM_THREAD_STATE."
    };
#else
#error "unsupported architecture"
#endif
  const int num_names = sizeof (names)/sizeof (const char*);

  /* Check all possible thread state names for a possible match.  */
  for (i = 0; i < num_names; i++)
    {
      if (strncmp (sname, names[i], strlen (names[i])) == 0)
	{
	  expected = names[i];
	  break;
	}
    }

  /* Make sure we found a matching thread state name.  */
  if (expected == NULL)
    return; /* We didn't find a match.  */

  /* Extract the thread index.  */
  i = strtol (sname + strlen (expected), NULL, 0);

  add_thread (ptid_build (1, i, num));
  if (ptid_equal (inferior_ptid, null_ptid))
    {
      inferior_ptid = ptid_build (1, i, num);
    }
}

static void
core_close_1 (void *arg)
{
  char *name;

  if (core_bfd == NULL)
    {
      return;
    }

  name = bfd_get_filename (core_bfd);
  if (!bfd_close (core_bfd))
    {
      warning ("Unable to close \"%s\": %s", name,
               bfd_errmsg (bfd_get_error ()));
    }

  core_bfd = NULL;
  inferior_ptid = null_ptid;

#ifdef CLEAR_SOLIB
  CLEAR_SOLIB ();
#endif

  if (macho_core_ops.to_sections)
    {
      xfree (macho_core_ops.to_sections);
      macho_core_ops.to_sections = NULL;
      macho_core_ops.to_sections_end = NULL;
    }
}

static void
core_close (int quitting)
{
  core_close_1 (NULL);
}

static void
core_open (char *filename, int from_tty)
{
  const char *p;
  int siggy;
  struct cleanup *old_chain;
  char *temp;
  bfd *temp_bfd;
  int ontop;
  int scratch_chan;
  struct bfd_section *sect;
  unsigned int i;

  target_preopen (from_tty);
  if (!filename)
    {
      error (core_bfd ?
             "No core file specified.  (Use `detach' to stop debugging a core file.)"
             : "No core file specified.");
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
      temp = concat (current_directory, "/", filename, NULL);
      xfree (filename);
      filename = temp;
    }

  old_chain = make_cleanup (free, filename);

  scratch_chan = open (filename, write_files ? O_RDWR : O_RDONLY, 0);
  if (scratch_chan < 0)
    perror_with_name (filename);

  temp_bfd = bfd_fdopenr (filename, gnutarget, scratch_chan);
  if (temp_bfd == NULL)
    perror_with_name (filename);

  if (bfd_check_format (temp_bfd, bfd_core) == 0)
    {
      /* Do it after the err msg */
      /* FIXME: should be checking for errors from bfd_close (for one thing,
         on error it does not free all the storage associated with the
         bfd).  */
      make_cleanup_bfd_close (temp_bfd);
      error ("\"%s\" is not a core dump: %s",
             filename, bfd_errmsg (bfd_get_error ()));
    }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain); /* Don't free filename any more */
  unpush_target (&macho_core_ops);
  core_bfd = temp_bfd;
  old_chain = make_cleanup (core_close_1, core_bfd);

  validate_files ();

  /* Find the data section */
  if (build_section_table (core_bfd, &macho_core_ops.to_sections,
                           &macho_core_ops.to_sections_end))
    error ("\"%s\": Can't find sections: %s",
           bfd_get_filename (core_bfd), bfd_errmsg (bfd_get_error ()));

  ontop = !push_target (&macho_core_ops);
  discard_cleanups (old_chain);

  p = bfd_core_file_failing_command (core_bfd);
  if (p)
    printf_filtered ("Core was generated by `%s'.\n", p);

  siggy = bfd_core_file_failing_signal (core_bfd);
  if (siggy > 0)
    printf_filtered ("Program terminated with signal %d, %s.\n", siggy,
                     target_signal_to_string (target_signal_from_host
                                              (siggy)));

  /* Build up thread list from BFD sections. */

  init_thread_list ();

  inferior_ptid = null_ptid;
  i = 0;

  for (sect = core_bfd->sections; sect != NULL; i++, sect = sect->next)
    check_thread (core_bfd, sect, i);

  CHECK_FATAL (i == core_bfd->section_count);

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("Core file contained no thread-specific data\n");
    }

  if (ontop)
    {
#ifdef MACOSX_DYLD
      /* Load all mach images by checking the frozen state of DYLD so
         we know where all shared libraries are.  */
      macosx_init_dyld_from_core ();
#endif
      /* Fetch all registers from core file.  */
      target_fetch_registers (-1);

      /* Now, set up the frame cache, and print the top of stack.  */
      flush_cached_frames ();
      select_frame (get_current_frame ());
      print_stack_frame (get_selected_frame (NULL), 1, SRC_AND_LOC);
      /* APPLE LOCAL begin subroutine inlining  */
      clear_inlined_subroutine_print_frames ();
      /* APPLE LOCAL end subroutine inlining  */
    }
  else
    {
      warning
        ("you won't be able to access this core file until you terminate\n"
         "your %s; do ``info files''", target_longname);
    }
}

static void
core_detach (char *args, int from_tty)
{
  if (args)
    error ("Too many arguments");
  unpush_target (&macho_core_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No core file now.\n");
}

/* Each architecture that supports core files needs to define a 
   structure that contains pointers to each possible flavour of
   registers that a core file for said arch can contain. When a 
   core file is opened,  the register contents found in the mach-o
   load commands for each thread will be cached in the 
   "thrd_info->private->core_thread_state" member of the
   thread_info structure. Any register sets that don't have values
   stored in the mach-o load commands will be NULL. This allows 
   read/write access to core file registers for all threads and 
   modified thread register values will survive thread context 
   switches. Values stored in these buffers are not swapped 
   to match the host byte order. This is done for fetch/store 
   efficiency reasons. Structures and functions that use the 
   unswapped values have a "_raw" suffix appended to them to 
   clarify their usage.  */

#if defined (TARGET_POWERPC)

struct core_cached_registers_raw
{
      gdb_ppc_thread_state_t * ppc_gp_regs;
      gdb_ppc_thread_fpstate_t * ppc_fp_regs;
      gdb_ppc_thread_vpstate_t * ppc_vp_regs;
      gdb_ppc_thread_state_64_t * ppc64_gp_regs;
};

#elif defined (TARGET_I386)

struct core_cached_registers_raw
{
      gdb_i386_thread_state_t * i386_gp_regs;
      gdb_i386_float_state_t * i386_fp_regs;
      gdb_x86_thread_state64_t * x86_64_gp_regs;
      gdb_x86_float_state64_t * x86_64_fp_regs;
};

#elif defined (TARGET_ARM)

struct core_cached_registers_raw
{
      gdb_arm_thread_state_t *arm_gp_regs;
      gdb_arm_thread_fpstate_t *arm_fp_regs;
};

#else
#error "unsupported architecture"
#endif

typedef struct core_cached_registers_raw core_cached_registers_raw_t;


/* This function will fetch the register values for the current
   thread from the core thread register cache and place the results
   in the thread register cache (regcache.c).  */

static int
core_fetch_cached_thread_registers ()
{
  core_cached_registers_raw_t *cached_regs_raw;
  struct thread_info *thrd_info = find_thread_pid (inferior_ptid);

  if (thrd_info == NULL || thrd_info->private == NULL)
    return 0;

  cached_regs_raw = (core_cached_registers_raw_t *)
		    thrd_info->private->core_thread_state;

  if (cached_regs_raw == NULL)
    return 0;

#if defined (TARGET_POWERPC)

  if (cached_regs_raw->ppc_gp_regs)
    ppc_macosx_fetch_gp_registers_raw (cached_regs_raw->ppc_gp_regs);

  if (cached_regs_raw->ppc_fp_regs)
    ppc_macosx_fetch_fp_registers_raw (cached_regs_raw->ppc_fp_regs);

  if (cached_regs_raw->ppc_vp_regs)
    ppc_macosx_fetch_vp_registers_raw (cached_regs_raw->ppc_vp_regs);

  if (cached_regs_raw->ppc64_gp_regs)
    ppc_macosx_fetch_gp_registers_64_raw (cached_regs_raw->ppc64_gp_regs);

#elif defined (TARGET_I386)

  if (cached_regs_raw->i386_gp_regs)
    i386_macosx_fetch_gp_registers_raw (cached_regs_raw->i386_gp_regs);

  if (cached_regs_raw->i386_fp_regs)
    i386_macosx_fetch_fp_registers_raw (cached_regs_raw->i386_fp_regs);
    
  if (cached_regs_raw->x86_64_gp_regs)
    x86_64_macosx_fetch_gp_registers_raw (cached_regs_raw->x86_64_gp_regs);
    
  if (cached_regs_raw->x86_64_fp_regs)
    x86_64_macosx_fetch_fp_registers_raw (cached_regs_raw->x86_64_fp_regs);

#elif defined (TARGET_ARM)

  if (cached_regs_raw->arm_gp_regs)
    arm_macosx_fetch_gp_registers_raw (cached_regs_raw->arm_gp_regs);

  if (cached_regs_raw->arm_fp_regs)
    arm_macosx_fetch_vfp_registers_raw (cached_regs_raw->arm_fp_regs);

#else

#error "unsupported architecture"

#endif

  return 1;
}


static void
core_cache_section_registers (asection *sec, int flavour, 
			      core_cached_registers_raw_t* cached_regs_raw)
{
  unsigned size;
  unsigned char *regs;

  size = bfd_section_size (core_bfd, sec);
  regs = (unsigned char *) xmalloc (size);
  if (regs == NULL)
    {
      fprintf_filtered (gdb_stderr,
                        "Unable to allocate space to read registers\n");
    }
  if (bfd_get_section_contents (core_bfd, sec, regs, (file_ptr) 0, size) != 1)
    {
      fprintf_filtered (gdb_stderr,
                        "Unable to read register data from core file\n");
    }

#if defined (TARGET_POWERPC)
  switch (flavour)
    {
    case BFD_MACH_O_PPC_THREAD_STATE:
      cached_regs_raw->ppc_gp_regs = (gdb_ppc_thread_state_t *) regs;
      regs = NULL;
      break;
      
    case BFD_MACH_O_PPC_FLOAT_STATE:
      cached_regs_raw->ppc_fp_regs = (gdb_ppc_thread_fpstate_t *) regs;
      regs = NULL;
      break;
      
    case BFD_MACH_O_PPC_EXCEPTION_STATE:
      break;

    case BFD_MACH_O_PPC_VECTOR_STATE:
      cached_regs_raw->ppc_vp_regs = (gdb_ppc_thread_vpstate_t *) regs;
      regs = NULL;
      break;
    
    case BFD_MACH_O_PPC_THREAD_STATE_64:
      cached_regs_raw->ppc64_gp_regs = (gdb_ppc_thread_state_64_t *) regs;
      regs = NULL;
      break;
    }
#elif defined (TARGET_I386)
  switch (flavour)
    {
    case BFD_MACH_O_i386_THREAD_STATE:
      cached_regs_raw->i386_gp_regs = (gdb_i386_thread_state_t *) regs;
      regs = NULL;
      break;
      
    case BFD_MACH_O_i386_FLOAT_STATE:
      cached_regs_raw->i386_fp_regs = (gdb_i386_float_state_t *) regs;
      regs = NULL;
      break;

    case BFD_MACH_O_x86_THREAD_STATE64:
      cached_regs_raw->x86_64_gp_regs = (gdb_x86_thread_state64_t *) regs;
      regs = NULL;
      break;

    case BFD_MACH_O_x86_FLOAT_STATE64:
      cached_regs_raw->x86_64_fp_regs = (gdb_x86_float_state64_t *) regs;
      regs = NULL;
      break;

    case BFD_MACH_O_x86_THREAD_STATE:
      {
	/* We are going to copy out just what we need from this structure
	   since it contains a flavour, and a count followed by other bytes.
	   We need to be sure not to set regs to NULL or we will have a 
	   memory leak since we will copy out just what we need into a new
	   buffer.  */
	ULONGEST sub_flavour = extract_unsigned_integer ((const gdb_byte *)regs, 4);
	if (sub_flavour == BFD_MACH_O_i386_THREAD_STATE)
	  {
	    gdb_assert (cached_regs_raw->i386_gp_regs == NULL);
	    cached_regs_raw->i386_gp_regs = (gdb_i386_thread_state_t *)
	                           xmalloc (sizeof (gdb_i386_thread_state_t));
	    memcpy (cached_regs_raw->i386_gp_regs, regs + 8, 
		    sizeof (gdb_i386_thread_state_t));
	  }
	else if (sub_flavour == BFD_MACH_O_x86_THREAD_STATE64)
	  {
	    gdb_assert (cached_regs_raw->x86_64_gp_regs == NULL);
	    cached_regs_raw->x86_64_gp_regs = (gdb_x86_thread_state64_t *)
	                           xmalloc (sizeof (gdb_x86_thread_state64_t));
	    memcpy (cached_regs_raw->x86_64_gp_regs, regs + 8, 
		    sizeof (gdb_x86_thread_state64_t));
	  }
      }
      break;

    case BFD_MACH_O_x86_FLOAT_STATE:
      {
	/* We are going to copy out just what we need from this structure
	   since it contains a flavour, and a count followed by other bytes.
	   We need to be sure not to set regs to NULL or we will have a 
	   memory leak since we will copy out just what we need into a new
	   buffer.  */
	ULONGEST sub_flavour = extract_unsigned_integer ((const gdb_byte *)regs, 4);
	if (sub_flavour == BFD_MACH_O_i386_FLOAT_STATE)
	  {
	    gdb_assert (cached_regs_raw->i386_fp_regs == NULL);
	    cached_regs_raw->i386_fp_regs = (gdb_i386_float_state_t *)
	                           xmalloc (sizeof (gdb_i386_float_state_t));
	    memcpy (cached_regs_raw->i386_fp_regs, regs + 8, 
		    sizeof (gdb_i386_float_state_t));
	  }
	else if (sub_flavour == BFD_MACH_O_x86_FLOAT_STATE64)
	  {
	    gdb_assert (cached_regs_raw->x86_64_fp_regs == NULL);
	    cached_regs_raw->x86_64_fp_regs = (gdb_x86_float_state64_t *)
	                           xmalloc (sizeof (gdb_x86_float_state64_t));
	    memcpy (cached_regs_raw->x86_64_fp_regs, regs + 8, 
		    sizeof (gdb_x86_float_state64_t));
	  }
      }
      break;

    case BFD_MACH_O_i386_EXCEPTION_STATE:
    case BFD_MACH_O_x86_EXCEPTION_STATE64:
      break;
    }
#elif defined (TARGET_ARM)
  switch (flavour)
    {
    case BFD_MACH_O_ARM_THREAD_STATE:
      cached_regs_raw->arm_gp_regs = (gdb_arm_thread_state_t *) regs;
      regs = NULL;
      break;
      
    case BFD_MACH_O_ARM_VFP_STATE:
      cached_regs_raw->arm_fp_regs = (gdb_arm_thread_fpstate_t *) regs;
      regs = NULL;
      break;
    }
#else
#error "unsupported architecture"
#endif
  /* If the flavor was recognized and are now owned by CACHED_REGS_RAW, 
     then REGS should have been set to NULL so we don't free them here.  */
  if (regs != NULL)
    xfree (regs);
}



/* Utility function to allocate and intitialize our private
   thread info structure.  */
int
create_private_thread_info (struct thread_info *thrd_info)
{
  if (thrd_info)
    {
      if (thrd_info->private != NULL)
	return 1; /* Success: private member already exists.  */
      else
	{
	  /* Allocate our private thread info and initialize it.  */
	  thrd_info->private = (struct private_thread_info *)
			 xmalloc (sizeof (struct private_thread_info));
	  if (thrd_info->private)
	    {
	      memset (thrd_info->private, 0, sizeof (struct private_thread_info));
	      /* Success: The private thread info got correctly allocated
	         and initialized.  */
	      return 1;
	    }
	}
    }
  /* Failure: NULL thread info passed in, or out of memory.  */
  return 0;
}

/* We currently leak one private thread info structure per thread
   as there is no callback when a thread info structure is about
   to get destroyed. If such a callback ever does exist, then we
   can use this function to do the cleanup.  */
void
delete_private_thread_info (struct thread_info *thrd_info)
{
  if (thrd_info != NULL && thrd_info->private != NULL)
    {
      delete_core_thread_state_cache (thrd_info);
      xfree (thrd_info->private);
      thrd_info->private = NULL;
    }
}



/* Create a private thread info structure if one is not already
   created, and cache a threads' registers so we can have write
   access to them.  */
int
create_core_thread_state_cache (struct thread_info *thrd_info)
{
  if (thrd_info && create_private_thread_info (thrd_info))
    {
      if (thrd_info->private->core_thread_state != NULL)
	return 1; /* Success: already have it.  */
      else
	{
	  /* Allocate our core thread state struct and initialize it.  */
	  thrd_info->private->core_thread_state = 
	    (core_cached_registers_raw_t *)
	    xmalloc (sizeof (core_cached_registers_raw_t));
	  
	  if (thrd_info->private->core_thread_state)
	    {
	      memset (thrd_info->private->core_thread_state, 0, 
		      sizeof (core_cached_registers_raw_t));
	      return 1;
	    }
	}
    }
  /* Failure: NULL thread info passed in, or out of memory.  */
  return 0;
}

/* Free the processor specific registers structures in the private
   core thread state if any exists.  */
void
delete_core_thread_state_cache (struct thread_info *thrd_info)
{
  if (thrd_info && thrd_info->private && thrd_info->private->core_thread_state)
    {
      core_cached_registers_raw_t *cached_regs_raw = 
	(core_cached_registers_raw_t *)thrd_info->private->core_thread_state;
#if defined (TARGET_POWERPC)

      if (cached_regs_raw->ppc_gp_regs) 
	xfree (cached_regs_raw->ppc_gp_regs);
	
      if (cached_regs_raw->ppc_fp_regs) 
	xfree (cached_regs_raw->ppc_fp_regs);
	
      if (cached_regs_raw->ppc_vp_regs) 
	xfree (cached_regs_raw->ppc_vp_regs);
	
      if (cached_regs_raw->ppc64_gp_regs)
	xfree (cached_regs_raw->ppc64_gp_regs);
	
#elif defined (TARGET_I386)

      if (cached_regs_raw->i386_gp_regs) 
	xfree (cached_regs_raw->i386_gp_regs);
	
      if (cached_regs_raw->i386_fp_regs) 
	xfree (cached_regs_raw->i386_fp_regs);
	
      if (cached_regs_raw->x86_64_gp_regs) 
	xfree (cached_regs_raw->x86_64_gp_regs);
	
      if (cached_regs_raw->x86_64_fp_regs) 
	xfree (cached_regs_raw->x86_64_fp_regs);

#elif defined (TARGET_ARM)

      if (cached_regs_raw->arm_gp_regs) 
	xfree (cached_regs_raw->arm_gp_regs);
	
      if (cached_regs_raw->arm_fp_regs) 
	xfree (cached_regs_raw->arm_fp_regs);
	
#else
#error "unsupported architecture"
#endif
      xfree (thrd_info->private->core_thread_state);
      thrd_info->private->core_thread_state = NULL;
    }
}



static void
core_fetch_registers (int regno)
{
  bfd *abfd = core_bfd;

  /* Get the thread index for the current thread. A bfd section at this
     index will hold the thread's first flavour of registers -- the GPRs
     -- and subsequent sections will contain other registers for this
     thread.  */
  long tid = ptid_get_tid (inferior_ptid);
  
  asection *sec;
  const char *thread_index_str = NULL;
  char flavour_str[256];
  unsigned int flavour = 0;
  struct thread_info * thrd_info = find_thread_pid (inferior_ptid);
  
  /* Make sure we have a valid thread to fetch our registers for.  */
  if (thrd_info == NULL)
    return;
    
  /* Check to see if we have already cached the core registers for this
     thread. This allows us to modify the registers found in a core 
     file by reading them into a cache and always using that cache
     when reading and writing registers.  */
  if (thrd_info->private == NULL || thrd_info->private->core_thread_state == NULL)
    {
      for (tid = ptid_get_tid (inferior_ptid);
	   tid < bfd_count_sections (abfd)
	   && (sec = lookup_section (abfd, tid)) != NULL;
	   tid++)
	{
	  const char *sname = bfd_section_name (abfd, sec);
	  
	  /* See if the section names starts with "LC_THREAD.".  */
	  if (strstr (sname, "LC_THREAD.") == sname)
	    {
	      /* Extract the flavour into a temp string.  */
	      strncpy (flavour_str, sname + 10, sizeof(flavour_str)-1);
	      char *dot = strchr (flavour_str, '.');
	      if (dot)
		{
		  /* Set the thread index string it it hasn't arlready been set 
		     to match the thread index of the inferior_ptid. If the
		     thread index string has been set, make sure it matches that
		     of the current thread so we can get all registers for this
		     thread.  */
		  if (thread_index_str == NULL)
		    thread_index_str = strrchr(sname, '.');
		  else if (strcmp (thread_index_str, dot) != 0)
		    continue;
		    
		  /* NULL terminate the flavour string and lookup the flavour
		     by name.  */
		  *dot = '\0';
#if defined (TARGET_POWERPC)
		  flavour = bfd_mach_o_flavour_from_string (BFD_MACH_O_CPU_TYPE_POWERPC, 
							    flavour_str);
#elif defined (TARGET_I386)
		  flavour = bfd_mach_o_flavour_from_string (BFD_MACH_O_CPU_TYPE_I386, 
							    flavour_str);
#elif defined (TARGET_ARM)
		  flavour = bfd_mach_o_flavour_from_string (BFD_MACH_O_CPU_TYPE_ARM, 
							    flavour_str);
#else
#error "unsupported architecture"
#endif
		  /* If we recognized the flavour, get the registers for it.  */
		  if (flavour != 0)
		    {
		      if (create_core_thread_state_cache (thrd_info))
			core_cache_section_registers (sec, flavour, 
				  thrd_info->private->core_thread_state);
		    }
		}
	    }
	}
    }
  /* Fetch the registers from our cache.  */
  core_fetch_cached_thread_registers ();
}

static void
core_files_info (struct target_ops *t)
{
  print_section_info (t, core_bfd);
}

static char *
macosx_core_ptid_to_str (ptid_t pid)
{
  static char buf[128];
  sprintf (buf, "core thread %lu", ptid_get_lwp (pid));
  return buf;
}

static int
core_thread_alive (ptid_t pid)
{
  return 1;
}

static void
core_prepare_to_store (void)
{
}

static void
core_store_registers (int regno)
{
  core_cached_registers_raw_t *cached_regs_raw;
  struct thread_info *thrd_info = find_thread_pid (inferior_ptid);

  if (thrd_info == NULL || thrd_info->private == NULL)
    return;

  cached_regs_raw = (core_cached_registers_raw_t *)
		    thrd_info->private->core_thread_state;
  if (cached_regs_raw == NULL)
    return;

  /* We have all core registers in register caches per thread in the
     thread_info->private->core_thread_state. All we need to do it update
     this information and it will get read back out. The register bytes are
     stored in target endain format just like the register cache, so we must
     make sure to keep them in target endian format during this store.  */
#if defined (TARGET_POWERPC)

  if (cached_regs_raw->ppc_gp_regs)
    ppc_macosx_store_gp_registers_raw (cached_regs_raw->ppc_gp_regs);

  if (cached_regs_raw->ppc_fp_regs)
    ppc_macosx_store_fp_registers_raw (cached_regs_raw->ppc_fp_regs);

  if (cached_regs_raw->ppc_vp_regs)
    ppc_macosx_store_vp_registers_raw (cached_regs_raw->ppc_vp_regs);

  if (cached_regs_raw->ppc64_gp_regs)
    ppc_macosx_store_gp_registers_64_raw (cached_regs_raw->ppc64_gp_regs);

#elif defined (TARGET_I386)

  if (cached_regs_raw->i386_gp_regs)
    i386_macosx_store_gp_registers_raw (cached_regs_raw->i386_gp_regs);

  if (cached_regs_raw->i386_fp_regs)
    i386_macosx_store_fp_registers_raw (cached_regs_raw->i386_fp_regs);
    
  if (cached_regs_raw->x86_64_gp_regs)
    x86_64_macosx_store_gp_registers_raw (cached_regs_raw->x86_64_gp_regs);
    
  if (cached_regs_raw->x86_64_fp_regs)
    x86_64_macosx_store_fp_registers_raw (cached_regs_raw->x86_64_fp_regs);
    

#elif defined (TARGET_ARM)

  if (cached_regs_raw->arm_gp_regs)
    arm_macosx_store_gp_registers_raw (cached_regs_raw->arm_gp_regs);

  if (cached_regs_raw->arm_fp_regs)
    arm_macosx_store_vfp_registers_raw (cached_regs_raw->arm_fp_regs);
    
#else
#error "unsupported architecture"
#endif
     
}

static void
init_macho_core_ops ()
{
  macho_core_ops.to_shortname = "core-macho";
  macho_core_ops.to_longname = "Mach-O core dump file";
  macho_core_ops.to_doc =
    "Use a core file as a target.  Specify the filename of the core file.";
  macho_core_ops.to_open = core_open;
  macho_core_ops.to_close = core_close;
  macho_core_ops.to_attach = find_default_attach;
  macho_core_ops.to_detach = core_detach;
  macho_core_ops.to_fetch_registers = core_fetch_registers;
  macho_core_ops.to_prepare_to_store = core_prepare_to_store;
  macho_core_ops.to_store_registers = core_store_registers;
  macho_core_ops.deprecated_xfer_memory = xfer_memory;
  macho_core_ops.to_files_info = core_files_info;
  macho_core_ops.to_create_inferior = find_default_create_inferior;
  macho_core_ops.to_pid_to_str = macosx_core_ptid_to_str;
  macho_core_ops.to_stratum = core_stratum;
  macho_core_ops.to_has_all_memory = 0;
  macho_core_ops.to_has_memory = 1;
  macho_core_ops.to_has_stack = 1;
  macho_core_ops.to_has_registers = 1;
  macho_core_ops.to_has_execution = 0;
  macho_core_ops.to_thread_alive = core_thread_alive;
  macho_core_ops.to_magic = OPS_MAGIC;
};

void
_initialize_core_macho ()
{
  init_macho_core_ops ();
  add_target (&macho_core_ops);
}
