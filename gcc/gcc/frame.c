/* Subroutines needed for unwinding stack frames for exception handling.  */
/* Compile this one with gcc.  */
/* Copyright (C) 1997, 1998 Free Software Foundation, Inc.
   Contributed by Jason Merrill <jason@cygnus.com>.

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

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */

/* It is incorrect to include config.h here, because this file is being
   compiled for the target, and hence definitions concerning only the host
   do not apply.  */

#include "tconfig.h"

/* We disable this when inhibit_libc, so that gcc can still be built without
   needing header files first.  */
/* ??? This is not a good solution, since prototypes may be required in
   some cases for correct code.  See also libgcc2.c.  */
#ifndef inhibit_libc
/* fixproto guarantees these system headers exist. */
#include <stdlib.h>
#include <unistd.h>
#endif

#include "defaults.h"

#ifdef DWARF2_UNWIND_INFO
#include "dwarf2.h"
#include <stddef.h>
#include "frame.h"
#include "gthr.h"

#ifdef __DEBUG_EH__
#include <stdio.h>
#endif
#ifdef TRACE_EXCEPTIONS
int __trace_exceptions = 0;
#else
#define TRACE_EXCEPTIONS(MASK,ARG0,ARG1)	/* nothing  */
#endif

/* If someone has defined GET_AND_LOCK_TARGET_OBJECT_LIST, they're in
   charge of looking after mutex and threading.  */

#ifndef GET_AND_LOCK_TARGET_OBJECT_LIST
#ifdef __GTHREAD_MUTEX_INIT
static __gthread_mutex_t object_mutex = __GTHREAD_MUTEX_INIT;
#else
static __gthread_mutex_t object_mutex;
#endif
#endif

/* Don't use `fancy_abort' here even if config.h says to use it.  */
#ifdef abort
#undef abort
#endif

/* Some types used by the DWARF 2 spec.  */

typedef          int  sword __attribute__ ((mode (SI)));
typedef unsigned int  uword __attribute__ ((mode (SI)));
typedef unsigned int  uaddr __attribute__ ((mode (pointer)));
typedef          int  saddr __attribute__ ((mode (pointer)));
typedef unsigned char ubyte;

/* Terminology:
   CIE - Common Information Element
   FDE - Frame Descriptor Element

   There is one per function, and it describes where the function code
   is located, and what the register lifetimes and stack layout are
   within the function.

   The data structures are defined in the DWARF specfication, although
   not in a very readable way (see LITERATURE).

   Every time an exception is thrown, the code needs to locate the FDE
   for the current function, and starts to look for exception regions
   from that FDE. This works in a two-level search:
   a) in a linear search, find the shared image (i.e. DLL) containing
      the PC
   b) using the FDE table for that shared object, locate the FDE using
      binary search (which requires the sorting).  */   

/* The first few fields of a CIE.  The CIE_id field is 0 for a CIE,
   to distinguish it from a valid FDE.  FDEs are aligned to an addressing
   unit boundary, but the fields within are unaligned.  */

struct dwarf_cie {
  uword length;
  sword CIE_id;
  ubyte version;
  char augmentation[0];
} __attribute__ ((packed, aligned (__alignof__ (void *))));

/* The first few fields of an FDE.  */

struct dwarf_fde {
  uword length;
  sword CIE_delta;
  void* pc_begin;
  uaddr pc_range;
} __attribute__ ((packed, aligned (__alignof__ (void *))));

typedef struct dwarf_fde fde;

/* Objects to be searched for frame unwind info.  */

#ifndef GET_AND_LOCK_TARGET_OBJECT_LIST
static struct object *objects;
#define GET_AND_LOCK_TARGET_OBJECT_LIST(OB)		\
	do {						\
	  OB = objects;					\
	  init_object_mutex_once ();			\
          __gthread_mutex_lock (&object_mutex);		\
	} while (0)
                                    
#define UNLOCK_TARGET_OBJECT_LIST()  __gthread_mutex_unlock (&object_mutex)
#endif

/* The information we care about from a CIE.  */

struct cie_info {
  char *augmentation;
  void *eh_ptr;
  int code_align;
  int data_align;
  unsigned ra_regno;
};

/* The current unwind state, plus a saved copy for DW_CFA_remember_state.  */

struct frame_state_internal
{
  struct frame_state s;
  struct frame_state_internal *saved_state;
};

/* By default, the initial PC of an FDE is an absolute address.
   However on platforms with PIC based generation, this field contains
   a relative offset. On those platforms, the macro is overriden
   with a macro which performs the appropriate translation to absolute
   address.  */
 
#ifndef GET_DWARF2_FDE_INITIAL_PC
#define GET_DWARF2_FDE_INITIAL_PC(fde_ptr)	(fde_ptr->pc_begin)
#endif



/* This is undefined below if we need it to be an actual function.  */
#define init_object_mutex_once()

#if __GTHREADS
#ifdef __GTHREAD_MUTEX_INIT_FUNCTION

/* Helper for init_object_mutex_once.  */

static void
init_object_mutex (void)
{
  __GTHREAD_MUTEX_INIT_FUNCTION (&object_mutex);
}

/* Call this to arrange to initialize the object mutex.  */

#undef init_object_mutex_once
static void
init_object_mutex_once (void)
{
  static __gthread_once_t once = __GTHREAD_ONCE_INIT;
  __gthread_once (&once, init_object_mutex);
}

#endif /* __GTHREAD_MUTEX_INIT_FUNCTION */
#endif /* __GTHREADS */
  
/* Decode the unsigned LEB128 constant at BUF into the variable pointed to
   by R, and return the new value of BUF.  */

static void *
decode_uleb128 (unsigned char *buf, unsigned *r)
{
  unsigned shift = 0;
  unsigned result = 0;

  while (1)
    {
      unsigned byte = *buf++;
      result |= (byte & 0x7f) << shift;
      if ((byte & 0x80) == 0)
	break;
      shift += 7;
    }
  *r = result;
  return buf;
}

/* Decode the signed LEB128 constant at BUF into the variable pointed to
   by R, and return the new value of BUF.  */

static void *
decode_sleb128 (unsigned char *buf, int *r)
{
  unsigned shift = 0;
  unsigned result = 0;
  unsigned byte;

  while (1)
    {
      byte = *buf++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	break;
    }
  if (shift < (sizeof (*r) * 8) && (byte & 0x40) != 0)
    result |= - (1 << shift);

  *r = result;
  return buf;
}

/* Read unaligned data from the instruction buffer.  */

union unaligned {
  void *p;
  unsigned b2 __attribute__ ((mode (HI)));
  unsigned b4 __attribute__ ((mode (SI)));
  unsigned b8 __attribute__ ((mode (DI)));
} __attribute__ ((packed));
static inline void *
read_pointer (void *p)
{ union unaligned *up = p; return up->p; }
static inline unsigned
read_1byte (void *p)
{ return *(unsigned char *)p; }
static inline unsigned
read_2byte (void *p)
{ union unaligned *up = p; return up->b2; }
static inline unsigned
read_4byte (void *p)
{ union unaligned *up = p; return up->b4; }
static inline unsigned long
read_8byte (void *p)
{ union unaligned *up = p; return up->b8; }

/* This hook required for platforms performing PIC generation.*/

#ifndef READ_DWARF2_UNALIGNED_POINTER
#define READ_DWARF2_UNALIGNED_POINTER(p)	read_pointer ((p)) 
#endif

/* Ordering function for FDEs.  Functions can't overlap, so we just compare
   their starting addresses.  */

static inline saddr
fde_compare (const fde *x, const fde *y)
{
  return (saddr)GET_DWARF2_FDE_INITIAL_PC (x)
	  - (saddr)GET_DWARF2_FDE_INITIAL_PC (y);
}

/* Return the address of the FDE after P.  */

static inline fde *
next_fde (fde *p)
{
  return (fde *)(((char *)p) + p->length + sizeof (p->length));
}

/* Sorting an array of FDEs by address.
   (Ideally we would have the linker sort the FDEs so we don't have to do
   it at run time. But the linkers are not yet prepared for this.)  */

/* This is a special mix of insertion sort and heap sort, optimized for
   the data sets that actually occur. They look like
   101 102 103 127 128 105 108 110 190 111 115 119 125 160 126 129 130.
   I.e. a linearly increasing sequence (coming from functions in the text
   section), with additionally a few unordered elements (coming from functions
   in gnu_linkonce sections) whose values are higher than the values in the
   surrounding linear sequence (but not necessarily higher than the values
   at the end of the linear sequence!).
   The worst-case total run time is O(N) + O(n log (n)), where N is the
   total number of FDEs and n is the number of erratic ones.  */

typedef struct fde_vector
{
  fde **array;
  size_t count;
} fde_vector;

typedef struct fde_accumulator
{
  fde_vector linear;
  fde_vector erratic;
} fde_accumulator;

static inline void
start_fde_sort (fde_accumulator *accu, size_t count)
{
  accu->linear.array = (fde **) malloc (sizeof (fde *) * count);
  accu->erratic.array = (fde **) malloc (sizeof (fde *) * count);
  accu->linear.count = 0;
  accu->erratic.count = 0;
}

static inline void
fde_insert (fde_accumulator *accu, fde *this_fde)
{
  accu->linear.array[accu->linear.count++] = this_fde;
}

/* Split LINEAR into a linear sequence with low values and an erratic
   sequence with high values, put the linear one (of longest possible
   length) into LINEAR and the erratic one into ERRATIC. This is O(N).  */
static inline void
fde_split (fde_vector *linear, fde_vector *erratic)
{
  size_t count = linear->count;
  size_t linear_max = (size_t) -1;
  size_t previous_max[count];
  size_t i, j;

  for (i = 0; i < count; i++)
    {
      for (j = linear_max;
           j != (size_t) -1
           && fde_compare (linear->array[i], linear->array[j]) < 0;
           j = previous_max[j])
        {
          erratic->array[erratic->count++] = linear->array[j];
          linear->array[j] = (fde *) NULL;
        }
      previous_max[i] = j;
      linear_max = i;
    }

  for (i = 0, j = 0; i < count; i++)
    if (linear->array[i] != (fde *) NULL)
      linear->array[j++] = linear->array[i];
  linear->count = j;
}

/* This is O(n log(n)).  BSD/OS defines heapsort in stdlib.h, so we must
   use a name that does not conflict.  */
static inline void
frame_heapsort (fde_vector *erratic)
{
  /* For a description of this algorithm, see:
     Samuel P. Harbison, Guy L. Steele Jr.: C, a reference manual, 2nd ed.,
     p. 60-61. */
  fde ** a = erratic->array;
  /* A portion of the array is called a "heap" if for all i>=0:
     If i and 2i+1 are valid indices, then a[i] >= a[2i+1].
     If i and 2i+2 are valid indices, then a[i] >= a[2i+2]. */
#define SWAP(x,y) do { fde * tmp = x; x = y; y = tmp; } while (0)
  size_t n = erratic->count;
  size_t m = n;
  size_t i;

  while (m > 0)
    {
      /* Invariant: a[m..n-1] is a heap. */
      m--;
      for (i = m; 2*i+1 < n; )
        {
          if (2*i+2 < n
              && fde_compare (a[2*i+2], a[2*i+1]) > 0
              && fde_compare (a[2*i+2], a[i]) > 0)
            {
              SWAP (a[i], a[2*i+2]);
              i = 2*i+2;
            }
          else if (fde_compare (a[2*i+1], a[i]) > 0)
            {
              SWAP (a[i], a[2*i+1]);
              i = 2*i+1;
            }
          else
            break;
        }
    }
  while (n > 1)
    {
      /* Invariant: a[0..n-1] is a heap. */
      n--;
      SWAP (a[0], a[n]);
      for (i = 0; 2*i+1 < n; )
        {
          if (2*i+2 < n
              && fde_compare (a[2*i+2], a[2*i+1]) > 0
              && fde_compare (a[2*i+2], a[i]) > 0)
            {
              SWAP (a[i], a[2*i+2]);
              i = 2*i+2;
            }
          else if (fde_compare (a[2*i+1], a[i]) > 0)
            {
              SWAP (a[i], a[2*i+1]);
              i = 2*i+1;
            }
          else
            break;
        }
    }
#undef SWAP
}

/* Merge V1 and V2, both sorted, and put the result into V1. */
static void
fde_merge (fde_vector *v1, const fde_vector *v2)
{
  size_t i1, i2;
  fde * fde2;

  i2 = v2->count;
  if (i2 > 0)
    {
      i1 = v1->count;
      do {
        i2--;
        fde2 = v2->array[i2];
        while (i1 > 0 && fde_compare (v1->array[i1-1], fde2) > 0)
          {
            v1->array[i1+i2] = v1->array[i1-1];
            i1--;
          }
        v1->array[i1+i2] = fde2;
      } while (i2 > 0);
      v1->count += v2->count;
    }
}


/* Simple-minded duplicate FDE pruner.  Handy for coalesced symbols.  */
static
void prune_duplicate_fdes (fde_vector *v)
{
  size_t ix, num, dupcount;

  num = v->count;
  if (num <= 1)
    return;
  dupcount = 0;
  for (ix = 0; ix < num - 1; ++ix)
    {
      if (fde_compare (v->array[ix], v->array[ix+1]) == 0)
	{
	  /* Same symbol and different lengths?  Game over, man!  */

	  if (v->array[ix]->pc_range != v->array[ix+1]->pc_range)
	    {
	      /* Argh!  We evidently have two different-sized routines,
		 this is possible if there were different optimization
		 levels.  Rather than terminate, we try to figure out
		 which of the two lengths is more "plausible" and use
		 that one.  Need to talk to Kev about this one!!  */
#ifdef __DEBUG_EH__
	      fprintf (stderr, "####"
	        " index %d,%d compare equal (PC=%08x), but pc_ranges differ"
	        " (%d != %d)\n####\n", ix, ix+1, 
		GET_DWARF2_FDE_INITIAL_PC (v->array[ix]),
		v->array[ix]->pc_range,
		v->array[ix+1]->pc_range);
#endif

#ifdef PLAUSIBLE_PC_RANGE
	      /* If the FIRST (ix-th) item is the most plausible PC range,
		 swap items (ix) and (ix+1).  */

	      if (PLAUSIBLE_PC_RANGE (GET_DWARF2_FDE_INITIAL_PC (v->array[ix]),
					v->array[ix]->pc_range))
		{
#ifdef __DEBUG_EH__
		  fprintf (stderr, "1st item (len %d) deemed more plausible\n",
			v->array[ix]->pc_range);
#endif
		  v->array[ix+1] = v->array[ix];

		  /* v->array[ix] gets zeroed below so there's no point in
		     doing a real swap.  */
		}
#else
	       /* No dice.  Bail.  */

	       __terminate ();
#endif	/* PLAUSIBLE_PC_RANGE  */

	    }
	  ++dupcount;
#ifdef __DEBUG_EH__
	  if (__trace_exceptions)
	  fprintf (stderr, "## %d-th duplicate (index %d) removed (pc=%x)\n",
		dupcount, ix, GET_DWARF2_FDE_INITIAL_PC (v->array[ix]));
#endif
	  v->array[ix] = 0;
	  v->count--;
	}
    }

  /* Slide the elements of ARRAY back over any duplicates.  */
  if (dupcount != 0)
    {
      int w;
      for (w = ix = 0; ix < num; ++ix)
	if (v->array[ix])
	  v->array[w++] = v->array[ix];
    }
}

static fde **
end_fde_sort (fde_accumulator *accu, size_t count, size_t *final_count)
{
  if (accu->linear.count != count)
    abort ();
  fde_split (&accu->linear, &accu->erratic);
  if (accu->linear.count + accu->erratic.count != count)
    abort ();
  frame_heapsort (&accu->erratic);

  prune_duplicate_fdes (&accu->erratic);

  fde_merge (&accu->linear, &accu->erratic);
  free (accu->erratic.array);

#ifdef __DEBUG_EH__
  /* Make sure that we're actually sorted... */
  if (accu->linear.count > 1) {
    const fde_vector *v = &accu->linear;
    size_t ix, count = v->count;

    for (ix = 0; ix < count - 1; ++ix)
      {
	const fde *f = v->array[ix];
	const fde *g = v->array[ix+1];
	int cmp = fde_compare (f, g);

	/* Could be duplicated/coalesced.  Might be a good idea to shrink.  */

        if (cmp > 0)
	  {
	    fprintf (stderr, (cmp) ? "## NOT SORTED!! " : "   Duplicated ");

	    fprintf (stderr, "(index %d) PC %08x, len %08x (%6d)\n", ix,
		    GET_DWARF2_FDE_INITIAL_PC (f), f->pc_range, f->pc_range);

	    fprintf (stderr, "\t(index %d) PC %08x, len %08x (%6d)\n", ix+1,
		    GET_DWARF2_FDE_INITIAL_PC (g), g->pc_range, g->pc_range);

	    if (__trace_exceptions & 0x1000) __terminate ();
	  }
      }
  }
#endif	/* __DEBUG_EH__  */

  *final_count = accu->linear.count;
  return accu->linear.array;
}

static size_t
count_fdes (fde *this_fde, long section_size)
{
  size_t count;

  for (count = 0; (section_size > 0) && (this_fde->length != 0); 
  	this_fde = next_fde (this_fde))
    {
      section_size -= (this_fde->length + 4);
    
      /* Skip CIEs and linked once FDE entries.  */
      if (this_fde->CIE_delta == 0 || GET_DWARF2_FDE_INITIAL_PC (this_fde) == 0)
      	{
	  TRACE_EXCEPTIONS (TR_FDE, "count_fdes: Skipping CIE address=0x%08x\n",
					this_fde);
	  TRACE_EXCEPTIONS (TR_FDE, "            (initial pc was 0x%08x)\n",
					GET_DWARF2_FDE_INITIAL_PC (this_fde));
	  continue;
	}

      /* Check for bizarreness when pc_range is negative.  */
      if (((signed long)this_fde->pc_range) <= 0)
	{

#ifdef __DEBUG_EH__
	  fprintf (stderr, "\n### FUNCTION 0x%08x HAS ILLEGAL LENGTH %d, skipping", GET_DWARF2_FDE_INITIAL_PC (this_fde), this_fde->pc_range);
#endif /* __DEBUG_EH__  */

	  continue;
	}

      TRACE_EXCEPTIONS (TR_FDE, "count_fdes: FDE addr=0x%08x\n", this_fde);
      TRACE_EXCEPTIONS (TR_FDE, "            FDE size=0x%x\n",
							this_fde->length);
      TRACE_EXCEPTIONS (TR_FDE, "            FDE initial PC=0x%08x\n",
    					GET_DWARF2_FDE_INITIAL_PC (this_fde));
      TRACE_EXCEPTIONS (TR_FDE, "            FDE func size(pc_range)=0x%08x\n",
							this_fde->pc_range);

      ++count;
    }

  TRACE_EXCEPTIONS (TR_FDE, "count_fdes: #unused bytes in section=%d\n",section_size);

  return count;
}

static void
add_fdes (fde *this_fde, fde_accumulator *accu, void **beg_ptr, void **end_ptr, long section_size)
{
  void *pc_begin = *beg_ptr;
  void *pc_end = *end_ptr;

  for (; (section_size > 0) && (this_fde->length != 0); this_fde = next_fde (this_fde))
    {
      section_size -= (this_fde->length + 4);
 
      /* Skip CIEs and linked once FDE entries.  */
      if (this_fde->CIE_delta == 0 || GET_DWARF2_FDE_INITIAL_PC (this_fde) == 0)
	continue;

      if (((signed long)this_fde->pc_range) <= 0)
	continue;

      fde_insert (accu, this_fde);

      if (GET_DWARF2_FDE_INITIAL_PC (this_fde) < pc_begin)
	pc_begin = GET_DWARF2_FDE_INITIAL_PC (this_fde);
      if (GET_DWARF2_FDE_INITIAL_PC (this_fde) + this_fde->pc_range > pc_end)
	pc_end = GET_DWARF2_FDE_INITIAL_PC (this_fde) + this_fde->pc_range;
    }

  *beg_ptr = pc_begin;
  *end_ptr = pc_end;
}

/* Set up a sorted array of pointers to FDEs for a loaded object.  We
   count up the entries before allocating the array because it's likely to
   be faster.  */

#define NO_FRAME_SECTION_SIZE	0x7FFFFFFF

#ifndef FRAME_SECTION_SIZE
/* don't use frame section size.  */
#define FRAME_SECTION_SIZE(OB)	NO_FRAME_SECTION_SIZE
#endif

static void
frame_init (struct object* ob)
{
  size_t count, final_count;
  fde_accumulator accu;
  void *pc_begin, *pc_end;

  if (ob->fde_array)
    {
      fde **p = ob->fde_array;
      for (count = 0; *p; ++p)
	count += count_fdes (*p, NO_FRAME_SECTION_SIZE);
    }
  else
    count = count_fdes (ob->fde_begin, FRAME_SECTION_SIZE(ob));

#ifdef __DEBUG_EH__
  if (count == 0)
    abort ();
#endif
  ob->count = final_count = count;

  start_fde_sort (&accu, count);
  pc_begin = (void*)(uaddr)-1;
  pc_end = 0;

  if (ob->fde_array)
    {
      fde **p = ob->fde_array;
      for (; *p; ++p)
	add_fdes (*p, &accu, &pc_begin, &pc_end, NO_FRAME_SECTION_SIZE);
    }
  else
    add_fdes (ob->fde_begin, &accu, &pc_begin, &pc_end, FRAME_SECTION_SIZE(ob));

  ob->fde_array = end_fde_sort (&accu, count, &final_count);
  ob->pc_begin = pc_begin;
  ob->pc_end = pc_end;
  TRACE_EXCEPTIONS (TR_ALL, "frame_init: image starting pc=0x%08x\n", pc_begin);
  TRACE_EXCEPTIONS (TR_ALL, "frame_init: image ending pc=0x%08x\n", pc_end);

#ifdef __DEBUG_EH__
  if (count != final_count && __trace_exceptions)
    fprintf (stderr, "### Discarded %d duplicates (orig %d, now %d)\n",
	count - final_count, count, final_count);
#endif 

  ob->count = final_count;
}

/* Return a pointer to the FDE for the function containing PC.  */

static fde *
find_fde (void *pc)
{
  struct object *ob;
  size_t lo, hi;

 TRACE_EXCEPTIONS (TR_ALL, "find_fde: ------Stack Frame---- pc = 0x%08x\n",pc);

 GET_AND_LOCK_TARGET_OBJECT_LIST (ob);
  for ( ; ob; ob = ob->next)
    {
      if (ob->pc_begin == 0)
	frame_init (ob);
      if (pc >= ob->pc_begin && pc < ob->pc_end)
        {
          TRACE_EXCEPTIONS (TR_ALL, "find_fde: image MATCH: sect addr=0x%08x\n",
				ob->fde_begin);
          TRACE_EXCEPTIONS (TR_ALL, "          section size: %d bytes\n",
				FRAME_SECTION_SIZE(ob));
          TRACE_EXCEPTIONS (TR_ALL, "          section pc_begin: 0x%08x\n",
				ob->pc_begin);
          TRACE_EXCEPTIONS (TR_ALL, "          section pc_end:   0x%08x\n",
				ob->pc_end);
	  break;
	}
    }
 
  UNLOCK_TARGET_OBJECT_LIST();

   if (ob == 0)
    {
      TRACE_EXCEPTIONS (TR_ALL, "--> NO OBJECT MATCHING PC=0x%08x\n", pc);
      return 0;
    }

  /* Standard binary search algorithm.  */
  for (lo = 0, hi = ob->count; lo < hi; )
    {
      size_t i = (lo + hi) / 2;
      fde *f = ob->fde_array[i];

      if (pc < GET_DWARF2_FDE_INITIAL_PC (f))
	hi = i;
      else if (pc >= GET_DWARF2_FDE_INITIAL_PC (f) + f->pc_range)
	lo = i + 1;
      else
      	{
      	TRACE_EXCEPTIONS (TR_ALL, "find_fde: fde matched=0x%08x\n", f);

#ifdef __DEBUG_EH__
	/* For some bizarre reason, we sometimes get HUGE fde entries
	   which don't match any real function entry point but whose
	   range is such that we falsely match.  Need to talk to Kev
	   about this, 'cos I don't think we generate 'em.

	   For now, we keep searching for a "better fit."  */
	{
	  fde *n_fde, *best_fit = f;
	  size_t next_ix = i;

	  while (++next_ix < ob->count
		&& (n_fde = ob->fde_array[next_ix]) != NULL
		&& pc >= GET_DWARF2_FDE_INITIAL_PC (n_fde)
		&& n_fde->pc_range < ob->fde_array[next_ix-1]->pc_range
		&& pc < GET_DWARF2_FDE_INITIAL_PC (n_fde) + n_fde->pc_range)
	    best_fit = n_fde;

	  if (best_fit != f)
	    {
	      fprintf (stderr, "## Broken FDE index, was %d, "
			"fixed to %d, fde now 0x%x (for pc=%x)\n",
			i, next_ix-1, best_fit, pc);
	      return best_fit;
	    }
	}
#endif	/* __DEBUG_EH__  */
	return f;
	}
    }

  TRACE_EXCEPTIONS (TR_ALL, "find_fde: No fde matches pc 0x%08x\n", pc);
  return 0;
}

static inline struct dwarf_cie *
get_cie (fde *f)
{
  TRACE_EXCEPTIONS (TR_CIE, "get_cie: from fde=0x%08x\n", f);
  TRACE_EXCEPTIONS (TR_CIE, "get_cie: CIE referenced=0x%08x\n",
			(((void *)&f->CIE_delta) - f->CIE_delta));
  return ((void *)&f->CIE_delta) - f->CIE_delta;
}

/* Extract any interesting information from the CIE for the translation
   unit F belongs to.  */

static void *
extract_cie_info (fde *f, struct cie_info *c)
{
  void *p;
  int i;

  c->augmentation = get_cie (f)->augmentation;
  
  TRACE_EXCEPTIONS (TR_CIE, "extract_cie_info: augmentation string [%s]\n",
			c->augmentation);

  if (strcmp (c->augmentation, "") != 0
      && strcmp (c->augmentation, "eh") != 0
      && c->augmentation[0] != 'z')
    {
      TRACE_EXCEPTIONS (TR_CIE, "Invalid augmentation (0x%08x)!!\n",
			c->augmentation);
      return 0;
    }

  p = c->augmentation + strlen (c->augmentation) + 1;

  if (strcmp (c->augmentation, "eh") == 0)
    {
      c->eh_ptr = READ_DWARF2_UNALIGNED_POINTER (p);
      TRACE_EXCEPTIONS (TR_CIE, "extract_cie_info: exception table at 0x%08x\n",
			c->eh_ptr);
      p += sizeof (void *);
    }
  else {
    TRACE_EXCEPTIONS (TR_ALL, "*NO* exception table in CIE referenced from FDE 0x%08x!\n", f);
    c->eh_ptr = 0;
  }

  p = decode_uleb128 (p, &c->code_align);
  p = decode_sleb128 (p, &c->data_align);
  c->ra_regno = *(unsigned char *)p++;
  
  TRACE_EXCEPTIONS (TR_CIE, "extract_cie_info: code alignment=%d\n",
			c->code_align);
  TRACE_EXCEPTIONS (TR_CIE, "                  data alignment=%d\n",
			c->data_align);
  TRACE_EXCEPTIONS (TR_CIE, "                  return address reg=%d\n",
			c->ra_regno);

  /* If the augmentation starts with 'z', we now see the length of the
     augmentation fields.  */
  if (c->augmentation[0] == 'z')
    {
      p = decode_uleb128 (p, &i);
      p += i;
    }

  TRACE_EXCEPTIONS (TR_CIE, "extract_cie_info: Instructions starting addr=0x%08x\n", p);

  return p;
}

/* Decode one instruction's worth of DWARF 2 call frame information.
   Used by __frame_state_for.  Takes pointers P to the instruction to
   decode, STATE to the current register unwind information, INFO to the
   current CIE information, and PC to the current PC value.  Returns a
   pointer to the next instruction.  */

static void *
execute_cfa_insn (void *p, struct frame_state_internal *state,
		  struct cie_info *info, void **pc)
{
  unsigned insn = *(unsigned char *)p++;
  unsigned reg;
  int offset;

  if (insn & DW_CFA_advance_loc) 
    {
    TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_advance_loc(%d)\n", ((insn & 0x3f) * info->code_align));
    		
    *pc += ((insn & 0x3f) * info->code_align);
    }
  else if (insn & DW_CFA_offset)
    {
      reg = (insn & 0x3f);
      p = decode_uleb128 (p, &offset);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_offset(reg=%d,\n",reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn:               offset=%d)\n",offset);
      offset *= info->data_align;
      state->s.saved[reg] = REG_SAVED_OFFSET;
      state->s.reg_or_offset[reg] = offset;
    }
  else if (insn & DW_CFA_restore)
    {
      reg = (insn & 0x3f);
      state->s.saved[reg] = REG_UNSAVED;
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_restore(%d)\n",reg);
    }
  else switch (insn)
    {
    case DW_CFA_set_loc:
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_set_loc(0x%08x)\n",
      			READ_DWARF2_UNALIGNED_POINTER (p));
      *pc = READ_DWARF2_UNALIGNED_POINTER (p);
      p += sizeof (void *);
      break;
    case DW_CFA_advance_loc1:
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_advance_loc1(0x%08x)\n",
      			read_1byte (p));
      *pc += read_1byte (p);
      p += 1;
      break;
    case DW_CFA_advance_loc2:
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_advance_loc2(0x%08x)\n",
      			read_2byte (p));
      *pc += read_2byte (p);
      p += 2;
      break;
    case DW_CFA_advance_loc4:
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_advance_loc4(0x%08x)\n",
      			read_4byte (p));
      *pc += read_4byte (p);
      p += 4;
      break;

    case DW_CFA_offset_extended:
      p = decode_uleb128 (p, &reg);
      p = decode_uleb128 (p, &offset);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_offset_extended(reg=%d,\n",reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn:               offset=%d)\n",offset);
      offset *= info->data_align;
      state->s.saved[reg] = REG_SAVED_OFFSET;
      state->s.reg_or_offset[reg] = offset;
      break;
    case DW_CFA_restore_extended:
      p = decode_uleb128 (p, &reg);
      state->s.saved[reg] = REG_UNSAVED;
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_restore_extended(%d)\n",reg);
      break;

    case DW_CFA_undefined:
      p = decode_uleb128 (p, &reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_undefined(%d)\n",reg);
      break;

    case DW_CFA_same_value:
      p = decode_uleb128 (p, &reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_same_value(%d)\n",reg);
      break;

    case DW_CFA_nop:
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_nop\n",0);
      break;

    case DW_CFA_register:
      {
	unsigned reg2;
	p = decode_uleb128 (p, &reg);
	p = decode_uleb128 (p, &reg2);
      	TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_register(reg1=%d,\n",reg);
      	TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn:               reg2=%d)\n",reg2);
	state->s.saved[reg] = REG_SAVED_REG;
	state->s.reg_or_offset[reg] = reg2;
      }
      break;

    case DW_CFA_def_cfa:
      p = decode_uleb128 (p, &reg);
      p = decode_uleb128 (p, &offset);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_def_cfa(reg=%d,\n",reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn:               offset=%d)\n",offset);
      state->s.cfa_reg = reg;
      state->s.cfa_offset = offset;
      break;
    case DW_CFA_def_cfa_register:
      p = decode_uleb128 (p, &reg);
      state->s.cfa_reg = reg;
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_def_cfa_register(%d)\n",reg);
      break;
    case DW_CFA_def_cfa_offset:
      p = decode_uleb128 (p, &offset);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_def_cfa_offset(%d)\n",offset);
      state->s.cfa_offset = offset;
      break;
      
    case DW_CFA_remember_state:
      {
	struct frame_state_internal *save =
	  (struct frame_state_internal *)
	  malloc (sizeof (struct frame_state_internal));
	memcpy (save, state, sizeof (struct frame_state_internal));
	state->saved_state = save;
      }
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_remember_state\n",0);
      break;
    case DW_CFA_restore_state:
      {
	struct frame_state_internal *save = state->saved_state;
	memcpy (state, save, sizeof (struct frame_state_internal));
	free (save);
      }
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_restore_state\n",0);
      break;

      /* FIXME: Hardcoded for SPARC register window configuration.  */
    case DW_CFA_GNU_window_save:
      for (reg = 16; reg < 32; ++reg)
	{
	  state->s.saved[reg] = REG_SAVED_OFFSET;
	  state->s.reg_or_offset[reg] = (reg - 16) * sizeof (void *);
	}
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_GNU_window_save\n",0);
      break;

    case DW_CFA_GNU_args_size:
      p = decode_uleb128 (p, &offset);
      state->s.args_size = offset;
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: DW_CFA_GNU_args_size(%d)\n",offset);
      break;

    case DW_CFA_GNU_negative_offset_extended:
      p = decode_uleb128 (p, &reg);
      p = decode_uleb128 (p, &offset);
      offset *= info->data_align;
      state->s.saved[reg] = REG_SAVED_OFFSET;
      state->s.reg_or_offset[reg] = -offset;
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: "
			"DW_CFA_GNU_negative_offset_extended reg=%d,\n",
			reg);
      TRACE_EXCEPTIONS (TR_EXE, "execute_cfa_insn: "
			"                                     offs=%d)\n",
			-offset);
      break;

    default:
      TRACE_EXCEPTIONS (TR_ALL, "execute_cfa_insn: ILLEGAL INSTRUCTION\n",0);    
      abort ();
    }
  return p;
}

#ifdef TARGET_SPECIFIC_OBJECT_REGISTER_FUNCS

/* This target has its own method of registering frame info.  */

TARGET_SPECIFIC_OBJECT_REGISTER_FUNCS

#else

/* Called from crtbegin.o to register the unwind info for an object.  */

void
__register_frame_info (void *begin, struct object *ob)
{
  ob->fde_begin = begin;

  ob->pc_begin = ob->pc_end = 0;
  ob->fde_array = 0;
  ob->count = 0;

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  ob->next = objects;
  objects = ob;

  __gthread_mutex_unlock (&object_mutex);
}

void
__register_frame (void *begin)
{
  struct object *ob = (struct object *) malloc (sizeof (struct object));
  __register_frame_info (begin, ob);                       
}

/* Similar, but BEGIN is actually a pointer to a table of unwind entries
   for different translation units.  Called from the file generated by
   collect2.  */

void
__register_frame_info_table (void *begin, struct object *ob)
{
  ob->fde_begin = begin;
  ob->fde_array = begin;

  ob->pc_begin = ob->pc_end = 0;
  ob->count = 0;

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  ob->next = objects;
  objects = ob;

  __gthread_mutex_unlock (&object_mutex);
}

void
__register_frame_table (void *begin)
{
  struct object *ob = (struct object *) malloc (sizeof (struct object));
  __register_frame_info_table (begin, ob);
}

/* Called from crtbegin.o to deregister the unwind info for an object.  */

void *
__deregister_frame_info (void *begin)
{
  struct object **p;

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  p = &objects;
  while (*p)
    {
      if ((*p)->fde_begin == begin)
	{
	  struct object *ob = *p;
	  *p = (*p)->next;

	  /* If we've run init_frame for this object, free the FDE array.  */
	  if (ob->pc_begin)
	    free (ob->fde_array);

	  __gthread_mutex_unlock (&object_mutex);
	  return (void *) ob;
	}
      p = &((*p)->next);
    }

  __gthread_mutex_unlock (&object_mutex);
  abort ();
}

void
__deregister_frame (void *begin)
{
  free (__deregister_frame_info (begin));
}

#endif /* TARGET_SPECIFIC_OBJECT_REGISTER_FUNCS  */

/* Called from __throw to find the registers to restore for a given
   PC_TARGET.  The caller should allocate a local variable of `struct
   frame_state' (declared in frame.h) and pass its address to STATE_IN.  */

struct frame_state *
__frame_state_for (void *pc_target, struct frame_state *state_in)
{
  fde *f;
  void *insn, *end, *pc;
  struct cie_info info;
  struct frame_state_internal state;

  f = find_fde (pc_target);
  if (f == 0)
    return 0;

  insn = extract_cie_info (f, &info);
  if (insn == 0)
    return 0;

  memset (&state, 0, sizeof (state));
  state.s.retaddr_column = info.ra_regno;
  state.s.eh_ptr = info.eh_ptr;

  /* First decode all the insns in the CIE.  */
  end = next_fde ((fde*) get_cie (f));
  
  TRACE_EXCEPTIONS (TR_ALL, "frame_state_for: Executing CIE Instructions starting at: 0x%08x\n",insn);
  TRACE_EXCEPTIONS (TR_ALL, "frame_state_for: Execution will end before FDE: 0x%08x\n",end);
  
  while (insn < end)
    insn = execute_cfa_insn (insn, &state, &info, 0);

  insn = ((fde *)f) + 1;

  if (info.augmentation[0] == 'z')
    {
      int i;
      insn = decode_uleb128 (insn, &i);
      insn += i;
    }

  /* Then the insns in the FDE up to our target PC.  */
  end = next_fde (f);
  pc = GET_DWARF2_FDE_INITIAL_PC (f);
  
  TRACE_EXCEPTIONS (TR_ALL, "frame_state_for: Executing instructions for FDE: 0x%08x\n", f);
  TRACE_EXCEPTIONS (TR_ALL, "frame_state_for: Execute up to state PC=0x%08x\n",pc_target);
  TRACE_EXCEPTIONS (TR_ALL, "Start PC at: 0x%08x, ", pc);
  TRACE_EXCEPTIONS (TR_ALL, "length (range) of FDE is %d\n", f->pc_range);
  TRACE_EXCEPTIONS (TR_ALL, "frame_state_for: Stop at next FDE: 0x%08x\n",end);
  
  while (insn < end && pc <= pc_target)
    insn = execute_cfa_insn (insn, &state, &info, &pc);

  memcpy (state_in, &state.s, sizeof (state.s));
  TRACE_EXCEPTIONS (TR_ALL, "Returning state in: 0x%08x\n",&state_in);
  return state_in;
}
#endif /* DWARF2_UNWIND_INFO */
