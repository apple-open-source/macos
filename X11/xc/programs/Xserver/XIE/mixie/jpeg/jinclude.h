/* $Xorg: jinclude.h,v 1.4 2001/02/09 02:04:29 xorgcvs Exp $ */
/* Module jinclude.h */

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
/* $XFree86: xc/programs/Xserver/XIE/mixie/jpeg/jinclude.h,v 1.8 2001/12/14 19:58:39 dawes Exp $ */

/*
 * jinclude.h
 *
 * Copyright (C) 1991, 1992, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This is the central file that's #include'd by all the JPEG .c files.
 * Its purpose is to provide a single place to fix any problems with
 * including the wrong system include files.
 * You can edit these declarations if you use a system with nonstandard
 * system include files.
 */

#ifndef NO_XIE

#define _XIEC_MEMORY	/* for XieMalloc and XieFree, used in jmemsys.c */
#define XIE_SUPPORTED
#ifndef XFree86LOADER
#include <stdio.h>
#else
#include "xf86_ansic.h"
#endif
#include "misc.h" /* for pointer ;*/

#define INCLUDES_ARE_ANSI


#undef SIZEOF			/* in case you included X11/xmd.h */
#define SIZEOF(object)	((size_t) sizeof(object))
#define MEMZERO(target,size)	memset((void *)(target), 0, (size_t)(size))
#define MEMCOPY(dest,src,size)	memcpy((void *)(dest), (const void *)(src), (size_t)(size))

#else

/*
 * Normally the __STDC__ macro can be taken as indicating that the system
 * include files conform to the ANSI C standard.  However, if you are running
 * GCC on a machine with non-ANSI system include files, that is not the case.
 * In that case change the following, or add -DNONANSI_INCLUDES to your CFLAGS.
 */

#ifdef __STDC__
#ifndef NONANSI_INCLUDES
#define INCLUDES_ARE_ANSI	/* this is what's tested before including */
#endif
#endif

/*
 * <stdio.h> is included to get the FILE typedef and NULL macro.
 * Note that the core portable-JPEG files do not actually do any I/O
 * using the stdio library; only the user interface, error handler,
 * and file reading/writing modules invoke any stdio functions.
 * (Well, we did cheat a bit in jmemmgr.c, but only if MEM_STATS is defined.)
 */

#include <stdio.h>

/*
 * We need the size_t typedef, which defines the parameter type of malloc().
 * In an ANSI-conforming implementation this is provided by <stdio.h>,
 * but on non-ANSI systems it's more likely to be in <sys/types.h>.
 * On some not-quite-ANSI systems you may find it in <stddef.h>.
 */

#ifndef INCLUDES_ARE_ANSI			/* shouldn't need this if ANSI C */
#include <sys/types.h>
#endif
#ifdef __SASC			/* Amiga SAS C provides it in stddef.h. */
#include <stddef.h>
#endif

/*
 * In ANSI C, and indeed any rational implementation, size_t is also the
 * type returned by sizeof().  However, it seems there are some irrational
 * implementations out there, in which sizeof() returns an int even though
 * size_t is defined as long or unsigned long.  To ensure consistent results
 * we always use this SIZEOF() macro in place of using sizeof() directly.
 */

#undef SIZEOF			/* in case you included X11/xmd.h */
#define SIZEOF(object)	((size_t) sizeof(object))

/*
 * fread() and fwrite() are always invoked through these macros.
 * On some systems you may need to twiddle the argument casts.
 * CAUTION: argument order is different from underlying functions!
 */

#define JFREAD(file,buf,sizeofbuf)  \
  ((size_t) fread((void *) (buf), (size_t) 1, (size_t) (sizeofbuf), (file)))
#define JFWRITE(file,buf,sizeofbuf)  \
  ((size_t) fwrite((const void *) (buf), (size_t) 1, (size_t) (sizeofbuf), (file)))

/*
 * We need the memcpy() and strcmp() functions, plus memory zeroing.
 * ANSI and System V implementations declare these in <string.h>.
 * BSD doesn't have the mem() functions, but it does have bcopy()/bzero().
 * Some systems may declare memset and memcpy in <memory.h>.
 *
 * NOTE: we assume the size parameters to these functions are of type size_t.
 * Change the casts in these macros if not!
 */

#ifdef INCLUDES_ARE_ANSI
#include <string.h>
#define MEMZERO(target,size)	memset((void *)(target), 0, (size_t)(size))
#define MEMCOPY(dest,src,size)	memcpy((void *)(dest), (const void *)(src), (size_t)(size))
#else /* not ANSI */
#ifdef BSD
#include <strings.h>
#define MEMZERO(target,size)	bzero((void *)(target), (size_t)(size))
#define MEMCOPY(dest,src,size)	bcopy((const void *)(src), (void *)(dest), (size_t)(size))
#else /* not BSD, assume Sys V or compatible */
#include <string.h>
#define MEMZERO(target,size)	memset((void *)(target), 0, (size_t)(size))
#define MEMCOPY(dest,src,size)	memcpy((void *)(dest), (const void *)(src), (size_t)(size))
#endif /* BSD */
#endif /* ANSI */

#endif /* XIE */

/* Now include the portable JPEG definition files. */

#include "jconfig.h"

#include "jpegdata.h"
