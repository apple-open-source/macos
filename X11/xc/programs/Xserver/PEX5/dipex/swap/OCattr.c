/* $Xorg: OCattr.c,v 1.4 2001/02/09 02:04:15 xorgcvs Exp $ */

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
#define LOCAL_FLAG
#include "OCattr.h"

/*****************************************************************
 * Output Commands 
 *****************************************************************/

/*
    OC Conversion routines do not chain to another routine, so that these
    conversions may also be used for FetchElements and StoreElements, as
    well as RenderOC;
 */

ErrorCode
SwapPEXMarkerType (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexMarkerType	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->markerType);

}

ErrorCode
SwapPEXMarkerScale (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexMarkerScale	*strmPtr;
{
    SWAP_FLOAT (strmPtr->scale);

}

ErrorCode
SwapPEXMarkerColourIndex (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexMarkerColourIndex	*strmPtr;
{
    SWAP_TABLE_INDEX (strmPtr->index);

}

ErrorCode
SwapPEXMarkerBundleIndex (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexMarkerBundleIndex	*strmPtr;
{
    SWAP_TABLE_INDEX (strmPtr->index);

}

/*
typedef pexMarkerColourIndex pexTextColourIndex;
typedef pexMarkerColourIndex pexSurfaceEdgeColourIndex;
typedef pexMarkerBundleIndex pexTextFontIndex;
typedef pexMarkerBundleIndex pexTextBundleIndex;
typedef pexMarkerBundleIndex pexLineBundleIndex;
typedef pexMarkerBundleIndex pexInteriorStyleIndex;
typedef pexMarkerBundleIndex pexBfInteriorStyleIndex;
typedef pexMarkerBundleIndex pexInteriorBundleIndex;
typedef pexMarkerBundleIndex pexEdgeBundleIndex;
typedef pexMarkerBundleIndex pexViewIndex;
typedef pexMarkerBundleIndex pexDepthCueIndex;
*/

ErrorCode
SwapPEXAtextStyle (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexAtextStyle	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->style);

}

ErrorCode
SwapPEXTextPrecision (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexTextPrecision	*strmPtr;
{
    SWAP_CARD16 (strmPtr->precision);

}

ErrorCode
SwapPEXCharExpansion (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexCharExpansion	*strmPtr;
{
    SWAP_FLOAT (strmPtr->expansion);

}

ErrorCode
SwapPEXCharSpacing (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexCharSpacing	*strmPtr;
{
    SWAP_FLOAT (strmPtr->spacing);

}

ErrorCode
SwapPEXCharHeight (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexCharHeight	*strmPtr;
{
    SWAP_FLOAT (strmPtr->height);

}

/* typedef pexCharHeight pexAtextHeight;*/

ErrorCode
SwapPEXCharUpVector (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexCharUpVector	*strmPtr;
{
    SWAP_VECTOR2D (strmPtr->up);

}

/* typedef pexCharUpVector pexAtextUpVector; */

ErrorCode
SwapPEXTextPath (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexTextPath	*strmPtr;
{
    SWAP_CARD16 (strmPtr->path);

}

/* typedef pexTextPath pexAtextPath;*/

ErrorCode
SwapPEXTextAlignment (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexTextAlignment	*strmPtr;
{
    SWAP_TEXT_ALIGN_DATA (strmPtr->alignment);

}

/* typedef pexTextAlignment pexAtextAlignment;*/

ErrorCode
SwapPEXLineType (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexLineType	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->lineType);

}

ErrorCode
SwapPEXLineWidth (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexLineWidth	*strmPtr;
{
    SWAP_FLOAT (strmPtr->width);

}

ErrorCode
SwapPEXLineColourIndex (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexLineColourIndex	*strmPtr;
{
    SWAP_TABLE_INDEX (strmPtr->index);

}

ErrorCode
SwapPEXCurveApproximation (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexCurveApproximation	*strmPtr;
{

    SWAP_CURVE_APPROX (strmPtr->approx);

}

ErrorCode
SwapPEXPolylineInterp (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexPolylineInterp	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->polylineInterp);

}

ErrorCode
SwapPEXInteriorStyle (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexInteriorStyle	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->interiorStyle);

}

/* typedef pexInteriorStyle pexBfInteriorStyle;*/

ErrorCode
SwapPEXSurfaceColourIndex (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceColourIndex	*strmPtr;
{
    SWAP_TABLE_INDEX (strmPtr->index);

}

/* typedef pexSurfaceColourIndex pexBfSurfaceColourIndex;*/
/* typedef pexSurfaceColour pexBfSurfaceColour;*/

ErrorCode
SwapPEXSurfaceReflModel (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceReflModel	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->reflectionModel);

}


/* typedef pexSurfaceReflModel pexBfSurfaceReflModel;*/

ErrorCode
SwapPEXSurfaceInterp (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceInterp	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->surfaceInterp);

}


/* typedef pexSurfaceInterp pexBfSurfaceInterp;*/

ErrorCode
SwapPEXSurfaceApproximation (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceApproximation	*strmPtr;
{

    SwapSurfaceApprox (swapPtr, &(strmPtr->approx));
}

ErrorCode
SwapPEXCullingMode (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexCullingMode	*strmPtr;
{
    SWAP_CULL_MODE (strmPtr->cullMode);

}

ErrorCode
SwapPEXDistinguishFlag (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexDistinguishFlag	*strmPtr;
{
    SWAP_CARD16 (strmPtr->distinguish);

}

ErrorCode
SwapPEXPatternSize (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexPatternSize	*strmPtr;
{
    SWAP_COORD2D (strmPtr->size);

}

ErrorCode
SwapPEXPatternRefPt (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexPatternRefPt	*strmPtr;
{
    SWAP_COORD2D (strmPtr->point);

}

ErrorCode
SwapPEXPatternAttr (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexPatternAttr	*strmPtr;
{
    SWAP_COORD3D (strmPtr->refPt);
    SWAP_VECTOR3D (strmPtr->vector1);
    SWAP_VECTOR3D (strmPtr->vector2);

}

ErrorCode
SwapPEXSurfaceEdgeFlag (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceEdgeFlag	*strmPtr;
{

}

ErrorCode
SwapPEXSurfaceEdgeType (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceEdgeType	*strmPtr;
{
    SWAP_ENUM_TYPE_INDEX (strmPtr->edgeType);

}

ErrorCode
SwapPEXSurfaceEdgeWidth (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexSurfaceEdgeWidth	*strmPtr;
{
    SWAP_FLOAT (strmPtr->width);

}

ErrorCode
SwapPEXSetAsfValues (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexSetAsfValues	*strmPtr;
{
    SWAP_ASF_ATTR (strmPtr->attribute);

}

ErrorCode
SwapPEXLocalTransform (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexLocalTransform	*strmPtr;
{
    SWAP_COMPOSITION (strmPtr->compType);
    SWAP_MATRIX (strmPtr->matrix);

}

ErrorCode
SwapPEXLocalTransform2D (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexLocalTransform2D	*strmPtr;
{
    SWAP_COMPOSITION (strmPtr->compType);
    SWAP_MATRIX_3X3 (strmPtr->matrix3X3);

}

ErrorCode
SwapPEXGlobalTransform (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexGlobalTransform	*strmPtr;
{
    SWAP_MATRIX (strmPtr->matrix);

}

ErrorCode
SwapPEXGlobalTransform2D (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexGlobalTransform2D	*strmPtr;
{
    SWAP_MATRIX_3X3 (strmPtr->matrix3X3);

}

ErrorCode
SwapPEXModelClip (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexModelClip	*strmPtr;
{

}

ErrorCode
SwapPEXRestoreModelClip (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexRestoreModelClip	*strmPtr;
{

}


ErrorCode
SwapPEXPickId (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexPickId	*strmPtr;
{
    SWAP_CARD32 (strmPtr->pickId);

}

ErrorCode
SwapPEXHlhsrIdentifier (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexHlhsrIdentifier	*strmPtr;
{
    SWAP_CARD32 (strmPtr->hlhsrID);

}


ErrorCode
SwapPEXExecuteStructure (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexExecuteStructure	*strmPtr;
{
    SWAP_STRUCTURE (strmPtr->id);

}

ErrorCode
SwapPEXLabel (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexLabel	*strmPtr;
{
    SWAP_CARD32 (strmPtr->label);

}

ErrorCode
SwapPEXApplicationData (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexApplicationData	*strmPtr;
{
    SWAP_CARD16 (strmPtr->numElements);

}

    ErrorCode
SwapPEXGse (swapPtr, strmPtr)
pexSwap	  *swapPtr;
pexGse	    *strmPtr;
{
    SWAP_CARD32 (strmPtr->id);
    SWAP_CARD16 (strmPtr->numElements);

}

ErrorCode
SwapPEXRenderingColourModel (swapPtr, strmPtr)
pexSwap			*swapPtr;
pexRenderingColourModel	*strmPtr;
{
    SWAP_INT16(strmPtr->model);
}

ErrorCode
SwapPEXOCUnused (swapPtr, strmPtr)
pexSwap		*swapPtr;
pexElementInfo	*strmPtr;
{
}


/*****************************************************************
 * utilities							 *
 *****************************************************************/

unsigned char *
SwapCoord4DList(swapPtr, ptr, num)
pexSwap	  	*swapPtr;
pexCoord4D	*ptr;
CARD32		num;
{
    pexCoord4D *pc;
    int i;
    for (i=0, pc = (pexCoord4D *)ptr; i<num; i++, pc++)
	SWAP_COORD4D ((*pc));
    return (unsigned char *) pc;
}

unsigned char *
SwapCoord3DList(swapPtr, ptr, num)
pexSwap	  	*swapPtr;
pexCoord3D	*ptr;
CARD32		num;
{
    pexCoord3D *pc;
    int i;
    for (i=0, pc = ptr; i<num; i++, pc++)
	SWAP_COORD3D ((*pc));
    return (unsigned char *) pc;
}

unsigned char *
SwapCoord2DList(swapPtr, ptr, num)
pexSwap	  	*swapPtr;
pexCoord2D	*ptr;
CARD32		num;
{
    pexCoord2D *pc;
    int i;
    for (i=0, pc = (pexCoord2D *)ptr; i<num; i++, pc++)
	SWAP_COORD2D ((*pc));
    return (unsigned char *) pc;
}

unsigned char *
SwapColour(swapPtr, pc, form)
pexSwap		    *swapPtr;
pexColour	    *pc;
pexEnumTypeIndex    form;
{
    unsigned char *ptr = (unsigned char *)pc;

    switch (form) {

	case PEXIndexedColour:	{ SWAP_INDEXED_COLOUR (pc->format.indexed);
				  ptr += sizeof(pexIndexedColour);
				  break; }

	case PEXRgbFloatColour:	{SWAP_RGB_FLOAT_COLOUR(pc->format.rgbFloat);
				 ptr += sizeof(pexRgbFloatColour);
				 break; }

	case PEXCieFloatColour:	{SWAP_CIE_COLOUR(pc->format.cieFloat);
				 ptr += sizeof(pexCieColour);
				 break; }

	case PEXHsvFloatColour:	{SWAP_HSV_COLOUR(pc->format.hsvFloat);
				 ptr += sizeof(pexHsvColour);
				 break; }

	case PEXHlsFloatColour:	{SWAP_HLS_COLOUR(pc->format.hlsFloat);
				 ptr += sizeof(pexHlsColour);
				 break; }

	case PEXRgb16Colour:	{SWAP_RGB16_COLOUR(pc->format.rgb16);
				 ptr += sizeof(pexRgb16Colour);
				 break; }
	case PEXRgb8Colour:	{ptr += sizeof(pexRgb8Colour);
				 break; }

		  }
    return(ptr);
}



void
SwapHalfSpace(swapPtr, ph) 
pexSwap		*swapPtr;
pexHalfSpace	*ph;
{
    SWAP_COORD3D (ph->point); 
    SWAP_VECTOR3D (ph->vector); 
}

void
SwapHalfSpace2D(swapPtr, ph) 
pexSwap		*swapPtr;
pexHalfSpace2D	*ph;
{
    SWAP_COORD2D (ph->point); 
    SWAP_VECTOR2D (ph->vector); 
}

unsigned char *
SwapOptData(swapPtr, po, vertexAttribs, colourType)
pexSwap		*swapPtr;
unsigned char	*po;
pexBitmaskShort vertexAttribs;
pexColourType	colourType;
{
    if (vertexAttribs & PEXGAColour) {
	switch (colourType) {

	    case PEXIndexedColour:	{
		pexIndexedColour *pc = (pexIndexedColour *)po;
		SWAP_INDEXED_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	    case PEXRgbFloatColour: {
		pexRgbFloatColour *pc = (pexRgbFloatColour *)po;
		SWAP_RGB_FLOAT_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	    case PEXCieFloatColour: {
		pexCieColour *pc = (pexCieColour *)po;
		SWAP_CIE_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	    case PEXHsvFloatColour: {
		pexHsvColour *pc = (pexHsvColour *)po;
		SWAP_HSV_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	    case PEXHlsFloatColour: {
		pexHlsColour *pc = (pexHlsColour *)po;
		SWAP_HLS_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	    case PEXRgb8Colour: {
		pexRgb8Colour *pc = (pexRgb8Colour *)po;
		po = (unsigned char *)(pc+1); break; }

	    case PEXRgb16Colour: {
		pexRgb16Colour *pc = (pexRgb16Colour *)po;
		SWAP_RGB16_COLOUR((*pc));
		po = (unsigned char *)(pc+1); break; }

	}
    };

    if (vertexAttribs & PEXGANormal) {
    	pexCoord3D *pn = (pexCoord3D *)po;
	SWAP_VECTOR3D((*pn));
	po = (unsigned char *)(pn+1);
    };

    if (vertexAttribs & PEXGAEdges) {
    	CARD16 *pe = (CARD16 *)po;
	SWAP_CARD16((*pe));
	po = (unsigned char *)(pe+2);		/* padding too */
    };

    return (po);
}


unsigned char *
SwapVertex(swapPtr, pv, vertexAttribs, colourType) 
pexSwap		*swapPtr;
pexVertex	*pv;
pexBitmaskShort vertexAttribs;
pexColourType	colourType;
{
    CARD8 *ptr;
    SWAP_COORD3D ((pv->point));

    ptr = SwapOptData(	swapPtr, (unsigned char *)(pv+1), vertexAttribs,
			colourType); 
    return (ptr);

}

void
SwapTextAlignmentData(swapPtr, ptr) 
pexSwap			*swapPtr;
pexTextAlignmentData	*ptr;
{
    SWAP_TEXT_V_ALIGNMENT (ptr->vertical);
    SWAP_TEXT_H_ALIGNMENT (ptr->horizontal); 
}


SwapIndexedColourList(swapPtr, ptr, num)
pexSwap		    *swapPtr;
pexIndexedColour    *ptr;
CARD32		    num;
{
    int i;
    for (i=0; i<(num); i++, ptr++){
	SWAP_INDEXED_COLOUR((*ptr));
    }
}

SwapRgbFloatColourList(swapPtr, ptr, num)
pexSwap		    *swapPtr;
pexRgbFloatColour   *ptr;
CARD32		    num;
{
    int i;
    pexRgbFloatColour *pc = ptr;
    for (i=0; i<(num); i++, pc++){
	SWAP_RGB_FLOAT_COLOUR((*pc));
    }
}


void
SwapSurfaceApprox(swapPtr, ptr) 
pexSwap		    *swapPtr;
pexSurfaceApprox    *ptr;
{
    SWAP_ENUM_TYPE_INDEX (ptr->approxMethod);
    SWAP_FLOAT (ptr->uTolerance);
    SWAP_FLOAT (ptr->vTolerance); 
}


unsigned char *
SwapTrimCurve(swapPtr, pTC) 
pexSwap *swapPtr;
pexTrimCurve *pTC;
{
    int i;
    pexCoord3D *pc;
    PEXFLOAT *pf = 0;
    unsigned char *ptr = 0;
    SWAP_CARD16 (pTC->order);
    SWAP_INT16 (pTC->approxMethod);
    SWAP_FLOAT (pTC->tolerance);
    SWAP_FLOAT (pTC->tMin);
    SWAP_FLOAT (pTC->tMax);

    /* curveType, numKnots and numCoord are swapped by the calling routines */

    for (i=0, pf = (PEXFLOAT *)(pTC+1); i<pTC->numKnots; i++, pf++)
	SWAP_FLOAT((*pf));

    ptr = (unsigned char *)pf;
    if (pTC->type == PEXRational) 
	ptr = SwapCoord3DList(swapPtr, (pexCoord3D *)ptr, pTC->numCoord);
    else 
	ptr = SwapCoord2DList(swapPtr, (pexCoord2D *)ptr, pTC->numCoord);

    return ptr;
}
