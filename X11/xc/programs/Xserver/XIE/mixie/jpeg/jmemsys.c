/* $Xorg: jmemsys.c,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* Module jmemsys.c */

/****************************************************************************

Copyright 1993, 1994, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


				NOTICE
                              
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

     Permission to use, copy, modify, distribute and sell this
     software and its documentation for any purpose and without
     fee or royalty and to grant others any or all rights granted
     herein is hereby granted, provided that you agree to comply
     with the following copyright notice and statements, including
     the disclaimer, and that the same appears on all copies and
     derivative works of the software and documentation you make.
     
     "Copyright 1993, 1994 by AGE Logic, Inc."
     
     THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
     REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
     example, but not limitation, AGE LOGIC MAKE NO
     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
     FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
     INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC 
     SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
     EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
     INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
     OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
     ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
     BASED ON A WARRANTY, EVEN IF AGE LOGIC LICENSEES
     HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
     DAMAGES.
    
     The name of AGE Logic, Inc. may not be used in
     advertising or publicity pertaining to this software without
     specific, written prior permission from AGE Logic.

     Title to this software shall at all times remain with AGE
     Logic, Inc.
*****************************************************************************

	Gary Rogers, AGE Logic, Inc., October 1993
	Gary Rogers, AGE Logic, Inc., January 1994

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/mixie/jpeg/jmemsys.c,v 3.4 2001/12/14 19:58:39 dawes Exp $ */

/*
 * jmemnobs.c  (jmemsys.c)
 *
 * Copyright (C) 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file provides a really simple implementation of the system-
 * dependent portion of the JPEG memory manager.  This implementation
 * assumes that no backing-store files are needed: all required space
 * can be obtained from malloc().
 * This is very portable in the sense that it'll compile on almost anything,
 * but you'd better have lots of main memory (or virtual memory) if you want
 * to process big images.
 * Note that the max_memory_to_use option is ignored by this implementation.
 */

#include "jinclude.h"
#include "jmemsys.h"

#ifdef _XIEC_MEMORY
#include <memory.h>
#ifdef malloc
#undef malloc
#endif
#ifdef free
#undef free
#endif
#define malloc(size)    XieMalloc(size)
#define free(ptr)       XieFree(ptr)
#else
#ifdef INCLUDES_ARE_ANSI
#include <stdlib.h>		/* to declare malloc(), free() */
#else
extern pointer malloc PP((size_t size));
extern void free PP(pointer ptr));
#endif
#endif  /* _XIEC_MEMORY */


#ifndef XIE_SUPPORTED
static external_methods_ptr methods; /* saved for access to error_exit */
#endif	/* XIE_SUPPORTED */

/*
 * Memory allocation and freeing are controlled by the regular library
 * routines malloc() and free().
 */

GLOBAL pointer
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jget_small (size_t sizeofobject)
#else
jget_small (sizeofobject)
	size_t sizeofobject;
#endif	/* NeedFunctionPrototypes */
#else
jget_small (size_t sizeofobject)
#endif	/* XIE_SUPPORTED */
{
  return (pointer) malloc(sizeofobject);
}

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jfree_small (pointer object)
#else
jfree_small (object)
	pointer object;
#endif	/* NeedFunctionPrototypes */
#else
jfree_small (pointer object)
#endif	/* XIE_SUPPORTED */
{
  free(object);
}

/*
 * We assume NEED_FAR_POINTERS is not defined and so the separate entry points
 * jget_large, jfree_large are not needed.
 */


/*
 * This routine computes the total memory space available for allocation.
 * Here we always say, "we got all you want bud!"
 */

#ifndef XIE_SUPPORTED
GLOBAL long
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jmem_available (long min_bytes_needed, long max_bytes_needed)
#else
jmem_available (min_bytes_needed, max_bytes_needed)
	long min_bytes_needed;
	long max_bytes_needed;
#endif	/* NeedFunctionPrototypes */
#else
jmem_available (long min_bytes_needed, long max_bytes_needed)
#endif	/* XIE_SUPPORTED */
{
  return max_bytes_needed;
}


/*
 * Backing store (temporary file) management.
 * This should never be called and we just error out.
 */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jopen_backing_store (backing_store_ptr info, long total_bytes_needed)
#else
jopen_backing_store (info, total_bytes_needed)
	backing_store_ptr info;
	long total_bytes_needed;
#endif	/* NeedFunctionPrototypes */
#else
jopen_backing_store (backing_store_ptr info, long total_bytes_needed)
#endif	/* XIE_SUPPORTED */
{
  ERREXIT(methods, "Backing store not supported");
}
#endif   /* XIE_SUPPORTED */


/*
 * These routines take care of any system-dependent initialization and
 * cleanup required.  Keep in mind that jmem_term may be called more than
 * once.
 */

#ifndef XIE_SUPPORTED
GLOBAL void
jmem_init (external_methods_ptr emethods)
{
  methods = emethods;		/* save struct addr for error exit access */
  emethods->max_memory_to_use = 0;
}
#endif	/* XIE_SUPPORTED */

GLOBAL void
#ifdef XIE_SUPPORTED
#if NeedFunctionPrototypes
jmem_term (void)
#else
jmem_term ()
#endif	/* NeedFunctionPrototypes */
#else
jmem_term (void)
#endif	/* XIE_SUPPORTED */
{
  /* no work */
}
