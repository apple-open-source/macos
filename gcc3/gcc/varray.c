/* Virtual array support.
   Copyright (C) 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Cygnus Solutions.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to the Free
   the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.  */

#include "config.h"
#include "errors.h"
#include "system.h"
#include "rtl.h"
#include "tree.h"
#include "bitmap.h"
#include "varray.h"

/* APPLE LOCAL PFE */
#ifdef PFE
#include "pfe/pfe.h"
/* Certain varray's need to be part of pfe memory because they are
   frozen/thawed.  These are determined on a case-by-case basis as
   ones needed to ve freeze/thawed.  They are indicated by their
   varray name being prefixed with the string defined by PFE_VARRAY.  */
static char *use_pfe_mem_indicator = PFE_VARRAY"";
#define USE_PFE_MEMORY(name) (*(name) == *use_pfe_mem_indicator)
#endif

#define VARRAY_HDR_SIZE (sizeof (struct varray_head_tag) - sizeof (varray_data))

/* Allocate a virtual array with NUM_ELEMENT elements, each of which is
   ELEMENT_SIZE bytes long, named NAME.  Array elements are zeroed.  */
varray_type
varray_init (num_elements, element_size, name)
     size_t num_elements;
     size_t element_size;
     const char *name;
{
  size_t data_size = num_elements * element_size;
/* APPLE LOCAL PFE */
#ifndef PFE
  varray_type ptr = (varray_type) xcalloc (VARRAY_HDR_SIZE + data_size, 1);
#else
  varray_type ptr = USE_PFE_MEMORY (name)
  		      ? (varray_type) PFE_CALLOC (VARRAY_HDR_SIZE + data_size, 1,
  		      				  PFE_ALLOC_VARRAY)
  		      : (varray_type) xcalloc (VARRAY_HDR_SIZE + data_size, 1);
  name = PFE_SAVESTRING (name);
#endif /* PFE */

  ptr->num_elements = num_elements;
  ptr->elements_used = 0;
  ptr->element_size = element_size;
  ptr->name = name;
  return ptr;
}

/* Grow/shrink the virtual array VA to N elements.  Zero any new elements
   allocated.  */
varray_type
varray_grow (va, n)
     varray_type va;
     size_t n;
{
  size_t old_elements = va->num_elements;

  if (n != old_elements)
    {
      size_t element_size = va->element_size;
      size_t old_data_size = old_elements * element_size;
      size_t data_size = n * element_size;

/* APPLE LOCAL PFE */
#ifdef PFE
      if (va->name && USE_PFE_MEMORY (va->name))
        va = (varray_type) PFE_REALLOC ((char *)va, 
        				VARRAY_HDR_SIZE + data_size,
        				PFE_ALLOC_VARRAY);
      else
#endif /* PFE */
      va = (varray_type) xrealloc ((char *) va, VARRAY_HDR_SIZE + data_size);
      va->num_elements = n;
      if (n > old_elements)
	memset (&va->data.c[old_data_size], 0, data_size - old_data_size);
    }

  return va;
}

/* Check the bounds of a varray access.  */

#if defined ENABLE_CHECKING && (GCC_VERSION >= 2007)

extern void error PARAMS ((const char *, ...))	ATTRIBUTE_PRINTF_1;

void
varray_check_failed (va, n, file, line, function)
     varray_type va;
     size_t n;
     const char *file;
     int line;
     const char *function;
{
  internal_error ("virtual array %s[%lu]: element %lu out of bounds in %s, at %s:%d",
		  va->name, (unsigned long) va->num_elements, (unsigned long) n,
		  function, trim_filename (file), line);
}

#endif

/* APPLE LOCAL PFE */
/*-------------------------------------------------------------------*/
#ifdef PFE

/* This is used by the VARRAY_FREE when pfe is being used to selectively
   determine which memory the varray_type was allocated in to do the
   appropriate "free".  */
   
void
pfe_varray_free (vp)
     varray_type vp;
{
  if (USE_PFE_MEMORY (vp->name))
    pfe_free (vp);
  else
    free (vp);
}

/* Varray trees are freeze/thawed iff they truely were allocated in
   pfe memory.  Otherwise we shouldn't be calling this routine.  */
void
pfe_freeze_thaw_varray_tree (vpp)
     varray_type *vpp;
{
  varray_type va = (varray_type)PFE_FREEZE_THAW_PTR (vpp);
  size_t i, nelts;
  
  if (va)
    {
      if (!USE_PFE_MEMORY (va->name))
	internal_error ("Virtual array %s was assumed to be in pfe memory and isn't",
			RP (va->name));
      pfe_freeze_thaw_ptr_fp (&va->name);
      nelts = VARRAY_ACTIVE_SIZE (va);
      for (i = 0; i < nelts; ++i) 
	PFE_FREEZE_THAW_WALK (va->data.tree[i]);
    }
}

#endif /* PFE */
