/* $Xorg: pexUtils.h,v 1.4 2001/02/09 02:04:19 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef PEXUTILS_H
#define PEXUTILS_H

#include "ddpex.h"

/* declarations for the utilities in pexUtils.c */
extern listofObj  *puCreateList();
extern void	puDeleteList();
extern short	puInList();
extern short	puAddToList();
extern short	puRemoveFromList();
extern short	puCopyList();
extern short	puMergeLists();
extern int	puBuffRealloc();
extern void	puInitList();
extern int	puCountList();

/* useful macros to use with lists */
#define PU_TRUE	1
#define PU_FALSE 0

#define	PU_BAD_LIST	-1	/* error returned by list utilities */
 
#define PU_EMPTY_LIST( plist )  \
	(plist)->numObj = 0

#define PU_REMOVE_LAST_OBJ( plist )     \
	if ( (plist)->numObj > 0 )      (plist)->numObj--

/* useful macros to use with ddBuffer */
#define PU_BUF_HDR_SIZE(pBuffer) \
	(int) (((char *) pBuffer->pBuf) - ((char *) pBuffer->pHead))

#define PU_BUF_TOO_SMALL(pBuffer,minSize) \
	((minSize) > ((pBuffer->bufSize) - PU_BUF_HDR_SIZE(pBuffer) + 1))

#define PU_CHECK_BUFFER_SIZE( pBuffer, size ) 				\
    if ( PU_BUF_TOO_SMALL((pBuffer), (size)) )				\
	if (puBuffRealloc((pBuffer), (ddULONG)(size)) != Success )	\
	{								\
	    (pBuffer)->dataSize = 0;					\
	    return( BadAlloc );						\
        }

/*
    useful macro for ensuring that lists of shorts allocate full longwords
 */
#define MAKE_EVEN(_n)  \
	( (_n)%2 ? (_n)+1 : (_n) )


#endif	/* PEXUTILS_H */
