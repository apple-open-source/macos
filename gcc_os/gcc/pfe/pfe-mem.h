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

#ifndef GCC_PFE_MEMMGR_H
#define GCC_PFE_MEMMGR_H

#include <stddef.h>

/* Initialize the PFE's low-level memory manager.  */
extern void pfem_init 			PARAMS ((void));

/* Shut down the PFE's low-level memory manager.  */
extern void pfem_term 			PARAMS ((void));

/* PFE low-level memory manager's malloc.  */
extern void *pfem_malloc 		PARAMS ((size_t));

/* PFE low-level memory manager's calloc: allocates and zeros memory 
   for n objects.  */
extern void *pfem_calloc 		PARAMS ((size_t, size_t));

/* PFE low-level memory manager's realloc: reallocates memory given
   a pointer to a previous memory allocation.  */
extern void *pfem_realloc 		PARAMS ((void *, size_t));

/* PFE low-level memory manager's free.  */
extern void pfem_free 			PARAMS ((void *));

/* Identify ranges of memory used by the low-level memory manager.
   The parameter is a pointer to a call-back function that is passed
   the end-points of the memory range.  */
extern void pfem_id_ranges		PARAMS ((void (*)(unsigned long, unsigned long)));

/* Indicates if the pointer is to memory controlled by the PFE 
   memory manager.  (The memory may not currently be allocated.)  */
extern int pfem_is_pfe_mem		PARAMS ((void *));

#endif /* GCC_PFE_MEMMGR_H */
