/* $Xorg: memory.c,v 1.4 2001/02/09 02:04:22 xorgcvs Exp $ */
/* Module memory.c */

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

	memory.c: Xie Memory Management Wrapper Routines 

	Dean Verheiden, AGE Logic, Inc., April 1993

****************************************************************************/
/* $XFree86: xc/programs/Xserver/XIE/dixie/request/memory.c,v 3.7 2001/12/14 19:58:10 dawes Exp $ */

#define _XIEC_MEMORY

/*
**  Include files
*/
/*
 *  Core X Includes
 */
#include <X11/X.h>
#include <X11/Xfuncs.h>

#include "misc.h"	/* for pointer */
#include <memory.h>	/* to assert declaration and definition match */
#include "macro.h"


int ALLOCS = 0; /* DEBUG */

pointer XieMalloc(unsigned size)
{
  char *memptr = (char *)xalloc(size);

  if(memptr)
    ++ALLOCS; /* DEBUG */
  return (pointer )(memptr);
}


pointer XieRealloc(pointer ptr, unsigned size)
{
    return (pointer )xrealloc(ptr,size);    
}

pointer XieCalloc(unsigned size)
{
  char *memptr = (char *)xalloc(size);

  if(memptr) {
    ++ALLOCS; /* DEBUG */
    bzero(memptr,size);
  }
  return (pointer )(memptr);
}

pointer XieFree(pointer ptr)
{
  if(ptr) {
    --ALLOCS; /* DEBUG */
    xfree(ptr);
  }
  return(0);
}

/* End of module memory.c */
