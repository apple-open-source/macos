/* $Xorg: convReq.c,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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
#include "pexError.h"
#include "pexSwap.h"
#include "pex_site.h"
#include "ddpex.h"
#include "pexLookup.h"
#include "convertStr.h"

#undef LOCAL_FLAG
#define LOCAL_FLAG extern
#include "convUtil.h"
#include "ConvName.h"
#include "OCprim.h"
#include "OCcolour.h"
#include "OCattr.h"

#undef LOCAL_FLAG
#define LOCAL_FLAG
#include "convReq.h"

extern RequestFunction PEXRequest[];

#define PADDING(n) ( (n)&3 ? (4 - ((n)&3)) : 0)


/****************************************************************
 *  		REQUESTS 					*
 ****************************************************************/

#define CALL_REQUEST	return(PEXRequest[strmPtr->opcode](cntxtPtr, strmPtr))


ErrorCode
SWAP_FUNC_PREFIX(PEXGenericRequest) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexReq		*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
				/* length in 4 bytes quantities */
                                /* of whole request, including this header */
    CALL_REQUEST;
}

/*****************************************************************
 *  structures that follow request.
 *****************************************************************/

/* ResourceReq is used for any request which has a resource ID
   (or Atom or Time) as its one and only argument.  */

ErrorCode
SWAP_FUNC_PREFIX(PEXGenericResourceRequest) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexResourceReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD32 (strmPtr->id);  /* a Structure, Renderer, Font, Pixmap, etc. */

    CALL_REQUEST;
}


/*****************************************************************
 *  Specific Requests 
 *****************************************************************/


ErrorCode
SWAP_FUNC_PREFIX(PEXGetExtensionInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetExtensionInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD16 (strmPtr->clientProtocolMajor);
    SWAP_CARD16 (strmPtr->clientProtocolMinor);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetEnumeratedTypeInfo) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetEnumeratedTypeInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD32 i;
    CARD16 *ptr = (CARD16 *)(strmPtr+1);

    SWAP_CARD16 (strmPtr->length);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_BITMASK (strmPtr->itemMask);
    SWAP_CARD32 (strmPtr->numEnums);

    for (i=0; i<strmPtr->numEnums; i++, ptr++)
	SWAP_CARD16 ((*ptr));

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXCreateLookupTable) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCreateLookupTableReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_DRAWABLE (strmPtr->drawableExample);
    SWAP_LOOKUP_TABLE (strmPtr->lut);
    SWAP_TABLE_TYPE (strmPtr->tableType);
    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXCopyLookupTable) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCopyLookupTableReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_LOOKUP_TABLE (strmPtr->src);
    SWAP_LOOKUP_TABLE (strmPtr->dst);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreeLookupTableReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXGetTableInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetTableInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_DRAWABLE (strmPtr->drawableExample);
    SWAP_TABLE_TYPE (strmPtr->tableType);
    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXGetPredefinedEntries) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetPredefinedEntriesReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_DRAWABLE (strmPtr->drawableExample);
    SWAP_TABLE_TYPE (strmPtr->tableType);
    SWAP_TABLE_INDEX (strmPtr->start);
    SWAP_CARD16 (strmPtr->count);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexGetDefinedIndicesReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXGetTableEntry) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetTableEntryReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD16 (strmPtr->valueType);
    SWAP_LOOKUP_TABLE (strmPtr->lut);
    SWAP_TABLE_INDEX (strmPtr->index);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetTableEntries) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetTableEntriesReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD16 (strmPtr->valueType);
    SWAP_LOOKUP_TABLE (strmPtr->lut);
    SWAP_TABLE_INDEX (strmPtr->start);
    SWAP_CARD16 (strmPtr->count);
    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXSetTableEntries) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetTableEntriesReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    ErrorCode err = Success;
    pexTableType ttype;
    diLUTHandle lut;
    CARD32 num;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_LOOKUP_TABLE (strmPtr->lut);
    SWAP_TABLE_INDEX (strmPtr->start);
    SWAP_CARD16 (strmPtr->count);

    LU_TABLE(strmPtr->lut, lut);
    ttype = lut->lutType;
    num = (CARD32)(strmPtr->count);

    SWAP_FUNC_PREFIX(SwapTable) (   swapPtr, ttype, num,
				    (unsigned char *)(strmPtr+1));

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXDeleteTableEntries) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexDeleteTableEntriesReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_LOOKUP_TABLE (strmPtr->lut);
    SWAP_TABLE_INDEX (strmPtr->start);
    SWAP_CARD16 (strmPtr->count);
    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXCreatePipelineContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexCreatePipelineContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PC (strmPtr->pc);
    SWAP_CARD32 (strmPtr->itemMask[0]);
    SWAP_CARD32 (strmPtr->itemMask[1]);

    SWAP_FUNC_PREFIX(SwapPipelineContextAttr) (	swapPtr,
						(CARD32 *)(strmPtr->itemMask),
						(CARD8 *)(strmPtr + 1));

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXCopyPipelineContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexCopyPipelineContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PC (strmPtr->src);
    SWAP_PC (strmPtr->dst);
    SWAP_CARD32 (strmPtr->itemMask[0]);
    SWAP_CARD32 (strmPtr->itemMask[1]);
    CALL_REQUEST;
}

/* typedef pexResourceReq  pexFreePipelineContextReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXGetPipelineContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetPipelineContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PC (strmPtr->pc);
    SWAP_CARD32 (strmPtr->itemMask[0]);
    SWAP_CARD32 (strmPtr->itemMask[1]);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXChangePipelineContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexChangePipelineContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PC (strmPtr->pc);
    SWAP_CARD32 (strmPtr->itemMask[0]);
    SWAP_CARD32 (strmPtr->itemMask[1]);

    SWAP_FUNC_PREFIX(SwapPipelineContextAttr) (	swapPtr,
						(CARD32 *)(strmPtr->itemMask),
						(CARD8 *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXCreateRenderer) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCreateRendererReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_BITMASK (strmPtr->itemMask);

    SWAP_FUNC_PREFIX(SwapRendererAttributes) (	swapPtr, strmPtr->itemMask,
						(CARD8 *)(strmPtr+1));

    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreeRendererReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXChangeRenderer) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexChangeRendererReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_BITMASK (strmPtr->itemMask);

    SWAP_FUNC_PREFIX(SwapRendererAttributes) (	swapPtr, strmPtr->itemMask,
						(CARD8 *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetRendererAttributes) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetRendererAttributesReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_BITMASK (strmPtr->itemMask);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexGetRendererDynamics; */

ErrorCode
SWAP_FUNC_PREFIX(PEXBeginRendering) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexBeginRenderingReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexEndRenderingReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXBeginStructure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexBeginStructureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_CARD32 (strmPtr->sid);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexEndStructureReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXRenderOutputCommands) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexRenderOutputCommandsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_CARD32 (strmPtr->numCommands);
    SwapListOfOutputCommands (cntxtPtr, strmPtr->numCommands,
	(CARD32 *) (strmPtr + 1));
    CALL_REQUEST;
}
/* individual output commands may be found in the section "Output Commands" */


ErrorCode
SWAP_FUNC_PREFIX(PEXRenderElements) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexRenderElementsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_STRUCTURE (strmPtr->sid);
    SwapElementRange (swapPtr, &strmPtr->range);

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXAccumulateState) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexAccumulateStateReq	*strmPtr;
{
    pexElementRef	*pe;
    CARD32              i;

    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_CARD32 (strmPtr->numElRefs);

    pe = (pexElementRef *)(strmPtr+1);
    for (i = 0; i < strmPtr->numElRefs; i++, pe++)
	SWAP_ELEMENT_REF (*pe);

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXRenderNetwork) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexRenderNetworkReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_STRUCTURE (strmPtr->sid);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexCreateStructureReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXCopyStructure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCopyStructureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->src);
    SWAP_STRUCTURE (strmPtr->dst);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXDestroyStructures) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexDestroyStructuresReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexStructure *ps;

    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD32 (strmPtr->numStructures);

    for (i=0, ps=(pexStructure *)(strmPtr+1); i<strmPtr->numStructures; i++,ps++)
	SWAP_STRUCTURE ((*ps));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetStructureInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetStructureInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD16 (strmPtr->itemMask);
    SWAP_STRUCTURE (strmPtr->sid);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetElementInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetElementInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_STRUCTURE (strmPtr->sid);

    SwapElementRange (swapPtr, &strmPtr->range);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetStructuresInNetwork) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetStructuresInNetworkReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD16 (strmPtr->which);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetAncestors) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetAncestorsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD16 (strmPtr->pathOrder);
    SWAP_CARD32 (strmPtr->pathDepth);
    CALL_REQUEST;
}

/* typedef pexGetAncestorsReq pexGetDescendantsReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXFetchElements) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexFetchElementsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_STRUCTURE (strmPtr->sid);

    SwapElementRange (swapPtr, &strmPtr->range);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetEditingMode) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetEditingModeReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD16 (strmPtr->mode);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetElementPointer) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetElementPointerReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_ELEMENT_POS (strmPtr->position);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetElementPointerAtLabel) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexSetElementPointerAtLabelReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD32 (strmPtr->label);
    SWAP_INT32 (strmPtr->offset);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXElementSearch) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexElementSearchReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    CARD16 *pc;

    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_ELEMENT_POS (strmPtr->position);
    SWAP_CARD32 (strmPtr->direction);
    SWAP_CARD32 (strmPtr->numIncls);
    SWAP_CARD32 (strmPtr->numExcls);

    pc = (CARD16 *)(strmPtr+1);
    for (i=0; i< strmPtr->numIncls; i++, pc++)
	SWAP_CARD16((*pc));

    /* skip pad if there */
    if (strmPtr->numIncls & 0x1) pc++;

    for (i=0; i< strmPtr->numExcls; i++, pc++)
	SWAP_CARD16((*pc));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXStoreElements) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexStoreElementsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD32 (strmPtr->numCommands);
    SwapListOfOutputCommands (cntxtPtr, strmPtr->numCommands,
	(CARD32 *) (strmPtr + 1));
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXDeleteElements) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexDeleteElementsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);

    SwapElementRange (swapPtr, &strmPtr->range);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXDeleteElementsToLabel) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexDeleteElementsToLabelReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_ELEMENT_POS (strmPtr->position);
    SWAP_CARD32 (strmPtr->label);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXDeleteBetweenLabels) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexDeleteBetweenLabelsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_CARD32 (strmPtr->label1);
    SWAP_CARD32 (strmPtr->label2);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXCopyElements) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCopyElementsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->src);

    SwapElementRange (swapPtr, &strmPtr->srcRange);

    SWAP_STRUCTURE (strmPtr->dst);
    SWAP_ELEMENT_POS (strmPtr->dstPosition);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXChangeStructureRefs) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexChangeStructureRefsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_STRUCTURE (strmPtr->old_id);
    SWAP_STRUCTURE (strmPtr->new_id);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexCreateNameSetReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXCopyNameSet) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCopyNameSetReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_NAMESET (strmPtr->src);
    SWAP_NAMESET (strmPtr->dst);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreeNameSetReq;*/

/* typedef pexResourceReq pexGetNameSetReq; */

ErrorCode
SWAP_FUNC_PREFIX(PEXChangeNameSet) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexChangeNameSetReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i, num;
    pexName *pn;

    SWAP_CARD16 (strmPtr->length);
    SWAP_NAMESET (strmPtr->ns);
    SWAP_CARD16 (strmPtr->action);

    num = (int)(strmPtr->length - (sizeof(pexChangeNameSetReq)/sizeof(CARD32)));
    for (i=0, pn=(pexName *)(strmPtr+1); i<num; i++, pn++)
	SWAP_NAME ((*pn));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXCreateSearchContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexCreateSearchContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_SC (strmPtr->sc);
    SWAP_BITMASK (strmPtr->itemMask);

    SWAP_FUNC_PREFIX(SwapSearchContext) (   swapPtr, strmPtr->itemMask,
					    (unsigned char *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXCopySearchContext) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCopySearchContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_SC (strmPtr->src);
    SWAP_SC (strmPtr->dst);
    SWAP_BITMASK (strmPtr->itemMask);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreeSearchContextReq;*/

ErrorCode
SWAP_FUNC_PREFIX(PEXGetSearchContext) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetSearchContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_SC (strmPtr->sc);
    SWAP_BITMASK (strmPtr->itemMask);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXChangeSearchContext) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexChangeSearchContextReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_SC (strmPtr->sc);
    SWAP_BITMASK (strmPtr->itemMask);

    SWAP_FUNC_PREFIX(SwapSearchContext) (   swapPtr, strmPtr->itemMask,
					    (unsigned char *)(strmPtr+1));

    CALL_REQUEST;
}

/* typedef pexResourceReq pexSearchNetworkReq;*/

ErrorCode
SWAP_FUNC_PREFIX(PEXCreatePhigsWks) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCreatePhigsWksReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_LOOKUP_TABLE (strmPtr->markerBundle);
    SWAP_LOOKUP_TABLE (strmPtr->textBundle);
    SWAP_LOOKUP_TABLE (strmPtr->lineBundle);
    SWAP_LOOKUP_TABLE (strmPtr->interiorBundle);
    SWAP_LOOKUP_TABLE (strmPtr->edgeBundle);
    SWAP_LOOKUP_TABLE (strmPtr->colourTable);
    SWAP_LOOKUP_TABLE (strmPtr->depthCueTable);
    SWAP_LOOKUP_TABLE (strmPtr->lightTable);
    SWAP_LOOKUP_TABLE (strmPtr->colourApproxTable);
    SWAP_LOOKUP_TABLE (strmPtr->patternTable);
    SWAP_LOOKUP_TABLE (strmPtr->textFontTable);
    SWAP_NAMESET (strmPtr->highlightIncl);
    SWAP_NAMESET (strmPtr->highlightExcl);
    SWAP_NAMESET (strmPtr->invisIncl);
    SWAP_NAMESET (strmPtr->invisExcl);
    SWAP_CARD16 (strmPtr->bufferMode);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreePhigsWksReq;*/

ErrorCode
SWAP_FUNC_PREFIX(PEXGetWksInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetWksInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_BITMASK (strmPtr->itemMask[0]);
    SWAP_BITMASK (strmPtr->itemMask[1]);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetDynamics) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetDynamicsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_DRAWABLE (strmPtr->drawable);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetViewRep) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetViewRepReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_TABLE_INDEX (strmPtr->index);
    SWAP_PHIGS_WKS (strmPtr->wks);
    CALL_REQUEST;
}

/*
typedef pexResourceReq pexRedrawAllStructuresReq;	

typedef pexResourceReq pexUpdateWorkstationReq;
*/

ErrorCode
SWAP_FUNC_PREFIX(PEXRedrawClipRegion) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexRedrawClipRegionReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_CARD32 (strmPtr->numRects);

    SwapDeviceRects (swapPtr, strmPtr->numRects, (pexDeviceRect *)(strmPtr+1));
    CALL_REQUEST;
}

/*
typedef pexResourceReq pexExecuteDeferredActionsReq;
*/

ErrorCode
SWAP_FUNC_PREFIX(PEXSetViewPriority) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetViewPriorityReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_TABLE_INDEX (strmPtr->index1);
    SWAP_TABLE_INDEX (strmPtr->index2);
    SWAP_CARD16 (strmPtr->priority);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetDisplayUpdateMode) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexSetDisplayUpdateModeReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_ENUM_TYPE_INDEX (strmPtr->displayUpdate);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXMapDCtoWC) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexMapDCtoWCReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexDeviceCoord *pc;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_CARD32 (strmPtr->numCoords);

    for (i=0, pc=(pexDeviceCoord *)(strmPtr+1); i<strmPtr->numCoords; i++,pc++)
	SWAP_DEVICE_COORD((*pc));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXMapWCtoDC) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexMapWCtoDCReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    int i;
    pexCoord3D *pc;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD16 (strmPtr->index);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_CARD32 (strmPtr->numCoords);

    for ( i=0, pc=(pexCoord3D *)(strmPtr+1); i<strmPtr->numCoords; i++, pc++)
	SWAP_COORD3D((*pc));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetViewRep) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetViewRepReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);

    SwapViewRep (swapPtr, &strmPtr->viewRep);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetWksWindow) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetWksWindowReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SwapNpcSubvolume (swapPtr, &strmPtr->npcSubvolume);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetWksViewport) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetWksViewportReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);

    SwapViewport (swapPtr, &strmPtr->viewport);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetHlhsrMode) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetHlhsrModeReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_ENUM_TYPE_INDEX (strmPtr->mode);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXSetWksBufferMode) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexSetWksBufferModeReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_CARD16 (strmPtr->bufferMode);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXPostStructure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexPostStructureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_STRUCTURE (strmPtr->sid);
    SWAP_FLOAT (strmPtr->priority);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXUnpostStructure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexUnpostStructureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_STRUCTURE (strmPtr->sid);
    CALL_REQUEST;
}

/*
typedef pexResourceReq pexUnpostAllStructuresReq;

typedef pexResourceReq pexGetWksPostingsReq;
*/

ErrorCode
SWAP_FUNC_PREFIX(PEXGetPickDevice) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetPickDeviceReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_ENUM_TYPE_INDEX (strmPtr->devType);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_BITMASK (strmPtr->itemMask);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXChangePickDevice) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexChangePickDeviceReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_ENUM_TYPE_INDEX (strmPtr->devType);
    SWAP_BITMASK (strmPtr->itemMask);

    SWAP_FUNC_PREFIX(SwapPickDevAttr) (	swapPtr, strmPtr->itemMask,
					(unsigned char *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXCreatePickMeasure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexCreatePickMeasureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PHIGS_WKS (strmPtr->wks);
    SWAP_PICK_MEASURE (strmPtr->pm);
    SWAP_ENUM_TYPE_INDEX (strmPtr->devType);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexFreePickMeasureReq;*/

ErrorCode
SWAP_FUNC_PREFIX(PEXGetPickMeasure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexGetPickMeasureReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PICK_MEASURE (strmPtr->pm);
    SWAP_BITMASK (strmPtr->itemMask);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXUpdatePickMeasure) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexUpdatePickMeasureReq	*strmPtr;
{

    extern void SwapFLOAT();

    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_PICK_MEASURE (strmPtr->pm);
    SWAP_CARD32 (strmPtr->numBytes);
    /* SWAP the input data record for the registered devices */
    if (strmPtr->numBytes == 8) {
      unsigned char *ptr = (unsigned char *)(strmPtr+1);
      SWAP_CARD16 ((*((CARD16 *)ptr)));
      ptr += sizeof(CARD16);
      SWAP_CARD16 ((*((CARD16 *)ptr)));
      ptr += sizeof(CARD16);
      SWAP_FLOAT ((*((PEXFLOAT *)ptr)));

    } else if (strmPtr->numBytes == 24) {
      unsigned char *ptr = (unsigned char *)(strmPtr+1);
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
      SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
      ptr += sizeof(PEXFLOAT);
    } else
      return(BadLength); 
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXBeginPickOne) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexBeginPickOneReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_ENUM_TYPE_INDEX (strmPtr->method);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_INT32 (strmPtr->sid);

    SWAP_FUNC_PREFIX(SwapPickRecord) (swapPtr, 
				    (pexPickRecord *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXEndPickOne) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexEndPickOneReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXPickOne) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexPickOneReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_ENUM_TYPE_INDEX (strmPtr->method);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_STRUCTURE (strmPtr->sid);

    SWAP_FUNC_PREFIX(SwapPickRecord) (swapPtr, 
				    (pexPickRecord *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXBeginPickAll) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexBeginPickAllReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_ENUM_TYPE_INDEX (strmPtr->method);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_INT32 (strmPtr->sid);
    SWAP_CARD32 (strmPtr->pickMaxHits);

    SWAP_FUNC_PREFIX(SwapPickRecord) (swapPtr, 
				    (pexPickRecord *)(strmPtr+1));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXEndPickAll) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexEndPickAllReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_RENDERER (strmPtr->rdr);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXPickAll) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexPickAllReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_ENUM_TYPE_INDEX (strmPtr->method);
    SWAP_RENDERER (strmPtr->rdr);
    SWAP_DRAWABLE (strmPtr->drawable);
    SWAP_CARD32 (strmPtr->pickMaxHits);

    SWAP_FUNC_PREFIX(SwapPickRecord) (swapPtr, 
				    (pexPickRecord *)(strmPtr+1));

    CALL_REQUEST;
}


ErrorCode
SWAP_FUNC_PREFIX(PEXOpenFont) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexOpenFontReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_FONT (strmPtr->font);
    SWAP_CARD32 (strmPtr->numBytes);
    CALL_REQUEST;
}

/* typedef pexResourceReq pexCloseFontReq;*/

ErrorCode
SWAP_FUNC_PREFIX(PEXQueryFont) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexQueryFontReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_FONT (strmPtr->font);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXListFonts) (cntxtPtr, strmPtr)
pexContext	*cntxtPtr;
pexListFontsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD16 (strmPtr->maxNames);
    SWAP_CARD16 (strmPtr->numChars);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXListFontsWithInfo) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexListFontsWithInfoReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD16 (strmPtr->maxNames);
    SWAP_CARD16 (strmPtr->numChars);
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXQueryTextExtents) (cntxtPtr, strmPtr)
pexContext		*cntxtPtr;
pexQueryTextExtentsReq	*strmPtr;
{
    pexSwap		*swapPtr = cntxtPtr->swap;
    pexMonoEncoding	*pEnc;
    CARD32		*numEnc;
    int			bytes, i;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD16 (strmPtr->textPath);
    SWAP_CARD16 (strmPtr->fontGroupIndex);
    SWAP_CARD32 (strmPtr->id);
    SWAP_FLOAT (strmPtr->charExpansion);
    SWAP_FLOAT (strmPtr->charSpacing);
    SWAP_FLOAT (strmPtr->charHeight);

    SwapTextAlignmentData(swapPtr, &strmPtr->textAlignment);
    
    SWAP_CARD32 (strmPtr->numStrings);

    numEnc = (CARD32 *) (strmPtr + 1);

    for (i = 0; i < strmPtr->numStrings; i++)
    {
	SWAP_CARD32 (*numEnc);

	pEnc = (pexMonoEncoding *) (numEnc + 1);
	SWAP_FUNC_PREFIX(SwapMonoEncoding) (swapPtr, pEnc, *numEnc);

	bytes = pEnc->numChars * ((pEnc->characterSetWidth == PEXCSByte) ?
	  sizeof(CARD8) : ((pEnc->characterSetWidth == PEXCSShort) ?
	  sizeof(CARD16) : sizeof(CARD32)));

	numEnc = (CARD32 *) ((char *) (pEnc + 1) + bytes + PADDING (bytes));
    }

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXGetImpDepConstants) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexGetImpDepConstantsReq	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;
    CARD16 *ptr = (CARD16 *)(strmPtr+1);
    CARD32 i;

    SWAP_CARD16 (strmPtr->length);
    SWAP_ENUM_TYPE_INDEX (strmPtr->fpFormat);
    SWAP_CARD32 (strmPtr->drawable);
    SWAP_CARD32 (strmPtr->numNames);

    for (i=0; i<strmPtr->numNames; i++, ptr++)
	SWAP_CARD16 ((*ptr));

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXMatchRendererTargets) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexMatchRendererTargetsReq     	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD32 (strmPtr->drawable);
    SWAP_CARD16 (strmPtr->type);
    SWAP_CARD32 (strmPtr->visualID);
    SWAP_CARD32 (strmPtr->maxTriplets);

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXEscape) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexEscapeReq               	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD32 (strmPtr->escapeID);

    /* do MIT Registered Escapes */
    switch (strmPtr->escapeID) {
      case  PEXEscapeSetEchoColour: {
	pexEscapeSetEchoColourData *psec;

	psec = (pexEscapeSetEchoColourData *)(strmPtr+1);
	SWAP_ENUM_TYPE_INDEX (psec->fpFormat);
	SWAP_CARD32 (psec->rdr);
	SWAP_FUNC_PREFIX(SwapColourSpecifier) (swapPtr,
					      (pexColourSpecifier *)(psec+1));
	break;
      }
    }
	  
    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PREFIX(PEXEscapeWithReply) (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexEscapeWithReplyReq          	*strmPtr;
{
    pexSwap *swapPtr = cntxtPtr->swap;

    SWAP_CARD16 (strmPtr->length);
    SWAP_CARD16 (strmPtr->escapeID);

    /* do MIT Registered Escapes , none with Replies */
    /*
    switch (strmPtr->escapeID) {
    }
    */
	  

    CALL_REQUEST;
}

ErrorCode
SWAP_FUNC_PEX_PFX(RequestUnused)()
{

}


/****************************************************************
 *  		utilities					*
 ****************************************************************/
void
SWAP_FUNC_PREFIX(SwapTable)(swapPtr, TType, num, where)
pexSwap		    *swapPtr;
pexTableType	    TType;
CARD32		    num;
unsigned char	    *where;
{
    int i;
    unsigned char *ptr = where;

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
		SWAP_CARD16 (pe->numx);
		SWAP_CARD16 (pe->numy);
		ptr = SWAP_FUNC_PREFIX(SwapPatternEntry) (  swapPtr, pe,
							    pe->numx, pe->numy);
	    };
	    break; }

	case PEXTextFontLUT:	{
	    for ( i=0; i<num; i++, ptr += sizeof(pexFont)) {
		SWAP_FONT ((*(pexFont *)ptr));
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

    if (im & PEXSCStartPath) 
    {
	int		len, i;
	pexElementRef	*pe;
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	len = *((CARD32 *) sc_data);
	sc_data += sizeof(CARD32);
	for (i=0, pe = (pexElementRef *) sc_data; i<len; i++, pe++)
	{
	    SWAP_ELEMENT_REF (*pe);
	}
	sc_data = (unsigned char *) pe;
    }

    if (im & PEXSCNormalList) 
    {
	int	len, i;
	CARD32	*ns;
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	len = *((CARD32 *) sc_data);
	sc_data += sizeof (CARD32);
	for (i=0, ns = (CARD32 *) sc_data; i < len; i++, ns += 2)
	{
	    SWAP_NAMESET (ns[0]);
	    SWAP_NAMESET (ns[1]);
	}
	sc_data = (unsigned char *) ns;
    }

    if (im & PEXSCInvertedList) 
    {
	int	len, i;
	CARD32	*ns;
	SWAP_CARD32 ((*((CARD32 *)sc_data)));
	len = *((CARD32 *) sc_data);
	sc_data += sizeof (CARD32);
	for (i=0, ns = (CARD32 *) sc_data; i < len; i++, ns += 2)
	{
	    SWAP_NAMESET (ns[0]);
	    SWAP_NAMESET (ns[1]);
	}
	sc_data = (unsigned char *) ns;
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
    SWAP_CARD32 (pfi->numProps);

    pfp = (pexFontProp *)(pfi+1);
    for (i=0; i<pfi->numProps; i++)
	pfp = (pexFontProp *)(SwapFontProp(swapPtr, pfp));

    ptr = (CARD8 *)pfp;
    return (ptr);
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

    SWAP_COLOUR_TYPE (p_data->colourType);  

    max_colours = numx * numy;
    for (i=0; i<max_colours; i++) 
	pc = (pexColour *) SwapColour (swapPtr, pc, p_data->colourType);


    return ((unsigned char *)pc);
}

unsigned char *
SWAP_FUNC_PREFIX(SwapPipelineContextAttr) (swapPtr, itemMask, p_data)
pexSwap		    *swapPtr;
CARD32 *itemMask;
CARD8 *p_data;
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCDistinguishFlag) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	numHalfSpace = *((CARD32 *)ptr);
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	numLights = *((CARD32 *)ptr);
	ptr += sizeof(CARD32);
	for (i=0; i<numLights; i++) {
	    SWAP_CARD16 ((*((CARD16 *)ptr)));
	    ptr += sizeof(CARD16);
	}
	if (numLights%2)          /* pad odd length list to CARD32 boundary  */
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCParaSurfCharacteristics) {
	INT16 type;
	SWAP_INT16((*((INT16 *)ptr)));
	type = *((INT16 *)ptr);
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
		SWAP_CARD16((*((CARD16 *)ptr)));
		num = *((CARD16 *)ptr);
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	len = (int)(*ptr);
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	len = (int)(*ptr);
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
pexSwap	  *swapPtr;
CARD32	    im;
CARD8	    *p_data;
{
    CARD8 *ptr = p_data;
    CARD32 num, i;

    if (im & PEXRDPipelineContext) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	ptr += sizeof(CARD32);
    }

    if (im & PEXRDCurrentPath) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	num = *((CARD32 *)ptr);
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
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	num = *((CARD32 *)ptr);
	ptr += sizeof(CARD32);
	SwapDeviceRects (swapPtr, num, (pexDeviceRect *)ptr);
	ptr += sizeof(pexDeviceRect) * num;
    }

    if (im & PEXRDPickInclusion) {
	SWAP_NAMESET ((*((pexNameSet *)ptr)));
	ptr += sizeof(pexNameSet);
    }

    if (im & PEXRDPickExclusion) {
	SWAP_NAMESET ((*((pexNameSet *)ptr)));
	ptr += sizeof(pexNameSet);
    }

    if (im & PEXRDPickStartPath) {
	SWAP_CARD32 ((*((CARD32 *)ptr)));
	num = *((CARD32 *)ptr);
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

void
SWAP_FUNC_PREFIX(SwapPickRecord) (swapPtr, p_data)
pexSwap	  		    *swapPtr;
pexPickRecord	            *p_data;
{

    SWAP_CARD16 (p_data->pickType);

    switch(p_data->pickType) {
	case PEXPickDeviceDC_HitBox: {
	  unsigned char *ptr = (unsigned char *)(p_data+1);
	  SWAP_CARD16 ((*((CARD16 *)ptr)));
	  ptr += sizeof(CARD16);
	  SWAP_CARD16 ((*((CARD16 *)ptr)));
	  ptr += sizeof(CARD16);
	  SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	  break;
	} 
	case PEXPickDeviceNPC_HitVolume: {
	  unsigned char *ptr = (unsigned char *)(p_data+1);
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
	  SWAP_FLOAT ((*((PEXFLOAT *)ptr)));
	  break;
	} 
    } 
}

