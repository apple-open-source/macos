/* $Xorg: uconvRep.c,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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


/*
	Swapping and float conversion routines.

	Variations on a theme by CITI.
 */

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
#define LOCAL_FLAG extern
#include "convUtil.h"
#include "UconvName.h"
#include "OCprim.h"
#include "OCcolour.h"
#include "OCattr.h"

#undef LOCAL_FLAG
#define LOCAL_FLAG
#include "uconvRep.h"

#define PADDING(n) ( (n)&3 ? (4 - ((n)&3)) : 0)

/*************************************************************
	Replies

 *************************************************************/

void
SWAP_FUNC_PREFIX(ConvertGetExtensionInfoReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetExtensionInfoReq	    *strmPtr;
pexGetExtensionInfoReply    *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->majorVersion);
    SWAP_CARD16 (reply->minorVersion);
    SWAP_CARD32 (reply->release);
    SWAP_CARD32 (reply->lengthName);
    SWAP_CARD32 (reply->subsetInfo);
}

void
SWAP_FUNC_PREFIX(ConvertGetEnumeratedTypeInfoReply) (cntxtPtr, strmPtr, reply)
pexContext			*cntxtPtr;
pexGetEnumeratedTypeInfoReq	*strmPtr;
pexGetEnumeratedTypeInfoReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i, j, pad;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* NOT 0 */

    switch (strmPtr->itemMask) {

	case PEXETIndex : {
	    CARD16 *ptr;
	    /* PEXETIndex =1 per define in PEX.h */
	    /* a LISTofLISTofINT16               */
	    int numints;
	    for (i=0, ptr = (CARD16 *)(reply+1); i < reply->numLists; i++) {
		numints = *((CARD32 *)ptr);
		pad = numints & 1;
		SWAP_CARD32(*((CARD32 *)ptr));
		ptr += 2;
		for (j=0; j < numints; j++) {
		    SWAP_CARD16((*ptr));
		    ptr++;
		}
		ptr += pad;
	    }

	  break;
	}

	case PEXETMnemonic : {
	    CARD8 *ptr;
	    /* PEXETMnemonic =2 per define in PEX.h */
	    /* a LISTofLISTofSTRING                 */
	    int numstrings, numbytes;
	    for (i=0, ptr = (CARD8 *)(reply+1); i < reply->numLists; i++) {
		numstrings = *((CARD32 *)ptr);
		SWAP_CARD32(*((CARD32 *)ptr));
		ptr += 4;
		for (j=0; j < numstrings; j++) {
		    numbytes = *((CARD16 *)ptr);
		    SWAP_CARD16(*((CARD16 *)ptr));
		    ptr += 2 + numbytes + PADDING(2+numbytes);
		}
	    }
	  break;
	}

	case PEXETBoth : {
	    CARD8 *ptr;
	    /* PEXETBOTH =3 per define in PEX.h */
	    /* a LISTofLISTofENUM_DESC 		*/
	    int numenums, numbytes;
	    for (i=0, ptr = (CARD8 *)(reply+1); i < reply->numLists; i++) {
		numenums = *((CARD32 *)ptr);
		SWAP_CARD32(*((CARD32 *)ptr));
		ptr += 4;
		for (j=0; j < numenums; j++) {
		    SWAP_CARD16((*ptr));
		    ptr += 2;
		    numbytes = *((CARD16 *)ptr);
		    SWAP_CARD16(*((CARD16 *)ptr));
		    ptr += 2 + numbytes + PADDING(numbytes);
		}
	    }
	  break;
        }

	default: { /* counts */

	    CARD16 *ptr;
		/* swap the counts */
	    for (i=0, ptr = (CARD16 *)(reply+1); i < reply->numLists; i++) {
		SWAP_CARD32(*((CARD32 *)ptr));
		ptr += 2;
	    }
	    break; 
	}

    }

    SWAP_CARD32 (reply->numLists);

}

void
SWAP_FUNC_PREFIX(ConvertGetTableInfoReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetTableInfoReq	*strmPtr;
pexGetTableInfoReply *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->definableEntries);
    SWAP_CARD16 (reply->numPredefined);
    SWAP_INT16 (reply->predefinedMin);
    SWAP_INT16 (reply->predefinedMax);
}


void
SWAP_FUNC_PREFIX(ConvertGetPredefinedEntriesReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetPredefinedEntriesReq  *strmPtr;
pexGetPredefinedEntriesReply *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapTable) (   swapPtr, strmPtr->tableType,
				    reply->numEntries,
				    (unsigned char *)(reply+1));

    SWAP_CARD32 (reply->numEntries);
}

void
SWAP_FUNC_PREFIX(ConvertGetDefinedIndicesReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetDefinedIndicesReq	    *strmPtr;
pexGetDefinedIndicesReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    CARD16 *ind;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, ind=(CARD16 *)(reply+1); i < reply->numIndices; i++, ind++)
	SWAP_CARD16((*ind));

    SWAP_CARD32 (reply->numIndices);

}

void
SWAP_FUNC_PREFIX(ConvertGetTableEntryReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetTableEntryReq	*strmPtr;
pexGetTableEntryReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->status);
    SWAP_FUNC_PREFIX(SwapTable) (   swapPtr, reply->tableType, (CARD32)1,
				    (unsigned char *)(reply+1));

    SWAP_CARD16 (reply->tableType);
}

void
SWAP_FUNC_PREFIX(ConvertGetTableEntriesReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetTableEntriesReq	*strmPtr;
pexGetTableEntriesReply *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_FUNC_PREFIX(SwapTable) (   swapPtr, reply->tableType,
				    reply->numEntries,
				    (unsigned char *)(reply+1));
    SWAP_CARD32 (reply->numEntries);
    SWAP_CARD16 (reply->tableType);
}

void
SWAP_FUNC_PREFIX(ConvertGetPipelineContextReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetPipelineContextReq    *strmPtr;
pexGetPipelineContextReply  *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapPipelineContextAttr) (	swapPtr, (strmPtr->itemMask),
						(CARD8 *)(reply+1));
}

void
SWAP_FUNC_PREFIX(ConvertGetRendererAttributesReply) (cntxtPtr, strmPtr, reply)
pexContext			*cntxtPtr;
pexGetRendererAttributesReq	*strmPtr;
pexGetRendererAttributesReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapRendererAttributes) (	swapPtr, strmPtr->itemMask,
						(CARD8 *)(reply+1));
}

void
SWAP_FUNC_PREFIX(ConvertGetRendererDynamicsReply) (cntxtPtr, strmPtr, reply)
pexContext			*cntxtPtr;
pexGetRendererDynamicsReq	*strmPtr;
pexGetRendererDynamicsReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 *ptr = (CARD32 *)(reply+1);
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD32 ((*ptr)); ptr++;	/* tables */
    SWAP_CARD32 ((*ptr)); ptr++;	/* namesets */
    SWAP_CARD32 ((*ptr));		/* attributes */
}

void
SWAP_FUNC_PREFIX(ConvertGetStructureInfoReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetStructureInfoReq	    *strmPtr;
pexGetStructureInfoReply    *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->editMode);
    SWAP_CARD32 (reply->elementPtr);
    SWAP_CARD32 (reply->numElements);
    SWAP_CARD32 (reply->lengthStructure);
    SWAP_CARD16 (reply->hasRefs);
}

void
SWAP_FUNC_PREFIX(ConvertGetElementInfoReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetElementInfoReq	*strmPtr;
pexGetElementInfoReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexElementInfo *pe;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, pe=(pexElementInfo *)(reply+1); i < reply->numInfo; i++, pe++)
	SWAP_ELEMENT_INFO (*pe);

    SWAP_CARD32 (reply->numInfo);

}

void
SWAP_FUNC_PREFIX(ConvertGetStructuresInNetworkReply) (cntxtPtr, strmPtr, reply)
pexContext			*cntxtPtr;
pexGetStructuresInNetworkReq	*strmPtr;
pexGetStructuresInNetworkReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexStructure *ps;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, ps=(pexStructure *)(reply+1); i<reply->numStructures; i++, ps++)
	SWAP_STRUCTURE ((*ps));

    SWAP_CARD32 (reply->numStructures);

}


void
SWAP_FUNC_PREFIX(ConvertGetAncestorsReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetAncestorsReq	*strmPtr;
pexGetAncestorsReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 i, j, num, *buf;
    pexElementRef *pe;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, buf=(CARD32 *)(reply+1); i<reply->numPaths; i++) {
	num = *buf;
	SWAP_CARD32((*buf));
	buf++;
	for ( j=0, pe=(pexElementRef *)(buf); j<num; j++, pe++)
	    SWAP_ELEMENT_REF((*pe));
	buf = (CARD32 *)pe;
	}

    SWAP_CARD32 (reply->numPaths);
}

/* typedef pexGetAncestorsReply pexGetDescendantsReply; */

void
SWAP_FUNC_PREFIX(ConvertElementSearchReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexElementSearchReq	*strmPtr;
pexElementSearchReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->status);
    SWAP_CARD32 (reply->foundOffset);

}

void
SWAP_FUNC_PREFIX(ConvertFetchElementsReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexFetchElementsReq	*strmPtr;
pexFetchElementsReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexElementInfo *pe;
    CARD32 *curCmd;
    int	length;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for (i=0, curCmd = (CARD32 *)(reply+1);
	 i<reply->numElements;
	 i++, curCmd += length)
    {
	pe = (pexElementInfo *)curCmd;
	/* this check must match the one done in mipex.h MI_IS_PEX_OC macro */
        if ((PEXOCAll < pe->elementType) && (pe->elementType <= PEXMaxOC))
	    cntxtPtr->pexSwapReplyOC[ pe->elementType ] (swapPtr, pe);
	length = pe->length;
	SWAP_ELEMENT_INFO (*pe);
    }

    SWAP_CARD32 (reply->numElements);
}

void
SWAP_FUNC_PREFIX(ConvertGetNameSetReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexGetNameSetReq    *strmPtr;
pexGetNameSetReply  *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexName *pn;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, pn=(pexName *)(reply+1); i<reply->numNames; i++, pn++)
	SWAP_NAME((*pn));

    SWAP_CARD32 (reply->numNames);

}

void
SWAP_FUNC_PREFIX(ConvertGetSearchContextReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetSearchContextReq	    *strmPtr;
pexGetSearchContextReply    *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapSearchContext)(swapPtr, strmPtr->itemMask,
					(unsigned char *)(reply+1));

}

void
SWAP_FUNC_PREFIX(ConvertSearchNetworkReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexSearchNetworkReq	*strmPtr;
pexSearchNetworkReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexElementRef *pe;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, pe=(pexElementRef *)(reply+1); i<reply->numItems; i++, pe++)
	SWAP_ELEMENT_REF((*pe));

    SWAP_CARD32 (reply->numItems);

}

void
SWAP_FUNC_PREFIX(ConvertGetWksInfoReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexGetWksInfoReq    *strmPtr;
pexGetWksInfoReply  *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    unsigned char *ptr;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    ptr = (unsigned char *)(reply+1);

    /*  Read the Protocol Encoding, lots of non 4 byte fields here
	that are shipped as 4 bytes (effectively CARD32) so they 
	are swapped as CARD32    - JSH
    */
    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWDisplayUpdate) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWVisualState) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWDisplaySurface) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWViewUpdate) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWDefinedViews) {
	int len, i;
	len = *(int *)ptr;			    
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr+=sizeof(CARD32);
	for (i=0; i<len; i++, ptr += sizeof(CARD32)) {
	    SWAP_CARD32 ((*((CARD32 *)ptr)));
	}
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWWksUpdate){
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWReqNpcSubvolume) {
	SwapNpcSubvolume(swapPtr, (pexNpcSubvolume *)ptr);
	ptr += sizeof(pexNpcSubvolume);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWCurNpcSubvolume) {
	SwapNpcSubvolume(swapPtr, (pexNpcSubvolume *)ptr);
	ptr += sizeof(pexNpcSubvolume);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWReqWksViewport) {
	SwapViewport(swapPtr, (pexViewport *)ptr);
	ptr += sizeof(pexViewport);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWCurWksViewport) {
	SwapViewport(swapPtr, (pexViewport *)ptr);
	ptr += sizeof(pexViewport);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWHlhsrUpdate) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWReqHlhsrMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWCurHlhsrMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWDrawable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWMarkerBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWTextBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWLineBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWInteriorBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWEdgeBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWColourTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWDepthCueTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWLightTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWColourApproxTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWPatternTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWTextFontTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWHighlightIncl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWHighlightExcl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWInvisibilityIncl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWInvisibilityExcl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWPostedStructures) {
	CARD32 len, i;
	pexStructureInfo *ps;

	len = *((CARD32 *)ptr);
	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	ps = (pexStructureInfo *)ptr;
	for (i=0; i<len; i++, ps++)
	    SWAP_STRUCT_INFO ((*ps));
	ptr = (CARD8 *)ps;
    };

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWNumPriorities) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWBufferUpdate) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);	
    }	

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWReqBufferMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);	
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPWCurBufferMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);	
    }


}

void
SWAP_FUNC_PREFIX(ConvertGetDynamicsReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetDynamicsReq	*strmPtr;
pexGetDynamicsReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* 0 */
}

void
SWAP_FUNC_PREFIX(ConvertGetViewRepReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexGetViewRepReq    *strmPtr;
pexGetViewRepReply  *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    pexViewRep *pv;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->viewUpdate);

    pv = (pexViewRep *)(reply+1);

    SwapViewRep(swapPtr, pv);		/* requested */
    pv++;
    SwapViewRep(swapPtr, pv);		/* current   */

}

void
SWAP_FUNC_PREFIX(ConvertMapDCtoWCReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexMapDCtoWCReq	    *strmPtr;
pexMapDCtoWCReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexCoord3D *pc;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD16 (reply->viewIndex);

    for ( i=0, pc=(pexCoord3D *)(reply+1); i<reply->numCoords; i++, pc++)
	SWAP_COORD3D((*pc));

    SWAP_CARD32 (reply->numCoords);

}

void
SWAP_FUNC_PREFIX(ConvertMapWCtoDCReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexMapWCtoDCReq	    *strmPtr;
pexMapWCtoDCReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexDeviceCoord *pc;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for (i=0, pc=(pexDeviceCoord *)(reply+1); i<reply->numCoords; i++,pc++)
	SWAP_DEVICE_COORD((*pc));

    SWAP_CARD32 (reply->numCoords);

}

void
SWAP_FUNC_PREFIX(ConvertGetWksPostingsReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetWksPostingsReq	*strmPtr;
pexGetWksPostingsReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i, num;
    pexPhigsWks *pi;

    SWAP_CARD16 (reply->sequenceNumber);

    num = (int)((reply->length)/sizeof(pexPhigsWks));
    SWAP_CARD32 (reply->length);	/* not 0 */

    for ( i=0, pi=(pexPhigsWks *)(reply+1); i<num; i++, pi++)
	SWAP_PHIGS_WKS((*pi));

}

void
SWAP_FUNC_PREFIX(ConvertGetPickDeviceReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetPickDeviceReq	*strmPtr;
pexGetPickDeviceReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapPickDevAttr) (	swapPtr, strmPtr->itemMask,
					(unsigned char *)(reply+1));


}

void
SWAP_FUNC_PREFIX(ConvertGetPickMeasureReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexGetPickMeasureReq	*strmPtr;
pexGetPickMeasureReply	*reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SWAP_FUNC_PREFIX(SwapPickMeasAttr) (    swapPtr, strmPtr->itemMask,
					    (unsigned char *)(reply + 1));

}

void
SWAP_FUNC_PREFIX(ConvertEndPickOneReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexEndPickOneReq	*strmPtr;
pexEndPickOneReply	*reply;
{
    pexPickElementRef *p_data;
    CARD32  i;

    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	
    SWAP_CARD16 (reply->pickStatus);

    p_data = (pexPickElementRef *)(reply+1);
    for (i=0; i < reply->numPickElRefs; i++, p_data++) 
	    SWAP_PICK_ELEMENT_REF((*p_data));
    
    SWAP_CARD32 (reply->numPickElRefs);
}

void
SWAP_FUNC_PREFIX(ConvertPickOneReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexPickOneReq		*strmPtr;
pexPickOneReply		*reply;
{
    pexPickElementRef *p_data;
    CARD32  i;

    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);
    SWAP_CARD16 (reply->pickStatus);

    p_data = (pexPickElementRef *)(reply+1);
    for (i=0; i < reply->numPickElRefs; i++, p_data++) 
	    SWAP_PICK_ELEMENT_REF((*p_data));
    
    SWAP_CARD32 (reply->numPickElRefs);
}

void
SWAP_FUNC_PREFIX(ConvertEndPickAllReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexEndPickAllReq	*strmPtr;
pexEndPickAllReply	*reply;
{
    pexPickElementRef *p_data;
    CARD32 i, j, num, *buf;

    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);
    SWAP_CARD16 (reply->pickStatus);
    SWAP_CARD16 (reply->morePicks);

    /* JSH commented out because until the Pick Functionality is
       actually written this will seg fault on a NULL pointer

    for ( i=0, buf=(CARD32 *)(reply+1); i < reply->numPicked; i++) {
	num = *buf;
	SWAP_CARD32((*buf));
	buf++;
	for ( j=0, p_data = (pexPickElementRef *)(buf); j < num; j++, p_data++)
	    SWAP_PICK_ELEMENT_REF((*p_data));
	buf = (CARD32 *)p_data;
    }

    */
    SWAP_CARD32 (reply->numPicked);
}

void
SWAP_FUNC_PREFIX(ConvertPickAllReply) (cntxtPtr, strmPtr, reply)
pexContext		*cntxtPtr;
pexPickAllReq	*strmPtr;
pexPickAllReply	*reply;
{
    pexPickElementRef *p_data;
    CARD32 i, j, num, *buf;

    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);
    SWAP_CARD16 (reply->pickStatus);
    SWAP_CARD16 (reply->morePicks);

    /* JSH commented out because until the Pick Functionality is
       actually written this will seg fault on a NULL pointer

    for ( i=0, buf=(CARD32 *)(reply+1); i < reply->numPicked; i++) {
	num = *buf;
	SWAP_CARD32((*buf));
	buf++;
	for ( j=0, p_data = (pexPickElementRef *)(buf); j < num; j++, p_data++)
	    SWAP_PICK_ELEMENT_REF((*p_data));
	buf = (CARD32 *)p_data;
    }

    */
    SWAP_CARD32 (reply->numPicked);
}


void
SWAP_FUNC_PREFIX(ConvertQueryFontReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexQueryFontReq	    *strmPtr;
pexQueryFontReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD32 (reply->lengthFontInfo);

    SWAP_FUNC_PREFIX(SwapFontInfo) (swapPtr, (pexFontInfo *)(reply+1));
}

void
SWAP_FUNC_PREFIX(ConvertListFontsReply) (cntxtPtr, strmPtr, reply)
pexContext	    *cntxtPtr;
pexListFontsReq	    *strmPtr;
pexListFontsReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_FUNC_PREFIX(SwapStringList) (	swapPtr, reply->numStrings,
					(pexString *)(reply+1));

    SWAP_CARD32 (reply->numStrings);
}

void
SWAP_FUNC_PREFIX(ConvertListFontsWithInfoReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexListFontsWithInfoReq	    *strmPtr;
pexListFontsWithInfoReply   *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 numInfo;
    CARD32 i;
    CARD8 *ptr;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    ptr = SWAP_FUNC_PREFIX(SwapStringList) (	swapPtr, reply->numStrings,
						(pexString *)(reply+1));

    SWAP_CARD32 (reply->numStrings);

    numInfo = *((CARD32 *)ptr);
    SWAP_CARD32((*((CARD32 *)ptr)));
    ptr += sizeof(CARD32);

    for (i=0; i<numInfo; i++)
	ptr = SWAP_FUNC_PREFIX(SwapFontInfo) (swapPtr, (pexFontInfo *)ptr);	
}

void
SWAP_FUNC_PREFIX(ConvertQueryTextExtentsReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexQueryTextExtentsReq	    *strmPtr;
pexQueryTextExtentsReply    *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 numInfo;

    numInfo = reply->length / 6;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    SwapExtentInfo (swapPtr, numInfo, (pexExtentInfo *) (reply + 1));
}

void
SWAP_FUNC_PREFIX(ConvertGetImpDepConstantsReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexGetImpDepConstantsReq    *strmPtr;
pexGetImpDepConstantsReply  *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD16 *pnames = (CARD16 *)(strmPtr+1);
    CARD32 i;
    CARD32 *ptr = (CARD32 *)(reply+1);
    PEXFLOAT *pf;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    for (i=0; i<strmPtr->numNames; i++, pnames++, ptr++) {
	switch (*pnames) {
	    case PEXIDDitheringSupported:
	    case PEXIDMaxEdgeWidth:
	    case PEXIDMaxLineWidth:
	    case PEXIDMaxMarkerSize:
	    case PEXIDMaxModelClipPlanes:
	    case PEXIDMaxNameSetNames:
	    case PEXIDMaxNonAmbientLights:
	    case PEXIDMaxNURBOrder:
	    case PEXIDMaxTrimCurveOrder:
	    case PEXIDMinEdgeWidth:
	    case PEXIDMinLineWidth:
	    case PEXIDMinMarkerSize:
	    case PEXIDNominalEdgeWidth:
	    case PEXIDNominalLineWidth:
	    case PEXIDNominalMarkerSize:
	    case PEXIDNumSupportedEdgeWidths:
	    case PEXIDNumSupportedLineWidths:
	    case PEXIDNumSupportedMarkerSizes:
	    case PEXIDBestColourApproximation:
	    case PEXIDTransparencySupported:
						SWAP_CARD32((*ptr));
						break;
	    case PEXIDChromaticityRedU:
	    case PEXIDChromaticityRedV:
	    case PEXIDLuminanceRed:
	    case PEXIDChromaticityGreenU:
	    case PEXIDChromaticityGreenV:
	    case PEXIDLuminanceGreen:
	    case PEXIDChromaticityBlueU:
	    case PEXIDChromaticityBlueV:
	    case PEXIDLuminanceBlue:
	    case PEXIDChromaticityWhiteU:
	    case PEXIDChromaticityWhiteV:
	    case PEXIDLuminanceWhite:
		      pf = (PEXFLOAT *)ptr;
		      SWAP_FLOAT((*pf));
		break;
	}
    }
}

void
SWAP_FUNC_PREFIX(ConvertMatchRendererTargetsReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexMatchRendererTargetsReq  *strmPtr;
pexMatchRendererTargetsReply    *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 i, numTargets;
    pexRendererTarget *rd_data;

    numTargets = reply->length / 2;
    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */

    rd_data = (pexRendererTarget *)(reply + 1);

    for ( i = 0; i < numTargets; i++, rd_data++ )
	SWAP_RENDERER_TARGET((*rd_data));

}

void
SWAP_FUNC_PREFIX(EscapeWithReplyReply) (cntxtPtr, strmPtr, reply)
pexContext		    *cntxtPtr;
pexEscapeWithReplyReq       *strmPtr;
pexEscapeWithReplyReply     *reply;
{
    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (reply->sequenceNumber);
    SWAP_CARD32 (reply->length);	/* not 0 */
    SWAP_CARD32 (reply->escapeID);

    /* vendor specific stuff goes here */
}

void
SWAP_FUNC_PREFIX(NoReply)()
{

 }


/****************************************************************
 *  		utilities					*
 ****************************************************************/

unsigned char *
SWAP_FUNC_PREFIX(SwapStringList) (swapPtr, numStrings, stringPtr)
pexSwap		*swapPtr;
CARD32	    	numStrings;
pexString   	*stringPtr;
{
    CARD32 i;
    CARD16 j;
    CARD8 *ptr = (CARD8 *)stringPtr;

    for (i=0; i<numStrings; i++) {
	SWAP_STRING ((*((pexString *)ptr)));
	j = ((pexString *)ptr)->length;
	ptr += j + j%2;
    }
    return (ptr);
}


void
SWAP_FUNC_PREFIX(SwapTable)(swapPtr, TType, num, where)
pexSwap		*swapPtr;
pexTableType	TType;
CARD32		num;
unsigned char	*where;
{
    int i;
    unsigned char *ptr = where;
    CARD32  numFontIDs;

    switch (TType) { 
	case PEXLineBundleLUT:	{
	    for (i=0; i<num; i++) 
		ptr = SWAP_FUNC_PREFIX(SwapLineBundleEntry) (swapPtr,
						(pexLineBundleEntry *)ptr);
	    break; } 

	case PEXMarkerBundleLUT:	{ 
	    for (i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapMarkerBundleEntry) (swapPtr,
						(pexMarkerBundleEntry *)ptr);
	    break; }

	case PEXTextBundleLUT:	{ 
	    for (i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapTextBundleEntry) (swapPtr,
						(pexTextBundleEntry *)ptr);
	    break; }

	case PEXInteriorBundleLUT:	{ 
	    for (i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapInteriorBundleEntry) (swapPtr,
						(pexInteriorBundleEntry *)(ptr));
	    break; }

	case PEXEdgeBundleLUT:	{ 
	    for (i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapEdgeBundleEntry) (swapPtr,
						(pexEdgeBundleEntry *)(ptr));
	    break; }

	case PEXPatternLUT:	{ 
	    pexPatternEntry *pe;
	    for (i=0; i<num; i++) {
		pe = (pexPatternEntry *)ptr;
		ptr = SWAP_FUNC_PREFIX(SwapPatternEntry) (  swapPtr, pe,
							    pe->numx, pe->numy);
		SWAP_CARD16 (pe->numx);
		SWAP_CARD16 (pe->numy);
	    };
	    break; }

	case PEXTextFontLUT:	{
	    for (i=0; i<num; i++) {
	      numFontIDs = *(CARD32 *)ptr;
	      ptr += 4;
	      for ( i=0; i<numFontIDs; i++, ptr += sizeof(pexFont)) {
		  SWAP_FONT ((*(pexFont *)ptr));
	      }
	    }
	    break; }

	case PEXColourLUT:	{ 
	    for ( i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						(pexColourSpecifier *)ptr);
	    break; }

	case PEXViewLUT:	{ 
	    pexViewEntry *pv = (pexViewEntry *)ptr;
	    for ( i=0; i<num; i++, pv++)
		SwapViewEntry (swapPtr, pv);
	    break; }

	case PEXLightLUT:	{ 
	    for ( i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapLightEntry) (swapPtr,
						    (pexLightEntry *)(ptr));
	    break; }

	case PEXDepthCueLUT:	{ 
	    for ( i=0; i<num; i++)
		ptr = SWAP_FUNC_PREFIX(SwapDepthCueEntry) (swapPtr,
						(pexDepthCueEntry *)(ptr));
	    break; }

	case PEXColourApproxLUT:	{ 
	    extern void SwapColourApproxEntry();
	    pexColourApproxEntry *pa = (pexColourApproxEntry *)ptr;
	    for ( i=0; i<num; i++, pa++)
		SwapColourApproxEntry( swapPtr, pa);
	    break; }

	}
}


void
SWAP_FUNC_PREFIX(SwapSearchContext)(swapPtr, im, ptr) 
pexSwap		*swapPtr;
CARD32		im;
unsigned char	*ptr;
{
    unsigned char *sc_data = ptr;

    if (im & PEXSCPosition) {
	SWAP_COORD3D ((*((pexCoord3D *)sc_data)));
	sc_data += sizeof(pexCoord3D);
    };

    if (im & PEXSCDistance) {
	SWAP_FLOAT ((*((PEXFLOAT *)sc_data)));
	sc_data += sizeof(PEXFLOAT);
    };

    /* next 2 are sent as CARD32 so swap them as if they are */
    if (im & PEXSCCeiling) {
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	sc_data += sizeof(CARD32);	    
    }

    if (im & PEXSCModelClipFlag) {
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	sc_data += sizeof (CARD32);	   
    }

    if (im & PEXSCStartPath) {
	int len, i;
	len = *((CARD32 *) sc_data);
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	sc_data += sizeof(CARD32);
	for (i=0; i<len; i++, sc_data += sizeof(pexElementRef)) {
	    SWAP_ELEMENT_REF ((*((pexElementRef *)sc_data)));
	}
    }

    if (im & PEXSCNormalList) {
	int len, i;
	len = *((CARD32 *) sc_data);
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	sc_data += sizeof(CARD32);
	for (i=0; i<len*2; i++, sc_data += sizeof(CARD32)) {
	    SWAP_NAMESET ((*((CARD32 *)sc_data)));
	}

    }

    if (im & PEXSCInvertedList) {
	int len, i;
	len = *((CARD32 *) sc_data);
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	sc_data += sizeof(CARD32);
	for (i=0; i<len*2; i++, sc_data += sizeof(CARD32)) {
	    SWAP_NAME ((*((CARD32 *)sc_data)));
	}

    }

}


CARD8 *
SWAP_FUNC_PREFIX(SwapFontInfo) (swapPtr, pfi)
pexSwap		*swapPtr;
pexFontInfo	*pfi;
{
    CARD8 *ptr;
    CARD32 i;
    pexFontProp *pfp;

    SWAP_CARD32 (pfi->firstGlyph);
    SWAP_CARD32 (pfi->lastGlyph);
    SWAP_CARD32 (pfi->defaultGlyph);

    pfp = (pexFontProp *)(pfi+1);
    for (i=0; i<pfi->numProps; i++)
	pfp = (pexFontProp *)(SwapFontProp(swapPtr, pfp));

    SWAP_CARD32 (pfi->numProps);

    ptr = (CARD8 *)pfp;
    return (ptr);
}


void
SWAP_FUNC_PREFIX(SwapPickMeasAttr) (swapPtr, im, p_data)
pexSwap		*swapPtr;
CARD32		im;
unsigned char	*p_data;
{
    unsigned char *ptr = p_data;

    if (im & PEXPMStatus) {
	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXPMPath) {
	CARD32 i;
	CARD32 numRefs = *((CARD32 *)ptr);
	pexPickElementRef *pp;

	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0, pp = (pexPickElementRef *)ptr; i<numRefs; i++, pp++) {
	    SWAP_PICK_ELEMENT_REF((*pp));
	}
    }
}


unsigned char *
SWAP_FUNC_PREFIX(SwapLightEntry) (swapPtr, p_data) 
pexSwap		    *swapPtr;
pexLightEntry	    *p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_ENUM_TYPE_INDEX (p_data->lightType);
    SWAP_VECTOR3D (p_data->direction);
    SWAP_COORD3D (p_data->point);
    SWAP_FLOAT (p_data->concentration);
    SWAP_FLOAT (p_data->spreadAngle);
    SWAP_FLOAT (p_data->attenuation1);
    SWAP_FLOAT (p_data->attenuation2);

    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (   swapPtr,
						    &(p_data->lightColour));

    return (ptr);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapLineBundleEntry) (swapPtr, p_data) 
pexSwap		    *swapPtr;
pexLineBundleEntry  *p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_ENUM_TYPE_INDEX (p_data->lineType); 
    SWAP_ENUM_TYPE_INDEX (p_data->polylineInterp); 
    SWAP_FLOAT (p_data->lineWidth); 
    SWAP_CURVE_APPROX (p_data->curveApprox);
    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier)(swapPtr, &(p_data->lineColour));

    return (ptr);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapMarkerBundleEntry) (swapPtr, p_data) 
pexSwap			*swapPtr;
pexMarkerBundleEntry	*p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_ENUM_TYPE_INDEX (p_data->markerType); 
    SWAP_FLOAT (p_data->markerScale); 
    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (   swapPtr,
						    &(p_data->markerColour));

    return (ptr);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapTextBundleEntry) (swapPtr, p_data) 
pexSwap		    *swapPtr;
pexTextBundleEntry  *p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_CARD16 (p_data->textFontIndex); 
    SWAP_CARD16 (p_data->textPrecision); 
    SWAP_FLOAT (p_data->charExpansion); 
    SWAP_FLOAT (p_data->charSpacing); 
    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier)(swapPtr, &(p_data->textColour));

    return (ptr);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapInteriorBundleEntry) (swapPtr, p_data) 
pexSwap			*swapPtr;
pexInteriorBundleEntry	*p_data;
{
    unsigned char *po;
    SWAP_ENUM_TYPE_INDEX (p_data->interiorStyle); 
    SWAP_INT16 (p_data->interiorStyleIndex);
    SWAP_ENUM_TYPE_INDEX (p_data->reflectionModel); 
    SWAP_ENUM_TYPE_INDEX (p_data->surfaceInterp); 
    SWAP_ENUM_TYPE_INDEX (p_data->bfInteriorStyle); 
    SWAP_INT16 (p_data->bfInteriorStyleIndex);
    SWAP_ENUM_TYPE_INDEX (p_data->bfReflectionModel); 
    SWAP_ENUM_TYPE_INDEX (p_data->bfSurfaceInterp); 

    SwapSurfaceApprox (swapPtr, &(p_data->surfaceApprox));
    po = (unsigned char *)(p_data+1);
    po = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						(pexColourSpecifier *)po);
    po = SWAP_FUNC_PREFIX(SwapReflectionAttr) (	swapPtr,
						(pexReflectionAttr *)po);
    po = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						(pexColourSpecifier *)po);
    po = SWAP_FUNC_PREFIX(SwapReflectionAttr) (	swapPtr,
						(pexReflectionAttr *)po); 

    return (po);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapEdgeBundleEntry) (swapPtr, p_data) 
pexSwap		    *swapPtr;
pexEdgeBundleEntry  *p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_ENUM_TYPE_INDEX (p_data->edgeType); 
    SWAP_FLOAT (p_data->edgeWidth);
    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier)(swapPtr, &(p_data->edgeColour));

    return (ptr);
}


unsigned char *
SWAP_FUNC_PREFIX(SwapDepthCueEntry) (swapPtr, p_data) 
pexSwap		    *swapPtr;
pexDepthCueEntry    *p_data;
{
    unsigned char *ptr = (unsigned char *)p_data;
    SWAP_FLOAT (p_data->frontPlane);
    SWAP_FLOAT (p_data->backPlane);
    SWAP_FLOAT (p_data->frontScaling);
    SWAP_FLOAT (p_data->backScaling);
    ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (   swapPtr,
						    &(p_data->depthCueColour));

    return (ptr);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapPatternEntry) (swapPtr, p_data, numx, numy) 
pexSwap		    *swapPtr;
pexPatternEntry	    *p_data;
CARD16		    numx;
CARD16		    numy;
{
    int i, max_colours;
    pexColour *pc = (pexColour *)(p_data + 1);

    max_colours = numx * numy;
    for (i=0; i<max_colours; i++) 
	pc = (pexColour *) SwapColour (swapPtr, pc, p_data->colourType);

    SWAP_COLOUR_TYPE (p_data->colourType);  

    return ((unsigned char *)pc);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapPipelineContextAttr) (swapPtr, itemMask, p_data)
pexSwap	*swapPtr;
CARD32 	*itemMask;
CARD8 	*p_data;
{
    /* NOTE: See the Protocol Encoding for a desription of these fields
       in places where CARD16 or INT16 are packed into a 4 byte field
       (essentially a CARD32) for transmission these fields must be
       byte swapped as a CARD32. - JSH
    */

    CARD8 *ptr = p_data;

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerType) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerScale) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerBundleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextFont) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextPrecision) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharExpansion) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharSpacing) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharHeight) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharUpVector) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextPath) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextAlignment) {
	SWAP_CARD16 ((*((CARD16 *)ptr)));
	ptr += sizeof(CARD16);
	SWAP_CARD16 ((*((CARD16 *)ptr)));
	ptr += sizeof(CARD16);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextHeight) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextUpVector) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextPath) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextAlignment) {
	SWAP_CARD16 ((*((CARD16 *)ptr)));
	ptr += sizeof(CARD16);
	SWAP_CARD16 ((*((CARD16 *)ptr)));
	ptr += sizeof(CARD16);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextStyle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextBundleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineType) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineWidth) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCurveApproximation) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPolylineInterp) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineBundleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorStyle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorStyleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceReflAttr) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceReflModel) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceInterp) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfInteriorStyle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfInteriorStyleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceReflAttr) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceReflModel) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceInterp) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceApproximation) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCullingMode) {
	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCDistinguishFlag) {
	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternSize) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefPt) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefVec1) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefVec2) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorBundleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeFlag) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeType) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeWidth) {
	SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	ptr += sizeof(PEXFLOAT);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						     (pexColourSpecifier *)ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCEdgeBundleIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLocalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++) {
		SWAP_FLOAT((*((PEXFLOAT *)ptr)));
		ptr += sizeof(PEXFLOAT);
	    }
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCGlobalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++) {
		SWAP_FLOAT((*((PEXFLOAT *)ptr)));
		ptr += sizeof(PEXFLOAT);
	    }
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCModelClip) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCModelClipVolume) {
	CARD32 i, numHalfSpace;
	pexHalfSpace *ph;
	numHalfSpace = *((CARD32 *)ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0, ph = (pexHalfSpace *)ptr; i<numHalfSpace; i++, ph++) {
	    SwapHalfSpace (swapPtr, ph);
	}
	ptr = (CARD8 *)ph;
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCViewIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLightState) {
	CARD32 i, numLights;
	numLights = *((CARD32 *)ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0; i<numLights; i++) {
	    SWAP_CARD16 ((*((CARD16 *)ptr)));
	    ptr += sizeof(CARD16);
	}
	if (numLights%2)       /* pad odd length list to CARD32 boundary  */
	    ptr += sizeof(CARD16);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCDepthCueIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSetAsfValues) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));     /* enables BITMASK */
	ptr += sizeof(CARD32);
	SWAP_CARD32 ((*((CARD32 *)ptr)));     /* asfs themselves */
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPickId) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCHlhsrIdentifier) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCNameSet) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCColourApproxIndex) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCRenderingColourModel) {
	SWAP_CARD32((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCParaSurfCharacteristics) {
	INT16 type;
	type = *((INT16 *)ptr);
	SWAP_INT16((*((INT16 *)ptr)));
	ptr += sizeof(CARD16);
	SWAP_CARD16((*((CARD16 *)ptr)));
	ptr += sizeof(CARD16);
	switch (type) {
	    case PEXPSCNone:
	    case PEXPSCImpDep: break;

	    case PEXPSCIsoCurves: {
		SWAP_CARD16((*((CARD16 *)ptr)));
		ptr += sizeof(CARD16);
		SWAP_CARD16((*((CARD16 *)ptr)));
		ptr += sizeof(CARD16);
		SWAP_CARD16((*((CARD16 *)ptr)));
		ptr += sizeof(CARD16);
		SWAP_CARD16((*((CARD16 *)ptr)));
		ptr += sizeof(CARD16);
		break;
	    }

	    case PEXPSCMcLevelCurves:
	    case PEXPSCWcLevelCurves: {
		CARD16 i, num;
		SWAP_COORD3D((*((pexCoord3D *)ptr)));
		ptr += sizeof(pexCoord3D);
		SWAP_VECTOR3D((*((pexVector3D *)ptr)));
		ptr += sizeof(pexVector3D);
		num = *((CARD16 *)ptr);
		SWAP_CARD16((*((CARD16 *)ptr)));
		ptr += sizeof(CARD32);
		for (i=0; i<num; i++) {
		    SWAP_FLOAT((*((PEXFLOAT *)ptr)));
		    ptr += sizeof(PEXFLOAT);
		}
	    }
	}
    }


    return (ptr);
}

void
SWAP_FUNC_PREFIX(SwapPickDevAttr) (swapPtr, im, pdata)
pexSwap		*swapPtr;
CARD32		im;
unsigned char	*pdata;
{
    unsigned char *ptr = pdata;
    int len, i;

    if (im & PEXPDPickStatus) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    if (im & PEXPDPickPath) {
	len = (int)(*ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0; i<len; i++, ptr += sizeof(pexPickElementRef)) {
	    SWAP_PICK_ELEMENT_REF ((*((pexPickElementRef *)ptr)));
	};
    };

    if (im & PEXPDPickPathOrder) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    if (im & PEXPDPickIncl) {
	SWAP_NAMESET ((*((pexNameSet *)ptr)));
	ptr += sizeof(pexNameSet);
    };

    if (im & PEXPDPickExcl) {
	SWAP_NAMESET ((*((pexNameSet *)ptr)));
	ptr += sizeof(pexNameSet);
    };

    if (im & PEXPDPickDataRec) {
	len = (int)(*ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	ptr += len + PADDING(len);		    /*	pad it out  */
    };

    if (im & PEXPDPickPromptEchoType) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    };

    if (im & PEXPDPickEchoVolume) {
	SwapViewport(swapPtr, (pexViewport *)ptr);
	ptr += sizeof(pexViewport);	
    }

    if (im & PEXPDPickEchoSwitch) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
    }
}


void
SWAP_FUNC_PREFIX(SwapRendererAttributes) (swapPtr, im, p_data)
pexSwap	*swapPtr;
CARD32	im;
CARD8	*p_data;
{
    CARD8 *ptr = p_data;
    CARD32 num, i;

    if (im & PEXRDPipelineContext) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDCurrentPath) {
	num = *((CARD32 *)ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0; i<num; i++, ptr += sizeof(pexElementRef))
	    SWAP_ELEMENT_REF((*((pexElementRef *)ptr)));
    }

    if (im & PEXRDMarkerBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDTextBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDLineBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDInteriorBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDEdgeBundle) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDViewTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDColourTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDDepthCueTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDLightTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDColourApproxTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDPatternTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDTextFontTable) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDHighlightIncl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDHighlightExcl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDInvisibilityIncl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDInvisibilityExcl) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

  /* Swap this and HLHSR as CARD32 since they're in a 4 byte field in
     the LISTofVALUE                                                  */

    if (im & PEXRDRendererState) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDHlhsrMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	
    }

    if (im & PEXRDNpcSubvolume) {
	SwapNpcSubvolume (swapPtr, (pexNpcSubvolume *)ptr);
	ptr += sizeof(pexNpcSubvolume);
    }

    if (im & PEXRDViewport) {
	SwapViewport (swapPtr, (pexViewport *)ptr);
	ptr += sizeof(pexViewport);
    }

    if (im & PEXRDClipList) {
	num = *((CARD32 *)ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	SwapDeviceRects (swapPtr, num, (pexDeviceRect *)ptr);
        ptr += sizeof(pexDeviceRect) * num;
    }

    if (im & PEXRDPickInclusion) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDPickExclusion) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDPickStartPath) {
	num = *((CARD32 *)ptr);
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
	for (i=0; i<num; i++, ptr += sizeof(pexElementRef))
	    SWAP_ELEMENT_REF((*((pexElementRef *)ptr)));
    }

    if (im & PEXRDBackgroundColour) {
	ptr = SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
						   (pexColourSpecifier *)ptr);
    }

    /* this is CARD8 cast into a CARD32 so it must get swapped as CARD32 */
    if (im & PEXRDClearI) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    /* this is CARD8 cast into a CARD32 so it must get swapped as CARD32 */
    if (im & PEXRDClearZ) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    /* this is CARD16 cast into a CARD32 so it must get swapped as CARD32 */
    if (im & PEXRDEchoMode) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }
}
