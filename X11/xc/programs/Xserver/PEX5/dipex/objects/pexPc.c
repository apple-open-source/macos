/* $Xorg: pexPc.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexPc.c,v 3.7 2001/12/14 19:57:43 dawes Exp $ */


/*++
 *		PEXCreatePipelineContext
 *		PEXCopyPipelineContext
 *		PEXFreePipelineContext
 *		PEXGetPipelineContext
 *		PEXChangePipelineContext
 --*/

#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "dipex.h"
#include "ddpex.h"
#include "pexLookup.h"
#include "pexExtract.h"
#include "pexUtils.h"
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif

/* need to do this to return correct ASF_ENABLES bits per Encoding */
#define ASF_ALL   0x3FFFFFFF

#define CHK_PEX_BUF(SIZE,INCR,REPLY,TYPE,PTR) {\
    (SIZE)+=(INCR); \
      if (pPEXBuffer->bufSize < (SIZE)) { \
	ErrorCode err = Success; \
	int offset = (int)(((unsigned char *)(PTR)) - ((unsigned char *)(pPEXBuffer->pHead))); \
	err = puBuffRealloc(pPEXBuffer,(ddULONG)(SIZE)); \
	if (err) PEX_ERR_EXIT(err,0,cntxtPtr); \
	(REPLY) = (TYPE *)(pPEXBuffer->pHead); \
	(PTR) = (unsigned char *)(pPEXBuffer->pHead + offset); } \
}

#define PADDING(n) ( (n)&3 ? (4 - ((n)&3)) : 0)

ErrorCode
UpdatePCRefs (pc, pr, action)
ddPCStr *pc;
ddRendererStr *pr;
ddAction action;
{ 
    if (action == ADD) {
	if (puAddToList((ddPointer) &pr, (unsigned long)1, pc->rendRefs)
	    == BadAlloc)
	return (BadAlloc);
    } else
	puRemoveFromList((ddPointer) &pr, pc->rendRefs);

    return(Success);
}

static ErrorCode
UpdatePipelineContext (cntxtPtr, pca, itemMask, ptr)
pexContext	*cntxtPtr;
ddPCAttr	*pca;
CARD32		itemMask[3];
unsigned char	*ptr;
{
    ErrorCode err = Success;

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerType) {
	EXTRACT_INT16_FROM_4B (pca->markerType, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerScale) {
	EXTRACT_FLOAT (pca->markerScale, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->markerColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCMarkerBundleIndex) {
	EXTRACT_CARD16_FROM_4B (pca->markerIndex, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextFont) {
	EXTRACT_CARD16_FROM_4B (pca->textFont, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextPrecision) {
	EXTRACT_CARD16_FROM_4B (pca->textPrecision, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharExpansion) {
	EXTRACT_FLOAT (pca->charExpansion, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharSpacing) {
	EXTRACT_FLOAT (pca->charSpacing, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->textColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharHeight) {
	EXTRACT_FLOAT (pca->charHeight, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCharUpVector) {
	EXTRACT_FLOAT (pca->charUp.x, ptr);
	EXTRACT_FLOAT (pca->charUp.y, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextPath) {
	EXTRACT_CARD16_FROM_4B (pca->textPath, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextAlignment) {
	EXTRACT_CARD16 (pca->textAlignment.vertical, ptr);
	EXTRACT_CARD16 (pca->textAlignment.horizontal, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextHeight) {
	EXTRACT_FLOAT (pca->atextHeight, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextUpVector) {
	EXTRACT_FLOAT (pca->atextUp.x, ptr);
	EXTRACT_FLOAT (pca->atextUp.y, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextPath) {
	EXTRACT_CARD16_FROM_4B (pca->atextPath, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextAlignment) {
	EXTRACT_CARD16 (pca->atextAlignment.vertical, ptr);
	EXTRACT_CARD16 (pca->atextAlignment.horizontal, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCAtextStyle) {
	EXTRACT_INT16_FROM_4B (pca->atextStyle, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCTextBundleIndex) {
	EXTRACT_CARD16_FROM_4B (pca->textIndex, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineType) {
	EXTRACT_INT16_FROM_4B (pca->lineType, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineWidth) {
	EXTRACT_FLOAT (pca->lineWidth, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->lineColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCurveApproximation) {
	EXTRACT_INT16_FROM_4B (pca->curveApprox.approxMethod, ptr);
	EXTRACT_FLOAT (pca->curveApprox.tolerance, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPolylineInterp) {
	EXTRACT_INT16_FROM_4B (pca->lineInterp,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLineBundleIndex) {
	EXTRACT_CARD16_FROM_4B (pca->lineIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorStyle) {
	EXTRACT_INT16_FROM_4B (pca->intStyle,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorStyleIndex) {
	EXTRACT_INT16_FROM_4B (pca->intStyleIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->surfaceColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceReflAttr) {
	EXTRACT_FLOAT (pca->reflAttr.ambient, ptr);
	EXTRACT_FLOAT (pca->reflAttr.diffuse, ptr);
	EXTRACT_FLOAT (pca->reflAttr.specular, ptr);
	EXTRACT_FLOAT (pca->reflAttr.specularConc, ptr);
	EXTRACT_FLOAT (pca->reflAttr.transmission, ptr);
	EXTRACT_COLOUR_SPECIFIER (pca->reflAttr.specularColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceReflModel) {
	EXTRACT_INT16_FROM_4B (pca->reflModel,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceInterp) {
	EXTRACT_INT16_FROM_4B (pca->surfInterp,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfInteriorStyle) {
	EXTRACT_INT16_FROM_4B (pca->bfIntStyle,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfInteriorStyleIndex) {
	EXTRACT_INT16_FROM_4B (pca->bfIntStyleIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->bfSurfColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceReflAttr) {
	EXTRACT_FLOAT (pca->bfReflAttr.ambient, ptr);
	EXTRACT_FLOAT (pca->bfReflAttr.diffuse, ptr);
	EXTRACT_FLOAT (pca->bfReflAttr.specular, ptr);
	EXTRACT_FLOAT (pca->bfReflAttr.specularConc, ptr);
	EXTRACT_FLOAT (pca->bfReflAttr.transmission, ptr);
	EXTRACT_COLOUR_SPECIFIER (pca->bfReflAttr.specularColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceReflModel) {
	EXTRACT_INT16_FROM_4B (pca->bfReflModel,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCBfSurfaceInterp) {
	EXTRACT_INT16_FROM_4B (pca->bfSurfInterp,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceApproximation) {
	EXTRACT_INT16_FROM_4B (pca->surfApprox.approxMethod,ptr);
	EXTRACT_FLOAT (pca->surfApprox.uTolerance, ptr);
	EXTRACT_FLOAT (pca->surfApprox.vTolerance, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCCullingMode) {
         EXTRACT_CARD16_FROM_4B (pca->cullMode,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCDistinguishFlag) {
	EXTRACT_CARD8_FROM_4B (pca->distFlag,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternSize) {
	EXTRACT_FLOAT (pca->patternSize.x, ptr);
	EXTRACT_FLOAT (pca->patternSize.y, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefPt) {
	EXTRACT_FLOAT (pca->patternRefPt.x, ptr);
	EXTRACT_FLOAT (pca->patternRefPt.y, ptr);
	EXTRACT_FLOAT (pca->patternRefPt.z, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefVec1) {
	EXTRACT_FLOAT (pca->patternRefV1.x, ptr);
	EXTRACT_FLOAT (pca->patternRefV1.y, ptr);
	EXTRACT_FLOAT (pca->patternRefV1.z, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPatternRefVec2) {
	EXTRACT_FLOAT (pca->patternRefV2.x, ptr);
	EXTRACT_FLOAT (pca->patternRefV2.y, ptr);
	EXTRACT_FLOAT (pca->patternRefV2.z, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCInteriorBundleIndex) {
	EXTRACT_CARD16_FROM_4B (pca->intIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeFlag) {
	EXTRACT_CARD16_FROM_4B (pca->edges,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeType) {
	EXTRACT_INT16_FROM_4B (pca->edgeType,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeWidth) {
	EXTRACT_FLOAT (pca->edgeWidth,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSurfaceEdgeColour) {
	EXTRACT_COLOUR_SPECIFIER (pca->edgeColour, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCEdgeBundleIndex) {
	EXTRACT_CARD16_FROM_4B (pca->edgeIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLocalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		EXTRACT_FLOAT(pca->localMat[i][j], ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCGlobalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		EXTRACT_FLOAT(pca->globalMat[i][j], ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCModelClip) {
	EXTRACT_CARD16_FROM_4B (pca->modelClip,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCModelClipVolume) {
	unsigned long i, numHalfSpaces;
	EXTRACT_CARD32 ( numHalfSpaces, ptr);
	PU_EMPTY_LIST(pca->modelClipVolume);
      /* don't need to do this emptying the list and adding is sufficient
	puDeleteList(pca->modelClipVolume);
	pca->modelClipVolume = puCreateList(DD_HALF_SPACE);
	if (!pca->modelClipVolume) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
      */
	puAddToList((ddPointer)ptr, numHalfSpaces, pca->modelClipVolume);
	/* skip past list of Half Space */
	for ( i = 0; i < numHalfSpaces; i++) {
	  SKIP_STRUCT(ptr, 1, ddCoord3D);
	  SKIP_STRUCT(ptr, 1, ddVector3D);
	}
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCViewIndex) {
	EXTRACT_CARD16_FROM_4B (pca->viewIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCLightState) {
	unsigned long i, numskip;
	EXTRACT_CARD32(i,ptr);
	PU_EMPTY_LIST(pca->lightState);
	puAddToList((ddPointer)ptr,i,pca->lightState);
	/* skip over CARD16 and pad if any */
	numskip = (i + 1) / 2;
	SKIP_PADDING( ptr, numskip * 4);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCDepthCueIndex) {
	EXTRACT_CARD16_FROM_4B (pca->depthCueIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCSetAsfValues) {
	CARD32  asf_enables, asfs;
	EXTRACT_CARD32 (asf_enables,ptr);
	EXTRACT_CARD32 (asfs,ptr);
	pca->asfs = (pca->asfs & ~asf_enables) | (asfs & asf_enables);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCPickId) {
	EXTRACT_CARD32 (pca->pickId,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCHlhsrIdentifier) {
	EXTRACT_CARD32 (pca->hlhsrType,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCNameSet) {
	pexNameSet ns;
	EXTRACT_CARD32 (ns, ptr);
	LU_NAMESET(ns, pca->pCurrentNS);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCColourApproxIndex) {
	EXTRACT_CARD16_FROM_4B (pca->colourApproxIndex,ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCRenderingColourModel) {
	EXTRACT_INT16_FROM_4B (pca->rdrColourModel, ptr);
    }

    CHECK_BITMASK_ARRAY(itemMask, PEXPCParaSurfCharacteristics) {
	EXTRACT_INT16 (pca->psc.type, ptr);
	SKIP_PADDING(ptr,2);
	switch (pca->psc.type) {
	    case PEXPSCNone:
	    case PEXPSCImpDep:
		break;
	    case PEXPSCIsoCurves: {
		EXTRACT_CARD16(pca->psc.data.isoCurves.placementType, ptr);
		SKIP_PADDING(ptr,2);
		EXTRACT_CARD16(pca->psc.data.isoCurves.numUcurves, ptr);
		EXTRACT_CARD16(pca->psc.data.isoCurves.numVcurves, ptr);
		break;
	    }
	    case PEXPSCMcLevelCurves: {
		EXTRACT_COORD3D((&(pca->psc.data.mcLevelCurves.origin)),ptr);
		EXTRACT_COORD3D((&(pca->psc.data.mcLevelCurves.direction)),ptr);
		EXTRACT_CARD16(pca->psc.data.mcLevelCurves.numberIntersections,ptr);
		SKIP_PADDING(ptr,2);
		pca->psc.data.mcLevelCurves.pPoints = (ddFLOAT *)
		    xalloc((unsigned long) (sizeof(ddFLOAT) *
			    pca->psc.data.mcLevelCurves.numberIntersections));
		EXTRACT_STRUCT(	pca->psc.data.mcLevelCurves.numberIntersections,
				PEXFLOAT, pca->psc.data.mcLevelCurves.pPoints,
				ptr);
		break;
	    }

	    case PEXPSCWcLevelCurves: {
		EXTRACT_COORD3D(&(pca->psc.data.wcLevelCurves.origin),ptr);
		EXTRACT_COORD3D(&(pca->psc.data.wcLevelCurves.direction),ptr);
		EXTRACT_CARD16(pca->psc.data.wcLevelCurves.numberIntersections,ptr);
		SKIP_PADDING(ptr,2);
		pca->psc.data.wcLevelCurves.pPoints = (ddFLOAT *)
		    xalloc((unsigned long) (sizeof(ddFLOAT) *
			    pca->psc.data.wcLevelCurves.numberIntersections));
		EXTRACT_STRUCT(	pca->psc.data.wcLevelCurves.numberIntersections,
				PEXFLOAT, pca->psc.data.wcLevelCurves.pPoints,
				ptr);
		break;
	    }

	}
    }

    return (Success);

}


static
void CopyColourSpecifier (src, dst)
ddColourSpecifier *src, *dst;
{
	dst->colourType = src->colourType;

	switch (dst->colourType) {
	    case PEXIndexedColour: {
		dst->colour.indexed.index = src->colour.indexed.index;
		break;
	    }

	    case PEXRgbFloatColour: {
		dst->colour.rgbFloat.red = src->colour.rgbFloat.red;
		dst->colour.rgbFloat.green = src->colour.rgbFloat.green;
		dst->colour.rgbFloat.blue = src->colour.rgbFloat.blue;
		break;
	    }

	    case PEXCieFloatColour: {
		dst->colour.cieFloat.x = src->colour.cieFloat.x;
		dst->colour.cieFloat.y = src->colour.cieFloat.y;
		dst->colour.cieFloat.z = src->colour.cieFloat.z;
		break;
	    }

	    case PEXHsvFloatColour: {
		dst->colour.hsvFloat.hue = src->colour.hsvFloat.hue;
		dst->colour.hsvFloat.saturation
					= src->colour.hsvFloat.saturation;
		dst->colour.hsvFloat.value = src->colour.hsvFloat.value;
		break;
	    }

	    case PEXHlsFloatColour: {
		dst->colour.hlsFloat.hue = src->colour.hlsFloat.hue;
		dst->colour.hlsFloat.lightness = src->colour.hlsFloat.lightness;
		dst->colour.hlsFloat.saturation
					    = src->colour.hlsFloat.saturation;
		break;
	    }

	    case PEXRgb8Colour: {
		dst->colour.rgb8.red = src->colour.rgb8.red;
		dst->colour.rgb8.green = src->colour.rgb8.green;
		dst->colour.rgb8.blue = src->colour.rgb8.blue;
		break;
	    }

	    case PEXRgb16Colour: {
		dst->colour.rgb16.red = src->colour.rgb16.red;
		dst->colour.rgb16.green = src->colour.rgb16.green;
		dst->colour.rgb16.blue = src->colour.rgb16.blue;
		break;
	    }
	}
}


/*++	PEXCreatePipelineContext
 --*/
ErrorCode
PEXCreatePipelineContext (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexCreatePipelineContextReq	*strmPtr;
{
    ErrorCode err = Success;
    ErrorCode freePipelineContext();
    ddPCStr *pc;
    extern	void	DefaultPC();

    if (!LegalNewID(strmPtr->pc, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->pc,cntxtPtr);

    pc = (ddPCStr *)xalloc((unsigned long)(sizeof(ddPCStr) + sizeof(ddPCAttr)));
    if (!pc) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

    pc->PCid = strmPtr->pc;
    pc->rendRefs = puCreateList(DD_RENDERER);
    if (!pc->rendRefs) {
	xfree((pointer)pc);
	PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
    }
    pc->pPCAttr = (ddPCAttr *)(pc+1);
    DefaultPC(pc->pPCAttr);
    if (!pc->pPCAttr->modelClipVolume || !pc->pPCAttr->lightState) {
	puDeleteList(pc->rendRefs);
	xfree((pointer)pc);
	PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
    }

    err = UpdatePipelineContext(    cntxtPtr, pc->pPCAttr, strmPtr->itemMask,
				    (unsigned char *)(strmPtr + 1));
    if (err) {
	puDeleteList(pc->rendRefs);
	puDeleteList(pc->pPCAttr->modelClipVolume);
	puDeleteList(pc->pPCAttr->lightState);
	xfree((pointer)pc);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    ADDRESOURCE (strmPtr->pc, PEXPipeType, pc);
    return (err);

} /* end-PEXCreatePipelineContext() */

/*++	PEXCopyPipelineContext
 --*/
ErrorCode
PEXCopyPipelineContext (cntxtPtr, strmPtr)
pexContext              	*cntxtPtr;
pexCopyPipelineContextReq 	*strmPtr;
{
    ErrorCode err = Success;
    ddPCStr *src, *dst;

    LU_PIPELINECONTEXT(strmPtr->src, src);
    LU_PIPELINECONTEXT(strmPtr->dst, dst);

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerType) {
	dst->pPCAttr->markerType = src->pPCAttr->markerType;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerScale) {
	dst->pPCAttr->markerScale = src->pPCAttr->markerScale;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerColour) {
	CopyColourSpecifier(	&(src->pPCAttr->markerColour),
				&(dst->pPCAttr->markerColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerBundleIndex) {
	dst->pPCAttr->markerIndex = src->pPCAttr->markerIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextFont) {
	dst->pPCAttr->textFont = src->pPCAttr->textFont;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextPrecision) {
	dst->pPCAttr->textPrecision = src->pPCAttr->textPrecision;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharExpansion) {
	dst->pPCAttr->charExpansion = src->pPCAttr->charExpansion;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharSpacing) {
	dst->pPCAttr->charSpacing = src->pPCAttr->charSpacing;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextColour) {
	CopyColourSpecifier(	&(src->pPCAttr->textColour),
				&(dst->pPCAttr->textColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharHeight) {
	dst->pPCAttr->charHeight = src->pPCAttr->charHeight;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharUpVector) {
	dst->pPCAttr->charUp.x = src->pPCAttr->charUp.x;
	dst->pPCAttr->charUp.y = src->pPCAttr->charUp.y;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextPath) {
	dst->pPCAttr->textPath = src->pPCAttr->textPath;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextAlignment) {
	dst->pPCAttr->textAlignment.vertical
					= src->pPCAttr->textAlignment.vertical;
	dst->pPCAttr->textAlignment.horizontal
					= src->pPCAttr->textAlignment.horizontal;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextHeight) {
	dst->pPCAttr->atextHeight = src->pPCAttr->atextHeight;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextUpVector) {
	dst->pPCAttr->atextUp.x = src->pPCAttr->atextUp.x;
	dst->pPCAttr->atextUp.y = src->pPCAttr->atextUp.y;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextPath) {
	dst->pPCAttr->atextPath = src->pPCAttr->atextPath;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextAlignment) {
	dst->pPCAttr->atextAlignment.vertical
				    = src->pPCAttr->atextAlignment.vertical;
	dst->pPCAttr->atextAlignment.horizontal
				    = src->pPCAttr->atextAlignment.horizontal;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextStyle) {
	dst->pPCAttr->atextStyle = src->pPCAttr->atextStyle;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextBundleIndex) {
	dst->pPCAttr->textIndex = src->pPCAttr->textIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineType) {
	dst->pPCAttr->lineType = src->pPCAttr->lineType;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineWidth) {
	dst->pPCAttr->lineWidth = src->pPCAttr->lineWidth;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineColour) {
	CopyColourSpecifier(	&(src->pPCAttr->lineColour),
				&(dst->pPCAttr->lineColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCurveApproximation) {
	dst->pPCAttr->curveApprox.approxMethod
				    = src->pPCAttr->curveApprox.approxMethod;
	dst->pPCAttr->curveApprox.tolerance
				    = src->pPCAttr->curveApprox.tolerance;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPolylineInterp) {
	dst->pPCAttr->lineInterp = src->pPCAttr->lineInterp;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineBundleIndex) {
	dst->pPCAttr->lineIndex = src->pPCAttr->lineIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorStyle) {
	dst->pPCAttr->intStyle = src->pPCAttr->intStyle;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorStyleIndex) {
	dst->pPCAttr->intStyleIndex = src->pPCAttr->intStyleIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceColour) {
	CopyColourSpecifier(	&(src->pPCAttr->surfaceColour),
				&(dst->pPCAttr->surfaceColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceReflAttr) {
	dst->pPCAttr->reflAttr.ambient = src->pPCAttr->reflAttr.ambient;
	dst->pPCAttr->reflAttr.diffuse = src->pPCAttr->reflAttr.diffuse;
	dst->pPCAttr->reflAttr.specular = src->pPCAttr->reflAttr.specular;
	dst->pPCAttr->reflAttr.specularConc
					= src->pPCAttr->reflAttr.specularConc;
	dst->pPCAttr->reflAttr.transmission
					= src->pPCAttr->reflAttr.transmission;
	CopyColourSpecifier(	&(src->pPCAttr->reflAttr.specularColour),
				&(dst->pPCAttr->reflAttr.specularColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceReflModel) {
	dst->pPCAttr->reflModel = src->pPCAttr->reflModel;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceInterp) {
	dst->pPCAttr->surfInterp = src->pPCAttr->surfInterp;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfInteriorStyle) {
	dst->pPCAttr->bfIntStyle = src->pPCAttr->bfIntStyle;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfInteriorStyleIndex) {
	dst->pPCAttr->bfIntStyleIndex = src->pPCAttr->bfIntStyleIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceColour) {
	CopyColourSpecifier(	&(src->pPCAttr->bfSurfColour),
				&(dst->pPCAttr->bfSurfColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceReflAttr) {
	dst->pPCAttr->bfReflAttr.ambient = src->pPCAttr->bfReflAttr.ambient;
	dst->pPCAttr->bfReflAttr.diffuse = src->pPCAttr->bfReflAttr.diffuse;
	dst->pPCAttr->bfReflAttr.specular = src->pPCAttr->bfReflAttr.specular;
	dst->pPCAttr->bfReflAttr.specularConc
					= src->pPCAttr->bfReflAttr.specularConc;
	dst->pPCAttr->bfReflAttr.transmission
					= src->pPCAttr->bfReflAttr.transmission;
	CopyColourSpecifier(	&(src->pPCAttr->bfReflAttr.specularColour),
				&(dst->pPCAttr->bfReflAttr.specularColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceReflModel) {
	dst->pPCAttr->bfReflModel = src->pPCAttr->bfReflModel;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceInterp) {
	dst->pPCAttr->bfSurfInterp = src->pPCAttr->bfSurfInterp;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceApproximation) {
	dst->pPCAttr->surfApprox.approxMethod
					= src->pPCAttr->surfApprox.approxMethod;
	dst->pPCAttr->surfApprox.uTolerance
					= src->pPCAttr->surfApprox.uTolerance;
	dst->pPCAttr->surfApprox.vTolerance
					= src->pPCAttr->surfApprox.vTolerance;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCullingMode) {
        dst->pPCAttr->cullMode = src->pPCAttr->cullMode;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCDistinguishFlag) {
	dst->pPCAttr->distFlag = src->pPCAttr->distFlag;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternSize) {
	dst->pPCAttr->patternSize.x = src->pPCAttr->patternSize.x;
	dst->pPCAttr->patternSize.y = src->pPCAttr->patternSize.y;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefPt) {
	dst->pPCAttr->patternRefPt.x = src->pPCAttr->patternRefPt.x;
	dst->pPCAttr->patternRefPt.y = src->pPCAttr->patternRefPt.y;
	dst->pPCAttr->patternRefPt.z = src->pPCAttr->patternRefPt.z;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefVec1) {
	dst->pPCAttr->patternRefV1.x = src->pPCAttr->patternRefV1.x;
	dst->pPCAttr->patternRefV1.y = src->pPCAttr->patternRefV1.y;
	dst->pPCAttr->patternRefV1.z = src->pPCAttr->patternRefV1.z;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefVec2) {
	dst->pPCAttr->patternRefV2.x = src->pPCAttr->patternRefV2.x;
	dst->pPCAttr->patternRefV2.y = src->pPCAttr->patternRefV2.y;
	dst->pPCAttr->patternRefV2.z = src->pPCAttr->patternRefV2.z;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorBundleIndex) {
	dst->pPCAttr->intIndex = src->pPCAttr->intIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeFlag) {
	dst->pPCAttr->edges = src->pPCAttr->edges;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeType) {
	dst->pPCAttr->edgeType = src->pPCAttr->edgeType;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeWidth) {
	dst->pPCAttr->edgeWidth = src->pPCAttr->edgeWidth;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeColour) {
	CopyColourSpecifier(	&(src->pPCAttr->edgeColour),
				&(dst->pPCAttr->edgeColour));
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCEdgeBundleIndex) {
	dst->pPCAttr->edgeIndex = src->pPCAttr->edgeIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLocalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		dst->pPCAttr->localMat[i][j] = src->pPCAttr->localMat[i][j];
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCGlobalTransform) {
	int i, j;
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		dst->pPCAttr->globalMat[i][j] = src->pPCAttr->globalMat[i][j];
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCModelClip) {
	dst->pPCAttr->modelClip = src->pPCAttr->modelClip;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCModelClipVolume) {
	PU_EMPTY_LIST(dst->pPCAttr->modelClipVolume);
/*	puDeleteList(dst->pPCAttr->modelClipVolume);
	dst->pPCAttr->modelClipVolume = puCreateList(DD_HALF_SPACE);
	if (!dst->pPCAttr->modelClipVolume) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
*/	puCopyList( src->pPCAttr->modelClipVolume,
		    dst->pPCAttr->modelClipVolume);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCViewIndex) {
	dst->pPCAttr->viewIndex = src->pPCAttr->viewIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLightState) {
/*	dst->pPCAttr->lightState = src->pPCAttr->lightState;
	puDeleteList(dst->pPCAttr->lightState);
	dst->pPCAttr->lightState = puCreateList(DD_INDEX);
	if (!dst->pPCAttr->lightState) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
*/	PU_EMPTY_LIST(dst->pPCAttr->lightState);
	puCopyList( src->pPCAttr->lightState, dst->pPCAttr->lightState);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCDepthCueIndex) {
	dst->pPCAttr->depthCueIndex = src->pPCAttr->depthCueIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSetAsfValues) {
	dst->pPCAttr->asfs = src->pPCAttr->asfs;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPickId) {
	dst->pPCAttr->pickId = src->pPCAttr->pickId;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCHlhsrIdentifier) {
	dst->pPCAttr->hlhsrType = src->pPCAttr->hlhsrType;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCNameSet) {
	dst->pPCAttr->pCurrentNS = src->pPCAttr->pCurrentNS;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCColourApproxIndex) {
	dst->pPCAttr->colourApproxIndex = src->pPCAttr->colourApproxIndex;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCRenderingColourModel) {
	dst->pPCAttr->rdrColourModel = src->pPCAttr->rdrColourModel;
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCParaSurfCharacteristics) {
	dst->pPCAttr->psc = src->pPCAttr->psc;
    }

    return (Success);

} /* end-PEXCopyPipelineContext() */

ErrorCode
FreePipelineContext (pc, id)
ddPCStr *pc;
pexPC id;
{
    ErrorCode err = Success;
    extern ErrorCode UpdateRendRefs();
    int i;
    ddRendererStr *pr = (ddRendererStr *)(pc->rendRefs->pList);

    if (!pc) return (Success);

    for (i=0; i<pc->rendRefs->numObj; i++, pr++)
	UpdateRendRefs(	pr, pc->PCid, (unsigned long)PIPELINE_CONTEXT_RESOURCE,
			(unsigned long)REMOVE);

    puDeleteList(pc->rendRefs);
    puDeleteList(pc->pPCAttr->modelClipVolume);
    puDeleteList(pc->pPCAttr->lightState);
    xfree((pointer)pc);

    return (err);
}

/*++	PEXFreePipelineContext
 --*/
ErrorCode
PEXFreePipelineContext (cntxtPtr, strmPtr)
pexContext			*cntxtPtr;
pexFreePipelineContextReq	*strmPtr;
{
    ErrorCode err = Success;
    ddPCStr *pc;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXPipelineContextError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_PIPELINECONTEXT(strmPtr->id, pc);

    FreeResource (strmPtr->id, RT_NONE);

    return( err );

} /* end-PEXFreePipelineContext() */

/*++	PEXGetPipelineContext
 --*/
ErrorCode
PEXGetPipelineContext( cntxtPtr, strmPtr )
pexContext   	 		*cntxtPtr;
pexGetPipelineContextReq 	*strmPtr;
{
    /* NOTE: See the Protocol Encoding for exact details of the fields
       returned by this command. The encoding requires CARD16 and INT16
       to be sent in 4 byte fields (CARD32) for most fields of these 
       types, hence the use of PACK_CARD32 for these fields. - JSH
    */

    ErrorCode err = Success;
    ddPCStr *pc;
    ddPCAttr *pca;
    extern ddBufferPtr pPEXBuffer;
    pexGetPipelineContextReply *reply
			    = (pexGetPipelineContextReply *)(pPEXBuffer->pHead);
    CARD8 *replyPtr = (CARD8 *)(reply);
    int size = 0;
    int sze = 0;

    LU_PIPELINECONTEXT(strmPtr->pc, pc);

    pca = pc->pPCAttr;

    replyPtr += sizeof(pexGetPipelineContextReply);
    CHK_PEX_BUF(size, sizeof(pexGetPipelineContextReply), reply,
		pexGetPipelineContextReply, replyPtr);
    SETUP_INQ(pexGetPipelineContextReply);
    replyPtr = pPEXBuffer->pBuf;

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerType) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->markerType, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerScale) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->markerScale, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->markerColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->markerColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCMarkerBundleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->markerIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextFont) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->textFont, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextPrecision) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->textPrecision, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharExpansion) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->charExpansion, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharSpacing) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->charSpacing, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->textColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->textColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharHeight) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->charHeight, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCharUpVector) {
	CHK_PEX_BUF(size, 2 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->charUp.x, replyPtr);
	PACK_FLOAT ( pca->charUp.y, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextPath) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->textPath, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextAlignment) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD16 ( pca->textAlignment.vertical, replyPtr);
	PACK_CARD16 ( pca->textAlignment.horizontal, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextHeight) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->atextHeight, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextUpVector) {
	CHK_PEX_BUF(size, 2 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->atextUp.x, replyPtr);
	PACK_FLOAT ( pca->atextUp.y, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextPath) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->atextPath, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextAlignment) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD16 ( pca->atextAlignment.vertical, replyPtr);
	PACK_CARD16 ( pca->atextAlignment.horizontal, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCAtextStyle) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->atextStyle, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCTextBundleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->textIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineType) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->lineType, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineWidth) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->lineWidth, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->lineColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->lineColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCurveApproximation) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT) + sizeof(CARD32), reply,
		    pexGetPipelineContextReply, replyPtr);
	PACK_CARD32 ( pca->curveApprox.approxMethod, replyPtr);
	PACK_FLOAT ( pca->curveApprox.tolerance, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPolylineInterp) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->lineInterp, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLineBundleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->lineIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorStyle) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->intStyle, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorStyleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->intStyleIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->surfaceColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->surfaceColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceReflAttr) {
	CHK_PEX_BUF(size, 5 * sizeof(PEXFLOAT) + sizeof(CARD32)
				+ SIZE_COLOURSPEC(pca->reflAttr.specularColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_FLOAT ( pca->reflAttr.ambient, replyPtr);
	PACK_FLOAT ( pca->reflAttr.diffuse, replyPtr);
	PACK_FLOAT ( pca->reflAttr.specular, replyPtr);
	PACK_FLOAT ( pca->reflAttr.specularConc, replyPtr);
	PACK_FLOAT ( pca->reflAttr.transmission, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->reflAttr.specularColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceReflModel) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->reflModel, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceInterp) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->surfInterp, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfInteriorStyle) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->bfIntStyle, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfInteriorStyleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->bfIntStyleIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->bfSurfColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->bfSurfColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceReflAttr) {
	CHK_PEX_BUF(size, 5 * sizeof(PEXFLOAT) + sizeof(CARD32)
				+ SIZE_COLOURSPEC(pca->bfReflAttr.specularColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_FLOAT ( pca->bfReflAttr.ambient, replyPtr);
	PACK_FLOAT ( pca->bfReflAttr.diffuse, replyPtr);
	PACK_FLOAT ( pca->bfReflAttr.specular, replyPtr);
	PACK_FLOAT ( pca->bfReflAttr.specularConc, replyPtr);
	PACK_FLOAT ( pca->bfReflAttr.transmission, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->bfReflAttr.specularColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceReflModel) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->bfReflModel, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCBfSurfaceInterp) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->bfSurfInterp, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceApproximation) {
	CHK_PEX_BUF(size, sizeof(CARD32) + 2 * sizeof(PEXFLOAT), reply,
		    pexGetPipelineContextReply, replyPtr);
	PACK_CARD32 ( pca->surfApprox.approxMethod, replyPtr);
	PACK_FLOAT ( pca->surfApprox.uTolerance, replyPtr);
	PACK_FLOAT ( pca->surfApprox.vTolerance, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCCullingMode) {
        CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
                    replyPtr);
        PACK_CARD32 ( pca->cullMode, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCDistinguishFlag) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->distFlag, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternSize) {
	CHK_PEX_BUF(size, 2 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->patternSize.x, replyPtr);
	PACK_FLOAT ( pca->patternSize.y, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefPt) {
	CHK_PEX_BUF(size, 3 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->patternRefPt.x, replyPtr);
	PACK_FLOAT ( pca->patternRefPt.y, replyPtr);
	PACK_FLOAT ( pca->patternRefPt.z, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefVec1) {
	CHK_PEX_BUF(size, 3 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->patternRefV1.x, replyPtr);
	PACK_FLOAT ( pca->patternRefV1.y, replyPtr);
	PACK_FLOAT ( pca->patternRefV1.z, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPatternRefVec2) {
	CHK_PEX_BUF(size, 3 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->patternRefV2.x, replyPtr);
	PACK_FLOAT ( pca->patternRefV2.y, replyPtr);
	PACK_FLOAT ( pca->patternRefV2.z, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCInteriorBundleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->intIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeFlag) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->edges, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeType) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->edgeType, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeWidth) {
	CHK_PEX_BUF(size, sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_FLOAT ( pca->edgeWidth, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSurfaceEdgeColour) {
	CHK_PEX_BUF(size, sizeof(CARD32) + SIZE_COLOURSPEC(pca->edgeColour),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_COLOUR_SPECIFIER ( pca->edgeColour, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCEdgeBundleIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->edgeIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLocalTransform) {
	int i, j;
	CHK_PEX_BUF(size, 16 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		PACK_FLOAT( pca->localMat[i][j], replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCGlobalTransform) {
	int i, j;
	CHK_PEX_BUF(size, 16 * sizeof(PEXFLOAT), reply, pexGetPipelineContextReply,
		    replyPtr);
	for (i=0; i<4; i++)
	    for (j=0; j<4; j++)
		PACK_FLOAT( pca->globalMat[i][j], replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCModelClip) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->modelClip, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCModelClipVolume) {
	int i;
	pexHalfSpace *pphs = (pexHalfSpace *)(pca->modelClipVolume->pList);
	CHK_PEX_BUF(size, sizeof(CARD32)+(pca->modelClipVolume->numObj * sizeof(pexHalfSpace)),
	    reply, pexGetPipelineContextReply, replyPtr);
	PACK_CARD32 ( pca->modelClipVolume->numObj, replyPtr);
	for (i=0; i<pca->modelClipVolume->numObj; i++, pphs++) {
	    PACK_COORD3D( &pphs->point, replyPtr);
	    PACK_VECTOR3D( &pphs->vector, replyPtr);
	}
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCViewIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->viewIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCLightState) {
/*	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->lightState, replyPtr);
*/	int i;
	int sz = pca->lightState->numObj*sizeof(CARD16);
	CARD16 *pLS = (CARD16 *)(pca->lightState->pList);
	CHK_PEX_BUF(size,sizeof(CARD32)+sz+PADDING(sz),
		    reply, pexGetPipelineContextReply, replyPtr);
	PACK_CARD32(pca->lightState->numObj, replyPtr);
	for (i=0; i<pca->lightState->numObj; i++, pLS++) {
	    PACK_CARD16(*pLS, replyPtr); 
	}
        if (pca->lightState->numObj % 2)
	    SKIP_PADDING(replyPtr,2);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCDepthCueIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->depthCueIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCSetAsfValues) {
	CARD32  asf_enables;
	CHK_PEX_BUF(size, 2*sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
        asf_enables = ASF_ALL;
	PACK_CARD32 ( asf_enables, replyPtr);
	PACK_CARD32 ( pca->asfs, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCPickId) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->pickId, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCHlhsrIdentifier) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->hlhsrType, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCNameSet) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( GetId(pca->pCurrentNS), replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCColourApproxIndex) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->colourApproxIndex, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCRenderingColourModel) {
	CHK_PEX_BUF(size, sizeof(CARD32), reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_CARD32 ( pca->rdrColourModel, replyPtr);
    }

    CHECK_BITMASK_ARRAY(strmPtr->itemMask, PEXPCParaSurfCharacteristics) {
	switch (pca->psc.type) {
            case PEXPSCNone:
	    case PEXPSCImpDep: 
		sze = 0;
		break;
	    case PEXPSCIsoCurves: 
		sze = 8;
		break;
            case PEXPSCMcLevelCurves: 
		sze = (6 * sizeof(PEXFLOAT)) + 4 +  
		  (pca->psc.data.mcLevelCurves.numberIntersections *
		  sizeof(PEXFLOAT)) ;
		break;
	    case PEXPSCWcLevelCurves: 
		sze = (6 * sizeof(PEXFLOAT)) + 4 +  
		  (pca->psc.data.wcLevelCurves.numberIntersections *
		  sizeof(PEXFLOAT)) ;
		break;
	    default:
		sze = 0;
		break;
	}

	CHK_PEX_BUF(size, sizeof(CARD32)+sze, reply, pexGetPipelineContextReply,
		    replyPtr);
	PACK_INT16(pca->psc.type, replyPtr);
	PACK_INT16(sze, replyPtr);
	switch (pca->psc.type) {
	    case PEXPSCNone:
	    case PEXPSCImpDep:
		break;
	    case PEXPSCIsoCurves: {
		PACK_CARD16(pca->psc.data.isoCurves.placementType, replyPtr);
		SKIP_PADDING(replyPtr,2);
		PACK_CARD16(pca->psc.data.isoCurves.numUcurves, replyPtr);
		PACK_CARD16(pca->psc.data.isoCurves.numVcurves, replyPtr);
		break;
	    }
	    case PEXPSCMcLevelCurves: {
		PACK_COORD3D(&(pca->psc.data.mcLevelCurves.origin),replyPtr);
		PACK_COORD3D(&(pca->psc.data.mcLevelCurves.direction),replyPtr);
		PACK_CARD16(pca->psc.data.mcLevelCurves.numberIntersections,replyPtr);
		SKIP_PADDING(replyPtr,2);
		PACK_LISTOF_STRUCT(pca->psc.data.mcLevelCurves.numberIntersections,
			    PEXFLOAT, pca->psc.data.mcLevelCurves.pPoints,
			    replyPtr);
		break;
	    }

	    case PEXPSCWcLevelCurves: {
		PACK_COORD3D(&(pca->psc.data.wcLevelCurves.origin),replyPtr);
		PACK_COORD3D(&(pca->psc.data.wcLevelCurves.direction),replyPtr);
		PACK_CARD16(pca->psc.data.wcLevelCurves.numberIntersections,replyPtr);
		SKIP_PADDING(replyPtr,2);
		PACK_LISTOF_STRUCT(pca->psc.data.wcLevelCurves.numberIntersections,
				PEXFLOAT, pca->psc.data.wcLevelCurves.pPoints,
				replyPtr);
		break;
	    }
	}
    }

    pPEXBuffer->dataSize = (int)replyPtr - (int)(pPEXBuffer->pBuf); 

    reply->length = (unsigned long)LWORDS(pPEXBuffer->dataSize);
    WritePEXBufferReply(pexGetPipelineContextReply);

    return (err);

} /* end-PEXGetPipelineContext() */

/*++	PEXChangePipelineContext
 --*/
ErrorCode
PEXChangePipelineContext( cntxtPtr, strmPtr )
pexContext 			*cntxtPtr;
pexChangePipelineContextReq	*strmPtr;
{
    ErrorCode err = Success;
    ddPCStr *pc;

    LU_PIPELINECONTEXT(strmPtr->pc, pc);

    err = UpdatePipelineContext(    cntxtPtr, pc->pPCAttr, strmPtr->itemMask,
				    (unsigned char *)(strmPtr + 1));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangePipelineContext() */
/*++
 *
 *	End of File
 *
 --*/
