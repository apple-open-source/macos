/* $Xorg: convUtil.c,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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


#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "PEX.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "dipex.h"
#include "pexSwap.h"
#include "pex_site.h"
#include "convertStr.h"

#undef LOCAL_FLAG
#define LOCAL_FLAG
#include "convUtil.h"

#undef LOCAL_FLAG
#define LOCAL_FLAG extern
#include "OCattr.h"
#undef LOCAL_FLAG

/*
	Composite Conversions
 */


void
SwapViewport(swapPtr, ptr) 
pexSwap		*swapPtr;
pexViewport	*ptr;
{
    SWAP_DEVICE_COORD (ptr->minval);
    SWAP_DEVICE_COORD (ptr->maxval);
}

void
SwapViewEntry(swapPtr, ptr) 
pexSwap		*swapPtr;
pexViewEntry	*ptr;
{
    SWAP_CARD16 (ptr->clipFlags);

    SwapNpcSubvolume (swapPtr, &(ptr->clipLimits));

    SWAP_MATRIX (ptr->orientation);
    SWAP_MATRIX (ptr->mapping); 
}

void
SwapViewRep(swapPtr, ptr) 
pexSwap		*swapPtr;
pexViewRep	*ptr;
{
    SWAP_TABLE_INDEX (ptr->index);
    SwapViewEntry (swapPtr, &(ptr->view));
}


void
SwapColourApproxEntry(swapPtr, ptr) 
pexSwap			*swapPtr;
pexColourApproxEntry	*ptr;
{
    SWAP_INT16 (ptr->approxType);
    SWAP_INT16 (ptr->approxModel);
    SWAP_CARD16 (ptr->max1);
    SWAP_CARD16 (ptr->max2);
    SWAP_CARD16 (ptr->max3);
    SWAP_CARD32 (ptr->mult1);
    SWAP_CARD32 (ptr->mult2);
    SWAP_CARD32 (ptr->mult3);
    SWAP_FLOAT (ptr->weight1);
    SWAP_FLOAT (ptr->weight2);
    SWAP_FLOAT (ptr->weight3);
    SWAP_CARD32 (ptr->basePixel);
}


void
SwapDeviceRects(swapPtr, num, ptr)
pexSwap		*swapPtr;
CARD32		num;
pexDeviceRect	*ptr;
{
    CARD32 i;
    pexDeviceRect *pdr = ptr;
    for (i=0; i<num; i++, pdr++){
	SWAP_CARD16(pdr->xmin);
	SWAP_CARD16(pdr->ymin);
	SWAP_CARD16(pdr->xmax);
	SWAP_CARD16(pdr->ymax);
    }
}

void
SwapExtentInfo (swapPtr, num, pe)
pexSwap		*swapPtr;
CARD32		num;
pexExtentInfo	*pe;
{
    CARD32 i;
    for (i=0; i<num; i++, pe++) {
	SWAP_FLOAT (pe->lowerLeft.x);
	SWAP_FLOAT (pe->lowerLeft.y);
	SWAP_FLOAT (pe->upperRight.x);
	SWAP_FLOAT (pe->upperRight.y);
	SWAP_FLOAT (pe->concatpoint.x);
	SWAP_FLOAT (pe->concatpoint.y);
    }
}


unsigned char *
SwapFontProp (swapPtr, pfp)
pexSwap		*swapPtr;
pexFontProp	*pfp;
{
    SWAP_CARD32 (pfp->name);
    SWAP_CARD32 (pfp->value);
    pfp++;

    return (((unsigned char *)pfp));
}


void
SwapElementRange(swapPtr, pe) 
pexSwap		*swapPtr;
pexElementRange	*pe;
{
    SWAP_ELEMENT_POS (pe->position1);
    SWAP_ELEMENT_POS (pe->position2); 
}


void
SwapLocaltransform3ddata(swapPtr, pg) 
pexSwap			*swapPtr;
pexLocalTransform3DData *pg;
{
    SWAP_CARD16 (pg->composition); 
    SWAP_MATRIX (pg->matrix); 
}

void
SwapLocalTransform2DData(swapPtr, pg) 
pexSwap			*swapPtr;
pexLocalTransform2DData *pg;
{
    SWAP_CARD16 (pg->composition); 
    SWAP_MATRIX_3X3 (pg->matrix); 
}


void
SwapNpcSubvolume(swapPtr, ps) 
pexSwap		    *swapPtr;
pexNpcSubvolume	    *ps;
{
    SWAP_COORD3D (ps->minval);
    SWAP_COORD3D (ps->maxval); 
}



SwapListOfOutputCommands (cntxtPtr, num, oc)
pexContext  *cntxtPtr;
CARD32      num;
CARD32	    *oc;
{
    pexElementInfo  *pe;
    int		    i;
    pexSwap	    *swapPtr = cntxtPtr->swap;
    
    for (i = 0; i < num; i++)
    {
	pe = (pexElementInfo *) oc;
	SWAP_ELEMENT_INFO (*pe);
	if (PEXOCAll < pe->elementType && pe->elementType <= PEXMaxOC)
	    cntxtPtr->pexSwapRequestOC[pe->elementType](cntxtPtr->swap,pe);
	oc += pe->length;
    }
}
