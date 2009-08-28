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

#include "defs.h"
#include "inferior.h"
#include "symtab.h"
#include "symfile.h"
#include "objfiles.h"
#include "target.h"
#include "terminal.h"
#include "gdbcmd.h"
#include "regcache.h"
#include "value.h"
#include "exceptions.h"
#include "cli-out.h"
#include "environ.h"
#include "gdbcore.h"
#include "dictionary.h"
#include "block.h"
#include "objc-lang.h"

/* For the gdbarch_tdep structure so we can get the wordsize. */

#if defined (TARGET_POWERPC)
#include "ppc-tdep.h"
#elif defined (TARGET_I386)
#include "amd64-tdep.h"
#include "i386-tdep.h"
#elif defined (TARGET_ARM)
#include "arm-tdep.h"
#else
#error "Unrecognized target architecture."
#endif

#include "macosx-nat-mutils.h"
#include "macosx-nat-inferior.h"
#include "macosx-nat-inferior-debug.h"

#include <mach-o/nlist.h>

#include <mach/mach_error.h>

#include <sys/ptrace.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>

#include <unistd.h>

#include <AvailabilityMacros.h>

#include <mach/mach_vm.h>
#include <malloc/malloc.h>

#define MAX_INSTRUCTION_CACHE_WARNINGS 0

/* MINUS_INT_MIN is the absolute value of the minimum value that can
   be stored in a int.  We can't just use -INT_MIN, as that would
   implicitly be a int, not an unsigned int, and would overflow on 2's
   complement machines. */

#define MINUS_INT_MIN (((unsigned int) (- (INT_MAX + INT_MIN))) + INT_MAX)

static FILE *mutils_stderr = NULL;
static int mutils_debugflag = 0;

extern macosx_inferior_status *macosx_status;
static const char* g_macosx_protection_strs [8] = 
{ "---", "--r", "-w-", "-wr", "x--", "x-r", "xw-", "xwr" };

void
mutils_debug (const char *fmt, ...)
{
  va_list ap;
  if (mutils_debugflag)
    {
      va_start (ap, fmt);
      fprintf (mutils_stderr, "[%d mutils]: ", getpid ());
      vfprintf (mutils_stderr, fmt, ap);
      va_end (ap);
      fflush (mutils_stderr);
    }
}

static vm_size_t
child_get_pagesize ()
{
  kern_return_t status;
  static vm_size_t g_cached_child_page_size = 0;

  if (g_cached_child_page_size == 0)
    {
      status = host_page_size (mach_host_self (), &g_cached_child_page_size);
      /* This is probably being over-careful, since if we
         can't call host_page_size on ourselves, we probably
         aren't going to get much further.  */
      if (status != KERN_SUCCESS)
        g_cached_child_page_size = 0;
      MACH_CHECK_ERROR (status);
    }

  return g_cached_child_page_size;
}

/* Copy LEN bytes to or from inferior's memory starting at MEMADDR
   to debugger memory starting at MYADDR.   Copy to inferior if
   WRITE is nonzero.

   Returns the length copied. */

static int
mach_xfer_memory_remainder (CORE_ADDR memaddr, gdb_byte *myaddr,
                            int len, int write,
                            struct mem_attrib *attrib,
                            struct target_ops *target)
{
  vm_size_t pagesize = child_get_pagesize ();

  vm_offset_t mempointer;       /* local copy of inferior's memory */
  mach_msg_type_number_t memcopied;     /* for vm_read to use */

  CORE_ADDR pageaddr = memaddr - (memaddr % pagesize);

  kern_return_t kret;

  CHECK_FATAL (((memaddr + len - 1) - ((memaddr + len - 1) % pagesize))
               == pageaddr);

  if (!write)
    {
      kret = mach_vm_read (macosx_status->task, pageaddr, pagesize,
			   &mempointer, &memcopied);
      
      if (kret != KERN_SUCCESS)
	{
	  mutils_debug
	    ("Unable to read page for region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
	     paddr_nz (pageaddr), (unsigned long) len,
	     MACH_ERROR_STRING (kret), kret);
	  return 0;
	}
      if (memcopied != pagesize)
	{
	  kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
	  if (kret != KERN_SUCCESS)
	    {
	      warning
		("Unable to deallocate memory used by failed read from inferior: %s (0x%lx)",
		 MACH_ERROR_STRING (kret), (unsigned long) kret);
	    }
	  mutils_debug
	    ("Unable to read region at 0x%s with length %lu from inferior: "
	     "vm_read returned %lu bytes instead of %lu\n",
	     paddr_nz (pageaddr), (unsigned long) pagesize,
	     (unsigned long) memcopied, (unsigned long) pagesize);
	  return 0;
	}
      
      memcpy (myaddr, ((unsigned char *) 0) + mempointer
              + (memaddr - pageaddr), len);
      kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
      if (kret != KERN_SUCCESS)
	{
	  warning
	    ("Unable to deallocate memory used to read from inferior: %s (0x%ulx)",
	     MACH_ERROR_STRING (kret), kret);
	  return 0;
	}
    }
  else
    {
      /* We used to read in a whole page, then modify the page
	 contents, then write that page back out.  I bet we did that
	 so we didn't break up page maps or something like that.
	 However, in Leopard there's a bug in the shared cache
	 implementation, such that if we write into it with whole
	 pages the maximum page protections don't get set properly and
	 we can no longer reset the execute bit.  In 64 bit Leopard
	 apps, the execute bit has to be set or we can't run code from
	 there.

	 If we figure out that not writing whole pages causes problems
	 of it's own, then we will have to revisit this.  */

#if defined (TARGET_POWERPC)
      vm_machine_attribute_val_t flush = MATTR_VAL_CACHE_FLUSH;
      /* This vm_machine_attribute only works on PPC, so no reason
	 to keep failing on x86... */

      kret = vm_machine_attribute (mach_task_self (), mempointer,
                                   pagesize, MATTR_CACHE, &flush);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to flush GDB's address space after memcpy prior to vm_write: %s (0x%lx)\n",
             MACH_ERROR_STRING (kret), kret);
        }
#endif
      kret =
        mach_vm_write (macosx_status->task, memaddr, (pointer_t) myaddr, len);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to write region at 0x%s with length %lu to inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }

  return len;
}

static int
mach_xfer_memory_block (CORE_ADDR memaddr, gdb_byte *myaddr,
                        int len, int write,
                        struct mem_attrib *attrib, struct target_ops *target)
{
  vm_size_t pagesize = child_get_pagesize ();

  vm_offset_t mempointer;       /* local copy of inferior's memory */
  mach_msg_type_number_t memcopied;     /* for vm_read to use */

  kern_return_t kret;

  CHECK_FATAL ((memaddr % pagesize) == 0);
  CHECK_FATAL ((len % pagesize) == 0);

  if (!write)
    {
      kret =
        mach_vm_read (macosx_status->task, memaddr, len, &mempointer,
                      &memcopied);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to read region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
      if (memcopied != len)
        {
          kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
          if (kret != KERN_SUCCESS)
            {
              warning
                ("Unable to deallocate memory used by failed read from inferior: %s (0x%ux)",
                 MACH_ERROR_STRING (kret), kret);
            }
          mutils_debug
            ("Unable to read region at 0x%s with length %lu from inferior: "
             "vm_read returned %lu bytes instead of %lu\n",
             paddr_nz (memaddr), (unsigned long) len,
             (unsigned long) memcopied, (unsigned long) len);
          return 0;
        }
      memcpy (myaddr, ((unsigned char *) 0) + mempointer, len);
      kret = vm_deallocate (mach_task_self (), mempointer, memcopied);
      if (kret != KERN_SUCCESS)
        {
          warning
            ("Unable to deallocate memory used by read from inferior: %s (0x%ulx)",
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }
  else
    {
      kret =
        mach_vm_write (macosx_status->task, memaddr, (pointer_t) myaddr, len);
      if (kret != KERN_SUCCESS)
        {
          mutils_debug
            ("Unable to write region at 0x%s with length %lu from inferior: %s (0x%lx)\n",
             paddr_nz (memaddr), (unsigned long) len,
             MACH_ERROR_STRING (kret), kret);
          return 0;
        }
    }

  return len;
}


#if defined (VM_REGION_SUBMAP_SHORT_INFO_COUNT_64)

/* We probably have a Leopard based build since 
   VM_REGION_SUBMAP_SHORT_INFO_COUNT_64 was defined, so we will assign
   the functions to call for MACOSX_GET_REGION_INFO and MACOSX_VM_PROTECT.  */

#define macosx_get_region_info macosx_vm_region_recurse_short
#define macosx_vm_protect macosx_vm_protect_range

#else /* #if defined (VM_REGION_SUBMAP_SHORT_INFO_COUNT_64)  */

/* We need to build Salt on Tiger, but we want to try to run it on Leopard.
   On Leopard, there is both the SHORT and regular versions of the mach_vm_region_recurse
   call.  BUT the latter is MUCH slower on Leopard than on Tiger, whereas the short form
   is pretty much the same speed.  Sadly, the defines for the short version
   are missing on Tiger.  So in that case, I supply the defines here, copied
   over.  Then we try the call, and if we get KERN_INVALID_ARGUMENT, we know
   that we're on Tiger, and switch over to the long form.  */

  struct vm_region_submap_short_info_64 {
    vm_prot_t		protection;     /* present access protection */
    vm_prot_t		max_protection; /* max avail through vm_prot */
    vm_inherit_t		inheritance;/* behavior of map/obj on fork */
    memory_object_offset_t	offset;		/* offset into object/map */
    unsigned int            user_tag;	/* user tag on map entry */
    unsigned int            ref_count;	 /* obj/map mappers, etc */
    unsigned short          shadow_depth; 	/* only for obj */
    unsigned char           external_pager;  /* only for obj */
    unsigned char           share_mode;	/* see enumeration */
    boolean_t		is_submap;	/* submap vs obj */
    vm_behavior_t		behavior;	/* access behavior hint */
    vm_offset_t		object_id;	/* obj/map name, not a handle */
    unsigned short		user_wired_count; 
  };
  
  typedef struct vm_region_submap_short_info_64	*vm_region_submap_short_info_64_t;
  typedef struct vm_region_submap_short_info_64	 vm_region_submap_short_info_data_64_t;

#define VM_REGION_SUBMAP_SHORT_INFO_COUNT_64	((mach_msg_type_number_t) \
						 (sizeof(vm_region_submap_short_info_data_64_t)/sizeof(int)))

/* We probably have a Tiger based build since 
   VM_REGION_SUBMAP_SHORT_INFO_COUNT_64 wasn't defined, so we will assign
   the functions to call for MACOSX_GET_REGION_INFO and MACOSX_VM_PROTECT.  */
#if defined (TARGET_ARM)

/* ARM TARGET */
#define macosx_get_region_info macosx_vm_region_recurse_long
#define macosx_vm_protect macosx_vm_protect_region

#else /* #if defined (TARGET_ARM)  */

/* All other Tiger based targets.  */

/* Use the get_region_info variant call that tries both the long and short
   and region subrange calls.  */
#define macosx_get_region_info macosx_get_region_info_both

/* Use the vm_protect call that protect by the current region address.  */
#define macosx_vm_protect macosx_vm_protect_region

#endif  /* #else defined (TARGET_ARM)  */


#endif  /* #else defined (TARGET_ARM)  */


/* The old Tiger code used to call mach_vm_region to get the region 
   info, but this would return a very large region of memory and we
   would be modifying permissions on this large chunk.  */
static kern_return_t
macosx_vm_region (task_t task, 
		  mach_vm_address_t addr,
		  mach_vm_address_t *r_start,
		  mach_vm_size_t *r_size,
		  vm_prot_t *prot,
		  vm_prot_t *max_prot)
{
  mach_msg_type_number_t r_info_size;
  kern_return_t kret;
  mach_port_t r_object_name;
  vm_region_basic_info_data_64_t r_basic_data;
 
  r_info_size = VM_REGION_BASIC_INFO_COUNT_64;
  kret = mach_vm_region (macosx_status->task, r_start, r_size,
			 VM_REGION_BASIC_INFO_64,
			 (vm_region_info_t) & r_basic_data, &r_info_size,
			  &r_object_name);
  if (kret == KERN_SUCCESS)
    {
      *prot = r_basic_data.protection;
      *max_prot = r_basic_data.max_protection;
      mutils_debug ("macosx_vm_region ( 0x%8s ): [ 0x%8s - 0x%8s ) prot = %c%c%s "
		    "max_prot = %c%c%s\n",
		    paddr (addr),
		    paddr (*r_start), 
		    paddr (*r_start + *r_size), 
		    *prot & VM_PROT_COPY ? 'c' : '-',
		    *prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*prot & 7],
		    *max_prot & VM_PROT_COPY ? 'c' : '-',
		    *max_prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*max_prot & 7]);
    }
  else
    {
      mutils_debug ("macosx_vm_region ( 0x%8s ): ERROR %s\n",
		    paddr (addr), MACH_ERROR_STRING (kret));
      *r_start = 0;
      *r_size = 0;
      *prot = VM_PROT_NONE;
      *max_prot = VM_PROT_NONE;
    }
  return kret;

}


static kern_return_t
macosx_vm_region_recurse_long (task_t task, 
			       mach_vm_address_t addr,
			       mach_vm_address_t *r_start,
			       mach_vm_size_t *r_size,
			       vm_prot_t *prot,
			       vm_prot_t *max_prot)
{
  vm_region_submap_info_data_64_t r_long_data;
  mach_msg_type_number_t r_info_size;
  natural_t r_depth;
  kern_return_t kret;

  r_info_size = VM_REGION_SUBMAP_INFO_COUNT_64;
  r_depth = 1000;
  *r_start = addr;
    
  kret = mach_vm_region_recurse (task, 
				 r_start, r_size,
				 & r_depth,
				 (vm_region_recurse_info_t) &r_long_data, 
				 &r_info_size);
  if (kret == KERN_SUCCESS)
    {
      *prot = r_long_data.protection;
      *max_prot = r_long_data.max_protection;
      mutils_debug ("macosx_vm_region_recurse_long ( 0x%8s ): [ 0x%8s - 0x%8s ) "
		    "depth = %d, prot = %c%c%s max_prot = %c%c%s\n",
		    paddr (addr),
		    paddr (*r_start), 
		    paddr (*r_start + *r_size), 
		    r_depth, 
		    *prot & VM_PROT_COPY ? 'c' : '-',
		    *prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*prot & 7],
		    *max_prot & VM_PROT_COPY ? 'c' : '-',
		    *max_prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*max_prot & 7]);
    }
  else
    {
      mutils_debug ("macosx_vm_region_recurse_long ( 0x%8s ): ERROR %s\n",
		    paddr (addr), MACH_ERROR_STRING (kret));
      *r_start = 0;
      *r_size = 0;
      *prot = VM_PROT_NONE;
      *max_prot = VM_PROT_NONE;
    }

  return kret;
}


static kern_return_t
macosx_vm_region_recurse_short (task_t task, 
				mach_vm_address_t addr,
				mach_vm_address_t *r_start,
				mach_vm_size_t *r_size,
				vm_prot_t *prot,
				vm_prot_t *max_prot)
{
  vm_region_submap_short_info_data_64_t r_short_data;
  mach_msg_type_number_t r_info_size;
  natural_t r_depth;
  kern_return_t kret;

  r_info_size = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
  r_depth = 1000;
  *r_start = addr;

  kret = mach_vm_region_recurse (task, 
				 r_start, r_size,
				 & r_depth,
				 (vm_region_recurse_info_t) &r_short_data, 
				 &r_info_size);
  if (kret == KERN_SUCCESS)
    {
      *prot = r_short_data.protection;
      *max_prot = r_short_data.max_protection;
      mutils_debug ("macosx_vm_region_recurse_short ( 0x%8s ): [ 0x%8s - 0x%8s ) "
		    "depth = %d, prot = %c%c%s max_prot = %c%c%s\n",
		    paddr (addr),
		    paddr (*r_start), 
		    paddr (*r_start + *r_size), 
		    r_depth, 
		    *prot & VM_PROT_COPY ? 'c' : '-',
		    *prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*prot & 7],
		    *max_prot & VM_PROT_COPY ? 'c' : '-',
		    *max_prot & VM_PROT_NO_CHANGE ? '!' : '-',
		    g_macosx_protection_strs[*max_prot & 7]);
    }
  else
    {
      mutils_debug ("macosx_vm_region_recurse_short ( 0x%8s ): ERROR %s\n",
		    paddr (addr), MACH_ERROR_STRING (kret));
      *r_start = 0;
      *r_size = 0;
      *prot = VM_PROT_NONE;
      *max_prot = VM_PROT_NONE;
    }
  return kret;
}




static kern_return_t
macosx_vm_protect_range (task_t task,
			 mach_vm_address_t region_start, 
			 mach_vm_size_t region_size,
			 mach_vm_address_t addr,
			 mach_vm_size_t size,
			 vm_prot_t prot,
			 boolean_t set_max)
{
  kern_return_t kret;
  mach_vm_address_t protect_addr;
  mach_vm_size_t protect_size;

  /* On Leopard we want to protect the smallest range possible.  */
  protect_addr = addr;
  protect_size = size;

  kret = mach_vm_protect (task, protect_addr, protect_size, set_max, prot);
  mutils_debug ("macosx_vm_protect_range ( 0x%8s ):  [ 0x%8s - 0x%8s ) %s = %c%c%s => %s\n",
		paddr (addr),
		paddr (protect_addr), 
		paddr (protect_addr + protect_size), 
		set_max ? "max_prot" : "prot",
		prot & VM_PROT_COPY ? 'c' : '-',
		prot & VM_PROT_NO_CHANGE ? '!' : '-',
		g_macosx_protection_strs[prot & 7],
		kret ? MACH_ERROR_STRING (kret) : "0");
  return kret;
}

static kern_return_t
macosx_vm_protect_region (task_t task,
			  mach_vm_address_t region_start, 
			  mach_vm_size_t region_size,
			  mach_vm_address_t addr,
			  mach_vm_size_t size,
			  vm_prot_t prot,
			  boolean_t set_max)
{
  kern_return_t kret;
  mach_vm_address_t protect_addr;
  mach_vm_size_t protect_size;
  
  /* On Tiger we want to set protections at the region level.  */
  protect_addr = region_start;
  protect_size = region_size;

  kret = mach_vm_protect (task, protect_addr, protect_size, set_max, prot);

  mutils_debug ("macosx_vm_protect_region ( 0x%8s ):  [ 0x%8s - 0x%8s ) %s = %c%c%s => %s\n",
		paddr (addr),
		paddr (protect_addr), 
		paddr (protect_addr + protect_size), 
		set_max ? "max_prot" : "prot",
		prot & VM_PROT_COPY ? 'c' : '-',
		prot & VM_PROT_NO_CHANGE ? '!' : '-',
		g_macosx_protection_strs[prot & 7],
		kret ? MACH_ERROR_STRING (kret) : "0");

  return kret;
}


static kern_return_t
macosx_get_region_info_both (task_t task, 
			     mach_vm_address_t addr,
			     mach_vm_address_t *r_start,
			     mach_vm_size_t *r_size,
			     vm_prot_t *prot,
			     vm_prot_t *max_prot)
{
  static int use_short_info = 1;

  kern_return_t kret;
  
  if (use_short_info)
    {
      kret = macosx_vm_region_recurse_short (task, addr, r_start, r_size, 
					     prot, max_prot);

      if (kret == KERN_INVALID_ARGUMENT)
	{
	  use_short_info = 0;
	  mutils_debug ("vm_region_submap_short_info not supported, switching"
			" to long info.\n");
	  kret = macosx_vm_region_recurse_long (task, addr, r_start, r_size, 
						prot, max_prot);
	}
    }
  else
    {
      kret = macosx_vm_region_recurse_long (task, addr, r_start, r_size, 
					    prot, max_prot);
    }

  return kret;
}

int
mach_xfer_memory (CORE_ADDR memaddr, gdb_byte *myaddr,
                  int len, int write,
                  struct mem_attrib *attrib, struct target_ops *target)
{
  mach_vm_address_t r_start = 0;
  mach_vm_address_t r_end = 0;
  mach_vm_size_t r_size = 0;

  vm_prot_t orig_protection = 0;
  vm_prot_t max_orig_protection = 0;

  CORE_ADDR cur_memaddr;
  gdb_byte *cur_myaddr;
  int cur_len;

  vm_size_t pagesize = child_get_pagesize ();
  kern_return_t kret;
  int ret;

  /* check for out-of-range address */
  r_start = memaddr;
  if (r_start != memaddr)
    {
      errno = EINVAL;
      return 0;
    }
  
  if (len == 0)
    {
      return 0;
    }
  
  CHECK_FATAL (myaddr != NULL);
  errno = 0;
  
  /* check for case where memory available only at address greater than address specified */
  {
    kret = macosx_get_region_info (macosx_status->task, memaddr, &r_start, &r_size,
				   &orig_protection, &max_orig_protection);
    if (kret != KERN_SUCCESS)
      {
        return 0;
      }
    
    if (r_start > memaddr)
      {
        if ((r_start - memaddr) <= MINUS_INT_MIN)
          {
            mutils_debug
              ("First available address near 0x%s is at 0x%s; returning\n",
               paddr_nz (memaddr), paddr_nz (r_start));
            return -(r_start - memaddr);
          }
        else
          {
            mutils_debug ("First available address near 0x%s is at 0x%s "
                          "(too far; returning 0)\n",
                          paddr_nz (memaddr), paddr_nz (r_start));
            return 0;
          }
      }
  }

  cur_memaddr = memaddr;
  cur_myaddr = myaddr;
  cur_len = len;

  while (cur_len > 0)
    {
      int changed_protections = 0;

      /* We want the inner-most map containing our address, so set
	 the recurse depth to some high value, and call mach_vm_region_recurse.  */
      kret = macosx_get_region_info (macosx_status->task, cur_memaddr,
				     &r_start, &r_size, &orig_protection,
				     &max_orig_protection);
      
      if (r_start > cur_memaddr)
        {
          mutils_debug
            ("Next available region for address at 0x%s is 0x%s\n",
             paddr_nz (cur_memaddr), paddr_nz (r_start));
          break;
        }

      if (write)
        {
	  /* Keep the execute permission if we modify protections.  */
	  vm_prot_t new_prot = VM_PROT_READ | VM_PROT_WRITE;
	  	  
	  /* Do we need to modify our protections?  */
	  if (orig_protection & VM_PROT_WRITE)
	    {
	      /* We don't need to modify our protections.  */
	      kret = KERN_SUCCESS;
	      mutils_debug ("We already have write access to the region "
			    "containing: 0x%s, skipping permission modification.\n",
			    paddr_nz (cur_memaddr));
	    }
	  else
	    {
	      changed_protections = 1;
	      mach_vm_size_t prot_size;

	      if (cur_len < r_size - (cur_memaddr - r_start))
		prot_size = cur_len;
	      else
		prot_size = cur_memaddr - r_start;

	      kret = macosx_vm_protect (macosx_status->task, r_start, r_size, 
					cur_memaddr, prot_size, new_prot, 0);

	      if (kret != KERN_SUCCESS)
		{
		  mutils_debug ("Without COPY failed: %s (0x%lx)\n",
				MACH_ERROR_STRING (kret), kret);
		  kret = macosx_vm_protect (macosx_status->task, r_start, r_size, 
					    cur_memaddr, prot_size, 
					    VM_PROT_COPY | new_prot, 0);
		}

	      if (kret != KERN_SUCCESS)
		{
		  mutils_debug
		    ("Unable to add write access to region at 0x%s: %s (0x%lx)\n",
		     paddr_nz (r_start), MACH_ERROR_STRING (kret), kret);
		  break;
		}
	    }
        }

      r_end = r_start + r_size;

      CHECK_FATAL (r_start <= cur_memaddr);
      CHECK_FATAL (r_end >= cur_memaddr);
      CHECK_FATAL ((r_start % pagesize) == 0);
      CHECK_FATAL ((r_end % pagesize) == 0);
      CHECK_FATAL (r_end >= (r_start + pagesize));

      if ((cur_memaddr % pagesize) != 0)
        {
          int max_len = pagesize - (cur_memaddr % pagesize);
          int op_len = cur_len;
          if (op_len > max_len)
            {
              op_len = max_len;
            }
          ret = mach_xfer_memory_remainder (cur_memaddr, cur_myaddr, op_len,
                                            write, attrib, target);
        }
      else if (cur_len >= pagesize)
        {
          int max_len = r_end - cur_memaddr;
          int op_len = cur_len;
          if (op_len > max_len)
            {
              op_len = max_len;
            }
          op_len -= (op_len % pagesize);
          ret = mach_xfer_memory_block (cur_memaddr, cur_myaddr, op_len,
                                        write, attrib, target);
        }
      else
        {
          ret = mach_xfer_memory_remainder (cur_memaddr, cur_myaddr, cur_len,
                                            write, attrib, target);
        }

      if (write)
        {
	  /* This vm_machine_attribute isn't supported on i386,
	     so let's not try.  */
#if defined (TARGET_POWERPC)
	  vm_machine_attribute_val_t flush = MATTR_VAL_CACHE_FLUSH;
          kret = vm_machine_attribute (macosx_status->task, r_start, r_size,
                                       MATTR_CACHE, &flush);
          if (kret != KERN_SUCCESS)
            {
              static int nwarn = 0;
              nwarn++;
              if (nwarn <= MAX_INSTRUCTION_CACHE_WARNINGS)
                {
                  warning
                    ("Unable to flush data/instruction cache for region at 0x%s: %s",
                     paddr_nz (r_start), MACH_ERROR_STRING (ret));
                }
              if (nwarn == MAX_INSTRUCTION_CACHE_WARNINGS)
                {
                  warning
                    ("Support for flushing the data/instruction cache on this "
		     "machine appears broken");
                  warning ("No further warning messages will be given.");
                }
            }
#endif
	  /* Try and restore permissions on the minimal address range.  */
	  if (changed_protections)
	    {
	      mach_vm_size_t prot_size;
	      if (cur_len < r_size - (cur_memaddr - r_start))
		prot_size = cur_len;
	      else
		prot_size = cur_memaddr - r_start;

	      kret = macosx_vm_protect (macosx_status->task, r_start, r_size, 
					cur_memaddr, prot_size, 
					orig_protection, 0);
	      if (kret != KERN_SUCCESS)
		{
		  warning
		    ("Unable to restore original permissions for region at 0x%s",
		     paddr_nz (r_start));
		}
	    }
        }


      cur_memaddr += ret;
      cur_myaddr += ret;
      cur_len -= ret;

      if (ret == 0)
        {
          break;
        }
    }

  return len - cur_len;
}


LONGEST
mach_xfer_partial (struct target_ops *ops,
		   enum target_object object, const char *annex,
		   gdb_byte *readbuf, const gdb_byte *writebuf,
		   ULONGEST offset, LONGEST len)
{
  switch (object)
    {
    case TARGET_OBJECT_MEMORY:
      {
	ssize_t nbytes = len;

	if (readbuf)
	  nbytes = mach_xfer_memory (offset, readbuf, nbytes, 0, NULL, ops);
	if (writebuf && nbytes > 0)
	  nbytes = mach_xfer_memory (offset, writebuf, nbytes, 1, NULL, ops);
	return nbytes;
      }

    default:
      return -1;
    }
}

int
macosx_port_valid (mach_port_t port)
{
  mach_port_type_t ptype;
  kern_return_t ret;

  ret = mach_port_type (mach_task_self (), port, &ptype);
  return (ret == KERN_SUCCESS);
}

int
macosx_task_valid (task_t task)
{
  kern_return_t ret;
  struct task_basic_info info;
  unsigned int info_count = TASK_BASIC_INFO_COUNT;

  ret = task_info (task, TASK_BASIC_INFO, (task_info_t) & info, &info_count);
  return (ret == KERN_SUCCESS);
}

int
macosx_thread_valid (task_t task, thread_t thread)
{
  thread_array_t thread_list;
  unsigned int thread_count;
  kern_return_t kret;

  unsigned int found = 0;
  unsigned int i;

  CHECK_FATAL (task != TASK_NULL);

  kret = task_threads (task, &thread_list, &thread_count);
  if ((kret == KERN_INVALID_ARGUMENT)
      || (kret == MACH_SEND_INVALID_RIGHT) || (kret == MACH_RCV_INVALID_NAME))
    {
      return 0;
    }
  MACH_CHECK_ERROR (kret);

  for (i = 0; i < thread_count; i++)
    {
      if (thread_list[i] == thread)
        {
          found = 1;
        }
    }

  kret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                        (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (kret);

  if (!found)
    {
      mutils_debug ("thread 0x%lx no longer valid for task 0x%lx\n",
                    (unsigned long) thread, (unsigned long) task);
    }
  return found;
}

int
macosx_pid_valid (int pid)
{
  int ret;
  ret = kill (pid, 0);
  mutils_debug ("kill (%d, 0) : ret = %d, errno = %d (%s)\n", pid,
                ret, errno, strerror (errno));
  return ((ret == 0) || ((errno != ESRCH) && (errno != ECHILD)));
}

thread_t
macosx_primary_thread_of_task (task_t task)
{
  thread_array_t thread_list;
  unsigned int thread_count;
  thread_t tret = THREAD_NULL;
  kern_return_t ret;

  CHECK_FATAL (task != TASK_NULL);

  ret = task_threads (task, &thread_list, &thread_count);
  MACH_CHECK_ERROR (ret);

  tret = thread_list[0];

  ret = vm_deallocate (mach_task_self (), (vm_address_t) thread_list,
                       (vm_size_t) (thread_count * sizeof (thread_t)));
  MACH_CHECK_ERROR (ret);

  return tret;
}

kern_return_t
macosx_msg_receive (mach_msg_header_t * msgin, size_t msg_size,
                    unsigned long timeout, mach_port_t port)
{
  kern_return_t kret;
  mach_msg_option_t options;

  mutils_debug ("macosx_msg_receive: waiting for message\n");

  options = MACH_RCV_MSG;
  if (timeout > 0)
    {
      options |= MACH_RCV_TIMEOUT;
    }
  kret = mach_msg (msgin, options, 0, msg_size, port,
                   timeout, MACH_PORT_NULL);

  if (mutils_debugflag)
    {
      if (kret == KERN_SUCCESS)
        {
          macosx_debug_message (msgin);
        }
      else
        {
          mutils_debug ("macosx_msg_receive: returning %s (0x%lx)\n",
                        MACH_ERROR_STRING (kret), kret);
        }
    }

  return kret;
}

/* Allocate LEN bytes in the target's address space.  We could be much
   more efficient about how we use space (for example, by making a
   mmalloc pool out of it, or at a minimum, an obstack.  But since we
   only call this in the rare cases when malloc() isn't available, it
   shouldn't be too big a deal. */

CORE_ADDR
allocate_space_in_inferior_mach (int len)
{
  kern_return_t kret;
  vm_address_t address;
 
  kret = vm_allocate (macosx_status->task, &address, len, TRUE);
  if (kret != KERN_SUCCESS)
    error ("No memory available to program: call to vm_allocate failed");

  return address;
}

/* Used by macosx_allocate_space_in_inferior. */

struct macosx_alloc_data 
{
  CORE_ADDR addr;
  int len;
};

/* Used by macosx_allocate_space_in_inferior. */

static int
macosx_allocate_space_in_inferior_helper (struct ui_out *ui_out, void *args)
{
  struct macosx_alloc_data *alloc = (struct macosx_alloc_data *) args;
  alloc->addr = allocate_space_in_inferior_malloc (alloc->len);
  return 0;
}

/* Allocate LEN bytes in the target's address space.  Use the generic
   malloc-based code.  If that fails, use the Mach-based allocator. */

CORE_ADDR
macosx_allocate_space_in_inferior (int len)
{
  int ret;
  struct macosx_alloc_data alloc;
  struct cleanup *cleanups;

  cleanups = make_cleanup_ui_out_suppress_output (uiout);
  /* Suppress the debugger mode here since we trust the system malloc
     will never end up in the ObjC runtime.  */
  make_cleanup_set_restore_debugger_mode (NULL, -1);

  alloc.len = len;
  alloc.addr = 0;

  ret = catch_exceptions (uiout, macosx_allocate_space_in_inferior_helper,
                          &alloc, RETURN_MASK_ALL);

  do_cleanups (cleanups);

  if (ret >= 0)
    return alloc.addr;

  alloc.addr = allocate_space_in_inferior_mach (len);
  return alloc.addr;
}

/* This section provides a gdb interface to the Mac OS X malloc
   history.  We use two functions stack_logging_enumerate_records
   and stack_logging_frames_for_uniqued_stack.
   The definitions come from stack_logging.h, which is part of libc,
   but which isn't installed on the developer system, so I have to
   copy the def'ns here.  */

#define MAX_NUM_FRAMES 100

#define stack_logging_type_free         0
#define stack_logging_type_generic      1       /* anything that is not allocation/deallocation */
#define stack_logging_type_alloc        2       /* malloc, realloc, etc... */
#define stack_logging_type_dealloc      4       /* free, realloc, etc... */

#if HAVE_64_BIT_STACK_LOGGING

typedef struct {
	uint32_t		type_flags;
	uint64_t		stack_identifier;
	uint64_t		argument;
	mach_vm_address_t	address;
} mach_stack_logging_record_t;

#define STACK_LOGGING_ENUMERATION_PROVIDED      1       // temporary to avoid dependencies between projects

extern kern_return_t __mach_stack_logging_enumerate_records(task_t task, 
							    mach_vm_address_t address, 
							    void enumerator(mach_stack_logging_record_t, void *), 
							    void *context);
/* Gets all the records about address;
   If !address, gets all records */

extern kern_return_t __mach_stack_logging_frames_for_uniqued_stack(task_t task, 
								   uint64_t stack_identifier,
								   mach_vm_address_t *stack_frames_buffer, 
								   uint32_t max_stack_frames, 
								   uint32_t *num_frames);


/* END STACK_LOGGING.H  */

/* I added these ones: */
#define STACK_LOGGING_ALLOC_P(record) ((record).type_flags & stack_logging_type_alloc)
#define STACK_LOGGING_DEALLOC_P(record) ((record).type_flags & stack_logging_type_dealloc)

#elif HAVE_32_BIT_STACK_LOGGING

typedef struct {
  unsigned    type;
  unsigned    uniqued_stack;
  unsigned    argument;
  unsigned    address; /* disguised, to avoid confusing leaks */
} stack_logging_record_t;

extern kern_return_t stack_logging_enumerate_records(task_t task, 
						     memory_reader_t reader, 
						     vm_address_t address, 
						     void enumerator(stack_logging_record_t, void *), 
						     void *context);

extern kern_return_t stack_logging_frames_for_uniqued_stack(task_t task, 
							    memory_reader_t reader, 
							    unsigned uniqued_stack, 
							    vm_address_t *stack_frames_buffer, 
							    unsigned max_stack_frames, 
							    unsigned *num_frames);

/* I added these ones: */
#define STACK_LOGGING_ALLOC_P(record) ((record).type & stack_logging_type_alloc)
#define STACK_LOGGING_DEALLOC_P(record) ((record).type & stack_logging_type_dealloc)

/* gdb_malloc_reader: The libc malloc history reader requires a
   routine of this signature to read out inferior memory, and return a
   pointer to a local copy.  Note, the malloc history routines don't
   take over ownership of this memory, and may use it after the next
   call to the malloc reader.  So we build a chain of the memory we have
   copied over, and free it when the whole malloc-history command is done.  */

struct malloc_history_chain
{
  unsigned char *buffer;
  struct malloc_history_chain *next;
} *malloc_history_head;

static int
gdb_malloc_reader (task_t task, vm_address_t addr, vm_size_t size, void **local_copy)
{
  struct malloc_history_chain *tmp;

  if (task != macosx_status->task)
    {
      warning ("malloc reader called with wrong task.");
      return 1;
    }
  
  tmp = (struct malloc_history_chain *) xmalloc (sizeof (struct malloc_history_chain));
  tmp->buffer = (unsigned char *) xmalloc (size);
  if (target_read_memory (addr, tmp->buffer, size) != 0) 
    {
      warning ("malloc history request for %d bytes of memory at %s failed.",
	       size, paddr_nz (addr));
      xfree (tmp->buffer);
      xfree (tmp);
      return 1;
    }
  tmp->next = malloc_history_head;
  malloc_history_head = tmp;

  *local_copy = tmp->buffer;
  return 0;
}

static void 
free_malloc_history_buffers ()
{
  struct malloc_history_chain *tmp;
  while (malloc_history_head != NULL)
    {
      tmp = malloc_history_head;
      malloc_history_head = malloc_history_head->next;
      xfree (tmp->buffer);
      xfree (tmp);
    }
}
#endif

struct current_record_state
{
  vm_address_t requested_address;
  vm_address_t block_address;
};

/* This is the iterator function that libc uses in
   stack_logging_enumerate_records.  It calls this function for each
   uniqued stack that allocated a given address.  We just
   print out the symbolicated version of this stack.  
   If DATA is NULL, then just print out everything that comes in.  
   Otherwise, we use the state to keep track both of the requested
   address, and when we find a malloc that contains the given address
   we record the start block so we can match the free to this malloc
   event.  */

#if HAVE_64_BIT_STACK_LOGGING
static void 
do_over_unique_frames (mach_stack_logging_record_t record, void *data) 
{
  mach_vm_address_t frames[MAX_NUM_FRAMES];
#elif HAVE_32_BIT_STACK_LOGGING
static void 
do_over_unique_frames (stack_logging_record_t record, void *data) 
{
  vm_address_t frames[MAX_NUM_FRAMES];
#endif
  unsigned num_frames;
  struct cleanup *cleanup;
  struct symtab_and_line sal;
  int i;
  CORE_ADDR thread;
  int final_return = 0;
  struct current_record_state *state = (struct current_record_state *) data;

  if (state != NULL)
    {
      if (STACK_LOGGING_ALLOC_P (record))
	{
	  /* For alloc type events the "argument" field is the size of the allocation. */
	      if (state->requested_address >= record.address 
		  && state->requested_address < record.address + record.argument)
		{
		  /* We need to record the actual address of this allocation so we can 
		     match it up with the deallocation event.  */
		  state->block_address = record.address;
		  
		}
	      else
		return;
	}
      else if (STACK_LOGGING_DEALLOC_P (record))
	{
	  if (record.address != state->block_address)
	    return;
	}
      else
	return;
    }

#if HAVE_64_BIT_STACK_LOGGING
  if (__mach_stack_logging_frames_for_uniqued_stack (macosx_status->task, 
						     record.stack_identifier,
						     frames, MAX_NUM_FRAMES, &num_frames))
#elif HAVE_32_BIT_STACK_LOGGING
  if (stack_logging_frames_for_uniqued_stack (macosx_status->task, 
					      gdb_malloc_reader, 
					      record.uniqued_stack,
					      frames, MAX_NUM_FRAMES, &num_frames))
#endif
    {
      warning ("Error running stack_logging_frames_for_uniqued_stack");
      return;
    }

  if (num_frames == 0)
    return;

  /* The last element of the frame array always points to the result of pthread_self()
     (plus 1 for no apparent reason).  The second to the last element seems to
     always be "1" or sometimes "2".  We always make the first page unreadable,
     so I'll just say if the frame address is < 1024 it can't be right and elide it...  */
  thread = (CORE_ADDR) (frames[--num_frames] - 1);
  if (frames[num_frames - 1] <= 1024)
    num_frames--;

  cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, NULL);

  if (STACK_LOGGING_ALLOC_P (record))
    {
      ui_out_field_string (uiout, "record-type", "Alloc");
      ui_out_text (uiout, ": Block address: ");
      ui_out_field_core_addr (uiout, "block_address", record.address);
      ui_out_text (uiout, " length: ");
      ui_out_field_int (uiout, "length", record.argument);
      ui_out_text (uiout, "\n");
    }
  else
    {
      ui_out_field_string (uiout, "record-type", "Dealloc");
      ui_out_text (uiout, ": Block address: ");
      ui_out_field_core_addr (uiout, "block_address", record.address);
      ui_out_text (uiout, "\n");
      final_return = 1;
    }

  ui_out_text (uiout, "Stack - pthread: ");
  ui_out_field_fmt (uiout, "pthread", "0x%s", paddr_nz (thread));
  ui_out_text (uiout, " number of frames: ");
  ui_out_field_int (uiout, "num_frames", num_frames);
  ui_out_text (uiout, "\n");

  for (i = 0; i < num_frames; i++)
    {
      struct cleanup *frame_cleanup
	= make_cleanup_ui_out_tuple_begin_end (uiout, "frame");
      char *name;
      int err;
      struct gdb_exception e;
      /* This is cheesy spacing, but we really won't get
	 more than 1000 frames, so more work would be overkill.  */
      if (i < 10)
	ui_out_text (uiout, "    ");
      else if (i < 100)
	ui_out_text (uiout, "   ");
      else 
	ui_out_text (uiout, "  ");

      ui_out_field_int (uiout, "level", i);
      ui_out_text (uiout, ": ");

      ui_out_field_fmt (uiout, "addr", "0x%s", paddr_nz (frames[i]));

      /* Since we're going to do pc->symbol, we should raise the load level
	 of the library involved before doing so.  */

      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  pc_set_load_state (frames[i], OBJF_SYM_ALL, 1);
	}
      if (e.reason != NO_ERROR)
	{
	  ui_out_text (uiout, "\n");
	  warning ("Could not raise load level for objfile at pc: 0x%s.", paddr_nz (frames[i]));
	  continue;
	}

      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  err = find_pc_partial_function_no_inlined (frames[i], &name, NULL, NULL);
	}
      if (e.reason == NO_ERROR && err != 0)
	{
	  ui_out_text(uiout, " in ");
	  ui_out_field_string (uiout, "func", name);
	}

      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  sal = find_pc_line (frames[i], 0);
	}
      if (e.reason == NO_ERROR && sal.symtab != 0)
	{
	  ui_out_text (uiout, " at ");
	  ui_out_field_string (uiout, "file", sal.symtab->filename);
	  ui_out_text (uiout, ":");
	  ui_out_field_int (uiout, "line", sal.line);
	}
      ui_out_text (uiout, "\n");
      do_cleanups (frame_cleanup);
    }
  do_cleanups (cleanup);
  if (final_return)
    ui_out_text (uiout, "\n");
}

/* This adds the "info malloc-history" command.  Requires one argument
   (an address) and returns the malloc history for that address, as
   gathered by Libc if the MallocStackLoggingNoCompact environment
   variable is set.  */

void
malloc_history_info_command (char *arg, int from_tty)
{
#if HAVE_64_BIT_STACK_LOGGING || HAVE_32_BIT_STACK_LOGGING

#if HAVE_64_BIT_STACK_LOGGING
  mach_vm_address_t addr;
#elif HAVE_32_BIT_STACK_LOGGING
  vm_address_t addr;
#endif
  kern_return_t kret;
  volatile struct gdb_exception except;
  struct cleanup *cleanup;
  /* APPLE LOCAL - Make "-exact" the default, since there's no way to
     interrupt large spews that may result from using "-range".  */
  int exact = 1;
  vm_address_t passed_addr;
  struct current_record_state state, *passed_state;

  if (macosx_status == NULL)
    error ("No target");

  if (arg == NULL)
    error ("Argument required (expression to compute).");

  if (strstr (arg, "-exact") == arg)
    {
      exact = 1;
      arg += sizeof ("-exact") - 1;
      while (*arg == ' ')
	arg++;
    }
  else if (strstr (arg, "-range") == arg)
    {
      exact = 0;
      arg += sizeof ("-range") - 1;
      while (*arg == ' ')
	arg++;
    }

#if HAVE_64_BIT_STACK_LOGGING
  addr = (mach_vm_address_t) parse_and_eval_address (arg);
#elif HAVE_32_BIT_STACK_LOGGING
  addr = parse_and_eval_address (arg);
#endif

  if (!target_has_execution)
    error ("Can't get malloc history: target is not running");

  if (inferior_environ == NULL 
      || get_in_environ (inferior_environ, "MallocStackLoggingNoCompact") == NULL)
    {
      warning ("MallocStackLoggingNoCompact not set in target's environment"
	       " so the malloc history will not be available.");
    }
  cleanup = make_cleanup_ui_out_list_begin_end (uiout, "stacks");

  if (exact)
    {
      passed_addr = addr;
      passed_state = NULL;
    }
  else
    {
      passed_addr = 0;
      state.requested_address = addr;
      state.block_address = 0;
      passed_state = &state;
    }

  TRY_CATCH (except, RETURN_MASK_ERROR)
    {
#if HAVE_64_BIT_STACK_LOGGING
      kret = __mach_stack_logging_enumerate_records (macosx_status->task,
						     passed_addr,
						     do_over_unique_frames,
						     passed_state);
#elif HAVE_32_BIT_STACK_LOGGING
      kret = stack_logging_enumerate_records (macosx_status->task,
					      gdb_malloc_reader,
					      passed_addr,
					      do_over_unique_frames,
					      passed_state);
#endif
    }

#if HAVE_32_BIT_STACK_LOGGING
  /* Remember to reset the memory copy areas.  */
  free_malloc_history_buffers ();
#endif
  do_cleanups (cleanup);

  if (except.reason < 0)
    {
      error ("Caught an error while enumerating stack logging records: %s", except.message);
    }

  if (kret != KERN_SUCCESS)
    {
      error ("Unable to enumerate stack logging records: %s (ox%lx).",
             MACH_ERROR_STRING (kret), (unsigned long) kret);
    }
#else  /* HAVE_64_BIT_STACK_LOGGING || HAVE_32_BIT_STACK_LOGGING  */
  error ("Stack logging not supported for this target.");
#endif
}

/* Given an type TYPE, and an offset OFFSET into the type, this
   appends the path to the element at that offset to the string
   SYMBOL_NAME.  Returns 0 if it found the complete path, or the
   remaining offset from as far as it managed to get if it didn't.  */

int
build_path_to_element (struct type *type, CORE_ADDR offset, char **symbol_name)
{
  if (TYPE_CODE (type) == TYPE_CODE_STRUCT)
    {
      int i;

      for (i = 0; i < TYPE_NFIELDS (type); i++)
	{
	  if (TYPE_FIELD_STATIC_KIND (type, i) != 0)
	    continue;
	  if (offset >= TYPE_FIELD_BITPOS (type, i)/8
	      && offset < TYPE_FIELD_BITPOS (type, i)/8 
	      + TYPE_LENGTH (TYPE_FIELD_TYPE (type, i)))
	    {
	      int orig_len;
	      if (*symbol_name == NULL)
		{
		  orig_len = 0;
		  *symbol_name = xmalloc (strlen (TYPE_FIELD_NAME (type, i)) + 1);
		  strcpy (*symbol_name, TYPE_FIELD_NAME (type, i));
		}
	      else
		{
		  orig_len = strlen (*symbol_name);
		  
		  /* grow the string to accomodate the dot and the field name.  */
		  *symbol_name = xrealloc (*symbol_name, orig_len + 1
					   + strlen (TYPE_FIELD_NAME (type, i)) + 1);
		  (*symbol_name)[orig_len] = '.';
	      
		  strcpy (*symbol_name + orig_len + 1, TYPE_FIELD_NAME (type, i));
		}
	      if (TYPE_FIELD_BITPOS (type, i)/8 == offset)
		return 0;
	      else
		return build_path_to_element (TYPE_FIELD_TYPE (type, i), 
					      offset - TYPE_FIELD_BITPOS (type, i)/8, 
					      symbol_name);
	    }
	}
      return offset;
    }
  else if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      /* FIXME - Didn't do arrays yet.  */
      return offset;
    }
  else
    return offset;
}

char *
get_symbol_at_address_on_stack (CORE_ADDR stack_address, int *frame_level)
{

  char *symbol_name = NULL;
  struct frame_info *this_frame;
  int found_frame;
  CORE_ADDR this_sp;
  
  this_frame = get_current_frame ();
  found_frame = 0;
  *frame_level = -1;

  while (this_frame != NULL && !found_frame)
    {
      struct gdb_exception e;
      TRY_CATCH (e, RETURN_MASK_ERROR)
	{
	  this_sp = get_frame_base (this_frame);
	  if (stack_address <= this_sp)
	    {
	      /* You can't break here, because of the TRY_CATCH.  */
	      *frame_level = frame_relative_level (this_frame);
	      found_frame = 1;
	    }
	  else
	    this_frame = get_prev_frame (this_frame);
	}
      if (e.reason == RETURN_ERROR)
	{
	  this_frame = NULL;
	  break;
	}
    }

  if (found_frame == 1)
    {
      struct symbol *this_symbol = NULL;
      struct block *block = get_frame_block (this_frame, 0);
      if (block != NULL)
	{
	  while (block != 0)
	    {
	      /* Look through the block and see if there are
		 any symbols it would put at this address... */
	      struct dict_iterator iter;
	      struct symbol *sym;
	      
	      ALL_BLOCK_SYMBOLS (block, iter, sym)
		{
		  struct value *val = NULL;
		  struct gdb_exception e;
		  struct type *val_type;
		  CORE_ADDR val_address;
		  /* This is a little inefficient, but without repeating
		     a lot of code in findvar.c, there's no way to get this
		     information...  */
		  TRY_CATCH (e, RETURN_MASK_ERROR)
		    {
		      val = value_of_variable (sym, block);
		    }
		  
		  if (val == NULL)
		    continue;
		  
		  /* Here's where we handle finding the symbol.  Note that if
		     the address lines up with a struct or array, we still
		     pass it to build_path_to_element to get the first member
		     right.  */
		  val_type = value_type (val);
		  val_address = VALUE_ADDRESS (val);
		  if (val_address == stack_address
		      && TYPE_CODE (val_type) != TYPE_CODE_STRUCT
		      && TYPE_CODE (val_type) != TYPE_CODE_ARRAY)
		    {
		      symbol_name = xstrdup (SYMBOL_PRINT_NAME (sym));
		      goto found_symbol;
		    }
		  else if (stack_address >= val_address 
			   && ( stack_address 
				< val_address 
				+ TYPE_LENGTH (val_type)))
		    {
		      CORE_ADDR offset = stack_address - val_address;
		      this_symbol = sym;
		      symbol_name = xstrdup (SYMBOL_PRINT_NAME (sym));
		      build_path_to_element (val_type, offset, &symbol_name);
		      goto found_symbol;
		    }
		}
	      
	      if (BLOCK_FUNCTION (block))
		{
		  this_symbol = NULL;
		  break;
		}
	      block = BLOCK_SUPERBLOCK (block);
	    }
	}
    }
 found_symbol:
  return symbol_name;
}
/* This stuff all comes from auto_gdb_interface.h */
#define AUTO_BLOCK_GLOBAL       0
#define AUTO_BLOCK_STACK        1
#define AUTO_BLOCK_OBJECT       2
#define AUTO_BLOCK_BYTES        3
#define AUTO_BLOCK_ASSOCIATION  4

static char *auto_kind_strings[5] = {"global", "stack", "object", "bytes", "assoc"};
static char *auto_kind_spacer[5] = {"", " ", "", " ", " "};
static CORE_ADDR
gc_print_references (CORE_ADDR list_addr, int wordsize)
{
  int ref_index;
  LONGEST num_refs;

  if (safe_read_memory_integer (list_addr, 4, &num_refs) == 0)
    error ("Could not read number of references at %s",
	   paddr_nz (list_addr));
  /* The struct we're reading looks like:
       struct auto_memory_reference_list {
         uint32_t count;
         struct auto_memory_reference references[0];
       }
       
       gcc wants to make sure that the array is wordsize aligned within
       the struct.  So we need to step by wordsize here, even though we're
       reading a 4-byte integer out of the struct.  */

  list_addr += wordsize;
  //ui_out_field_int (uiout, "depth", num_refs);
  //ui_out_text (uiout, "\n");

  for (ref_index = 0; ref_index < num_refs; ref_index++)
    {
      struct cleanup *ref_cleanup;
      LONGEST offset;
      ULONGEST address;
      ULONGEST kind;
      ULONGEST retain_cnt;
      
      ref_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "reference");
      
      if (safe_read_memory_unsigned_integer (list_addr, wordsize, &address) == 0)
	error ("Could not read address and reference %d at %s.",
	       ref_index, paddr_nz (list_addr));
      list_addr += wordsize;
      if (safe_read_memory_integer (list_addr, wordsize, &offset) == 0)
	error ("Could not read offset and reference %d at %s.",
	       ref_index, paddr_nz (list_addr));
      list_addr += wordsize;
      if (safe_read_memory_unsigned_integer (list_addr, 4, &kind) == 0)
	error ("Could not read kind and reference %d at %s.",
	       ref_index, paddr_nz (list_addr));
      list_addr += 4;
      if (safe_read_memory_unsigned_integer (list_addr, 4, &retain_cnt) == 0)
	error ("Could not read retainCount and reference %d at %s.",
	       ref_index, paddr_nz (list_addr));
      list_addr += 4;
      
      if (ref_index < 10)
	ui_out_text (uiout, "   ");
      else 
	ui_out_text (uiout, "  ");

      ui_out_field_int (uiout, "depth", ref_index);
	  
      ui_out_text (uiout, " Kind: ");

      if (kind >= AUTO_BLOCK_GLOBAL && kind <= AUTO_BLOCK_ASSOCIATION)
	{
	  ui_out_field_string (uiout, "kind", auto_kind_strings[kind]);
	  ui_out_text (uiout, auto_kind_spacer[kind]);
	}
      else
	ui_out_field_int (uiout, "kind", kind);
      
      ui_out_text (uiout, "  rc: ");
      /* Cheesy spacing, if we ever get retain counts over 9999 we won't
	 space right.  */
      if (retain_cnt < 10)
	ui_out_text (uiout, "  ");
      else if (retain_cnt < 100)
	ui_out_text (uiout, " ");

      ui_out_field_int (uiout, "retain-count", retain_cnt);

      if (kind == AUTO_BLOCK_STACK) 
	{
	  CORE_ADDR stack_address;
	  int frame_level;
	  char *symbol_name;

	  stack_address = address + offset;

	  ui_out_text (uiout, "  Address: ");
	  ui_out_field_core_addr (uiout, "address", stack_address);

	  symbol_name = get_symbol_at_address_on_stack (stack_address, &frame_level);

	  if (frame_level >= 0)
	    {
	      ui_out_text (uiout, "  Frame level: ");
	      ui_out_field_int (uiout, "frame", frame_level);
	      ui_out_text (uiout, "  Symbol: ");
	      if (symbol_name == NULL)
		ui_out_field_string (uiout, "symbol", "<unknown>");
	      else
		{
		  ui_out_field_string (uiout, "symbol", symbol_name);
		  xfree (symbol_name);
		}
	    }
	  else
	    {
	      ui_out_text (uiout, "  Frame:");
	      ui_out_field_string (uiout, "frame", "<unknown>");
	    }
	}
      else if (kind == AUTO_BLOCK_OBJECT 
	       || kind == AUTO_BLOCK_ASSOCIATION)
	{
	  /* This is an ObjC object. */
	  struct gdb_exception e;

	  struct type *dynamic_type = NULL;
	  char *dynamic_name = NULL;

          struct ui_file *saved_gdb_stderr;
          static struct ui_file *null_stderr = NULL;

	  ui_out_text (uiout, "  Address: ");
	  ui_out_field_core_addr (uiout, "address", address);

          /* suppress error messages */
          if (null_stderr == NULL)
            null_stderr = ui_file_new ();

          saved_gdb_stderr = gdb_stderr;
          gdb_stderr = null_stderr;

	  TRY_CATCH (e, RETURN_MASK_ERROR)
	    {
	      dynamic_type = objc_target_type_from_object (address, NULL, 
                                                           wordsize,
							   &dynamic_name);
	    }
	  if (e.reason == RETURN_ERROR)
	    dynamic_type = NULL;

          gdb_stderr = saved_gdb_stderr;

	  if (dynamic_type != NULL)
	    {
	      char *ivar_name = NULL;
	      ui_out_text (uiout, "  Class: ");
	      ui_out_field_string (uiout, "class", TYPE_NAME (dynamic_type));
	      if (kind == AUTO_BLOCK_ASSOCIATION) 
		{
		  ui_out_text (uiout, "  Key: ");
		  ui_out_field_core_addr (uiout, "key", offset);
		  if (dynamic_name != NULL)
		    {
		      ui_out_text (uiout, "  Class: ");
		      ui_out_field_string (uiout, "class", dynamic_name);
		    }
		}
	      else if (offset > 0)
		{
		  int remaining_offset;
		  remaining_offset = build_path_to_element (dynamic_type, offset, 
							    &ivar_name);
		  if (ivar_name != NULL)
		    {
		      ui_out_text (uiout, "  ivar: ");
		      ui_out_field_string (uiout, "ivar", ivar_name);
		      xfree (ivar_name);
		    }
		  else
		    {
		      ui_out_text (uiout, "  Offset: ");
		      ui_out_field_core_addr (uiout, "offset", offset);
		    }		      
		}
	    }
	  else
	    {
	      if (kind == AUTO_BLOCK_ASSOCIATION) 
		{
		  ui_out_text (uiout, "  Key: ");
		  ui_out_field_core_addr (uiout, "key", offset);
		} 
	      else 
		{
		  ui_out_text (uiout, "  Offset: ");
		  ui_out_field_core_addr (uiout, "offset", offset);
		}
	      if (dynamic_name != NULL)
		{
		  ui_out_text (uiout, "  Class: ");
		  ui_out_field_string (uiout, "class", dynamic_name);
		}
	    }
	  if (dynamic_name != NULL)
	    xfree (dynamic_name);
	}
      else
	{
	  ui_out_text (uiout, "  Address: ");
	  ui_out_field_core_addr (uiout, "address", address);
	  
	  if (offset != 0)
	    {
	      ui_out_text (uiout, "  Offset: ");
	      ui_out_field_core_addr (uiout, "offset", offset);
	    }
	  if (kind == AUTO_BLOCK_GLOBAL)
	    {
	      struct obj_section *the_sect;
	      struct minimal_symbol *msymbol = NULL;

	      the_sect = find_pc_sect_section (address, NULL);
	      if (the_sect != NULL)
		{
		  msymbol 
		    = lookup_minimal_symbol_by_pc_section (address,
							   the_sect->the_bfd_section);
		}
	      ui_out_text (uiout, " Symbol: ");
	      if (msymbol) 
		ui_out_field_string (uiout, "symbol", SYMBOL_PRINT_NAME (msymbol));
	      else
		ui_out_field_string (uiout, "symbol", "<unknown>");
	    }
	}
      
      ui_out_text (uiout, "\n");
      
      do_cleanups (ref_cleanup);
    }
  return list_addr;
}

/* We need this to return the auto_zone for the root & reference
   tracing functions.  */
static struct cached_value *auto_zone_fn = NULL;

static void
gc_free_data (struct value *addr_val)
{
  static struct cached_value *free_fn = NULL;
  struct cleanup *old_cleanups;

  /* Finally, we need to free the root list.  */

  if (free_fn == NULL)
    free_fn = create_cached_function ("Auto::aux_free", 
				      builtin_type_void_func_ptr);
  if (free_fn == NULL)
    error ("Couldn't find \"Auto::aux_free\" function in the inferior.\n");
  make_cleanup_set_restore_debugger_mode (&old_cleanups, -1);
  make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);
  call_function_by_hand (lookup_cached_function (free_fn), 1, &addr_val);  
  do_cleanups (old_cleanups);
}

static void
gc_root_tracing_command (char *arg, int from_tty)
{
  static struct cached_value *enumerate_root_fn = NULL;
  struct value *arg_list[3], *root_list_val;
  struct cleanup *cleanup_chain;
  CORE_ADDR addr, list_addr;
  LONGEST num_roots;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;
  int root_index;

  if (arg == NULL || *arg == '\0')
    error ("Address expression required.");

  if (!target_has_execution)
    error ("The program is not running.");

  addr = parse_and_eval_address (arg);
  
  /* First we have to make sure that the symbols
     for libauto.dylib are raised to all.  */

  if (objfile_name_set_load_state ("libauto.dylib", OBJF_SYM_ALL, 1)
      == -1)
    warning ("Couldn't raise the load level of libauto.dylib.");

  
  /* Now we have to cons up a gdb type for the root tracing
     data structures that we will need.  */
  /* FIXME - Maybe it's easier to just grub in memory...  */

  /* Make a cached version of the root tracing functions,
     auto_zone and auto_gdb_enumerate_roots.  Get the auto_zone,
     and then call the root function.  */
 
  if (auto_zone_fn == NULL)
    auto_zone_fn = create_cached_function ("auto_zone", 
					   builtin_type_voidptrfuncptr);
  if (auto_zone_fn == NULL)
    error ("Couldn't find \"auto_zone\" function in inferior.");

  if (enumerate_root_fn == NULL)
    enumerate_root_fn = create_cached_function ("auto_gdb_enumerate_roots", 
					   builtin_type_voidptrfuncptr);
  if (enumerate_root_fn == NULL)
    error ("Couldn't find \"auto_gdb_enumerate_roots\" function in inferior.");
    
  cleanup_chain = make_cleanup_set_restore_unwind_on_signal (1);
  make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);
  make_cleanup_set_restore_debugger_mode (NULL, -1);
  arg_list[0] 
    = call_function_by_hand (lookup_cached_function (auto_zone_fn), 
			     0, NULL);

  /* Okay, we've got a value for the auto_zone, now call the enumerate
     function.  */

  arg_list[1] = value_from_pointer (builtin_type_void_data_ptr, addr);
  arg_list[2] = value_from_pointer (builtin_type_void_data_ptr, read_sp ());
  /* We've got a pointer to the root list, traverse it and print
     out it's contents.  */
  
  root_list_val 
    = call_function_by_hand (lookup_cached_function (enumerate_root_fn), 
					 3, arg_list);
  do_cleanups (cleanup_chain);

  /* Now we grub in memory to print out the list.  */
  list_addr = value_as_address (root_list_val);
  if (list_addr == 0)
    error ("auto_gdb_enumerate_roots returned 0, it is not safe"
	   " to examine collector state right now.");

  if (safe_read_memory_integer (list_addr, 4, &num_roots) == 0)
    error ("Could not read the root list at address: %s.",
	   paddr_nz (list_addr));

  cleanup_chain = make_cleanup_ui_out_tuple_begin_end (uiout, "roots");
  ui_out_text (uiout, "Number of roots: ");
  ui_out_field_int (uiout, "num_roots", num_roots);

  ui_out_text (uiout, "\n");


  /* Now print out all the roots, and recursively their references.  */

  /* The struct we're reading looks like:
       struct auto_root_list {
         uint32_t count;
         struct auto_memory_reference_list_t references[0];
       }
       
       gcc wants to make sure that the array is wordsize aligned within
       the struct.  So we need to step by wordsize here, even though we're
       reading a 4-byte integer out of the struct.  */
  list_addr += wordsize;
  
  for (root_index = 0; root_index < num_roots; root_index++)
    {
      struct cleanup *root_cleanup;

      ui_out_text (uiout, "Root:\n");

      root_cleanup = make_cleanup_ui_out_tuple_begin_end (uiout, "root");

      list_addr = gc_print_references (list_addr, wordsize);

      do_cleanups (root_cleanup);
      
    }

  do_cleanups (cleanup_chain);
  /* Finally, we need to free the root list.  */
  if (num_roots > 0)
    gc_free_data (root_list_val);
}

void
gc_reference_tracing_command (char *arg, int from_tty)
{
  static struct cached_value *enumerate_ref_fn = NULL;
  struct value *arg_list[3], *ref_list_val;
  struct cleanup *cleanup_chain;
  CORE_ADDR addr, list_addr;
  LONGEST num_refs;
  int wordsize = gdbarch_tdep (current_gdbarch)->wordsize;

  if (arg == NULL || *arg == '\0')
    error ("Address expression required.");

  if (!target_has_execution)
    error ("The program is not running.");

  addr = parse_and_eval_address (arg);
  
  /* First we have to make sure that the symbols
     for libauto.dylib are raised to all.  */

  if (objfile_name_set_load_state ("libauto.dylib", OBJF_SYM_ALL, 1)
      == -1)
    warning ("Couldn't raise the load level of libauto.dylib.");

  
  /* Now we have to cons up a gdb type for the root tracing
     data structures that we will need.  */
  /* FIXME - Maybe it's easier to just grub in memory...  */

  /* Make a cached version of the root tracing functions,
     auto_zone and auto_gdb_enumerate_roots.  Get the auto_zone,
     and then call the root function.  */
 
  if (auto_zone_fn == NULL)
    auto_zone_fn = create_cached_function ("auto_zone", 
					   builtin_type_voidptrfuncptr);
  if (auto_zone_fn == NULL)
    error ("Couldn't find \"auto_zone\" function in inferior.");

  if (enumerate_ref_fn == NULL)
    enumerate_ref_fn = create_cached_function ("auto_gdb_enumerate_references", 
					   builtin_type_voidptrfuncptr);
  if (enumerate_ref_fn == NULL)
    error ("Couldn't find \"auto_gdb_enumerate_references\" function in inferior.");
    
  cleanup_chain = make_cleanup_set_restore_unwind_on_signal (1);
  make_cleanup_set_restore_scheduler_locking_mode 
                      (scheduler_locking_on);
  make_cleanup_set_restore_debugger_mode (NULL, -1);
  arg_list[0] 
    = call_function_by_hand (lookup_cached_function (auto_zone_fn), 
			     0, NULL);

  /* Okay, we've got a value for the auto_zone, now call the enumerate
     function.  */

  arg_list[1] = value_from_pointer (builtin_type_void_data_ptr, addr);
  arg_list[2] = value_from_pointer (builtin_type_void_data_ptr, read_sp ());
  /* We've got a pointer to the root list, traverse it and print
     out it's contents.  */
  
  ref_list_val 
    = call_function_by_hand (lookup_cached_function (enumerate_ref_fn), 
					 3, arg_list);
  do_cleanups (cleanup_chain);

  /* Now we grub in memory to print out the list.  */
  list_addr = value_as_address (ref_list_val);
  if (list_addr == 0)
    error ("auto_gdb_enumerate_references returned 0, it is not safe to"
	   " examine collector state right now.");
  if (safe_read_memory_integer (list_addr, 4, &num_refs) == 0)
    error ("Could not read the reference list at address: %s.",
	   paddr_nz (list_addr));

  list_addr = gc_print_references (list_addr, wordsize);
      
  if (num_refs > 0)
    gc_free_data (ref_list_val);
}

void
_initialize_macosx_mutils ()
{
  mutils_stderr = fdopen (fileno (stderr), "w");

  add_setshow_boolean_cmd ("mutils", class_obscure,
			   &mutils_debugflag, _("\
Set if printing inferior memory debugging statements."), _("\
Show if printing inferior memory debugging statements."), NULL,
			   NULL, NULL,
			   &setdebuglist, &showdebuglist);

  add_info ("malloc-history", malloc_history_info_command, 
	    "List the stack(s) where malloc or free occurred for the address\n"
	    "resulting from expression given in the argument to the command.\n"
            "If the argument is preceeded by \"-exact\" then only malloc and free events\n"
	    "for that address will be reported, if by \"-range\" then any malloc\n"
	    "that contains that address within its range will be reported.\n"
	    "The default is \"-range\".\n"
	    "Note: you must set MallocStackLoggingNoCompact in the target\n"
	    "environment for the malloc history to be logged."); 

  add_info ("gc-roots", gc_root_tracing_command, 
	    "List the garbage collector's shortest unique roots to a given address.");

  add_info ("gc-references", gc_reference_tracing_command, 
	    "List the garbage collectors references for a given address.");

}

