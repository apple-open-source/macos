/* $Xorg: miCopy.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miCopy.c,v 3.8 2001/12/14 19:57:21 dawes Exp $ */


#include "ddpex.h"
#include "ddpex3.h"
#include "PEX.h"
#include "PEXproto.h"
#include "pexExtract.h"
#include "ddpex2.h"
#include "miStruct.h"
#include "pexUtils.h"
#include "pexos.h"


/*
    bcopy data, fix up pointers
 */
/*
    Please note that any routines added to this file may also cause
    a corresponding modification to the level function tables (miTables.c)
 */

/*
    Coders must ensure that storage is allocated in the same chunks as for the
    corresponding parse function, otherwise unfortunate things may
    happen during freeing of storage.
 */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define CAT(a,b)    a##b
#else
#define CAT(a,b)    a/**/b
#endif

#define OC_COPY_FUNC_HEADER(suffix)   \
    ddpex2rtn CAT(copy,suffix)(pSrc, ppDst) \
    miGenericElementStr	    *pSrc; \
    miGenericElementStr	    **ppDst;

#define DST_STORE_AND_COPY(DD_ST, TYPE, SIZE) \
    *ppDst = (miGenericElementPtr) \
		xalloc((unsigned long)((SIZE) + sizeof(miGenericElementStr))); \
    if (!(*ppDst)) return (BadAlloc); \
    memmove(  (char *)(*ppDst), (char *)pSrc, \
	    (int)((SIZE) + sizeof(miGenericElementStr))); \
    DD_ST = (TYPE *)((*ppDst)+1);

#define COPY_DECL(NAME,TYPE) \
	TYPE * CAT(dst,NAME) = 0, * CAT(src,NAME) = (TYPE *)(pSrc + 1);

#define COPY_MORE(DST, TYPE, NUMBER, SRC) \
    if ((NUMBER) > 0) { \
	DST = (TYPE *)xalloc((unsigned long)((NUMBER) * sizeof(TYPE))); \
	if (!(DST)) err = BadAlloc; \
	else memmove((char *)(DST),(char *)(SRC),(int)((NUMBER)*sizeof(TYPE))); } \
    else DST = 0;

/*
    Returns number of bytes used to store the data
    Differs from similar routine in pexOCParse.c because that one counts
	from the protocol format; this one counts dd native format instead
 */
static int
CountddFacetOptData(pFacet)
listofddFacet	    *pFacet;
{
    switch(pFacet->type){
	case DD_FACET_INDEX_NORM: 
    	    return (sizeof(ddIndexNormal) * pFacet->numFacets);

	case DD_FACET_RGBFLOAT_NORM: 
	    return (sizeof(ddRgbFloatNormal) * pFacet->numFacets);

	case DD_FACET_CIE_NORM: 
	    return (sizeof(ddCieNormal) * pFacet->numFacets);

	case DD_FACET_HSV_NORM: 
	    return (sizeof(ddHsvNormal) * pFacet->numFacets);

	case DD_FACET_HLS_NORM: 
	    return (sizeof(ddHlsNormal) * pFacet->numFacets);

	case DD_FACET_RGB8_NORM: 
	    return (sizeof(ddRgb8Normal) * pFacet->numFacets);

	case DD_FACET_RGB16_NORM: 
	    return (sizeof(ddRgb16Normal) * pFacet->numFacets);

	case DD_FACET_NORM: 
	    return (sizeof(ddVector3D) * pFacet->numFacets);

	case DD_FACET_INDEX: 
	    return (sizeof(ddIndexedColour) * pFacet->numFacets);

	case DD_FACET_RGBFLOAT: 
	    return (sizeof(ddRgbFloatColour) * pFacet->numFacets);

	case DD_FACET_CIE: 
	    return (sizeof(ddCieColour) * pFacet->numFacets);

	case DD_FACET_HSV: 
	    return (sizeof(ddHsvColour) * pFacet->numFacets);

	case DD_FACET_HLS: 
	    return (sizeof(ddHlsColour) * pFacet->numFacets);

	case DD_FACET_RGB8: 
	    return (sizeof(ddRgb8Colour) * pFacet->numFacets);

	case DD_FACET_NONE:
	default:
	    return (0);
    }
}

static int
CountddVertexData(pPoint, point_type)
listofddPoint	*pPoint;
ddPointType	point_type;
{
    switch (point_type) {
	case DD_INDEX_NORM_EDGE_POINT: 
	    return (sizeof(ddIndexNormEdgePoint) * pPoint->numPoints);
		
	case DD_RGBFLOAT_NORM_EDGE_POINT: 
	    return (sizeof(ddRgbFloatNormEdgePoint) * pPoint->numPoints);

	case DD_CIE_NORM_EDGE_POINT: 
	    return (sizeof(ddCieNormEdgePoint) * pPoint->numPoints);

	case DD_HSV_NORM_EDGE_POINT: 
	    return (sizeof(ddHsvNormEdgePoint) * pPoint->numPoints);

	case DD_HLS_NORM_EDGE_POINT: 
	    return (sizeof(ddHlsNormEdgePoint) * pPoint->numPoints);

	case DD_RGB8_NORM_EDGE_POINT: 
	    return (sizeof(ddRgb8NormEdgePoint) * pPoint->numPoints);

	case DD_RGB16_NORM_EDGE_POINT: 
	    return (sizeof(ddRgb16NormEdgePoint) * pPoint->numPoints);
	
	case DD_NORM_EDGE_POINT: 
	    return (sizeof(ddNormEdgePoint) * pPoint->numPoints);

	case DD_INDEX_NORM_POINT: 
	    return (sizeof(ddIndexNormalPoint) * pPoint->numPoints);
		
	case DD_RGBFLOAT_NORM_POINT: 
	    return (sizeof(ddRgbFloatNormalPoint) * pPoint->numPoints);

	case DD_CIE_NORM_POINT: 
	    return (sizeof(ddCieNormalPoint) * pPoint->numPoints);

	case DD_HSV_NORM_POINT: 
	    return (sizeof(ddHsvNormalPoint) * pPoint->numPoints);

	case DD_HLS_NORM_POINT: 
	    return (sizeof(ddHlsNormalPoint) * pPoint->numPoints);

	case DD_RGB8_NORM_POINT: 
	    return (sizeof(ddRgb8NormalPoint) * pPoint->numPoints);

	case DD_RGB16_NORM_POINT: 
	    return (sizeof(ddRgb16NormalPoint) * pPoint->numPoints);
	
	case DD_NORM_POINT: 
	    return (sizeof(ddNormalPoint) * pPoint->numPoints);

	case DD_INDEX_EDGE_POINT: 
	    return (sizeof(ddIndexEdgePoint) * pPoint->numPoints);

	case DD_RGBFLOAT_EDGE_POINT: 
	    return (sizeof(ddRgbFloatEdgePoint) * pPoint->numPoints);

	case DD_CIE_EDGE_POINT: 
	    return (sizeof(ddCieEdgePoint) * pPoint->numPoints);

	case DD_HSV_EDGE_POINT: 
	    return (sizeof(ddHsvEdgePoint) * pPoint->numPoints);

	case DD_HLS_EDGE_POINT: 
	    return (sizeof(ddHlsEdgePoint) * pPoint->numPoints);

	case DD_RGB8_EDGE_POINT: 
	    return (sizeof(ddRgb8EdgePoint) * pPoint->numPoints);

	case DD_RGB16_EDGE_POINT: 
	    return (sizeof(ddRgb16EdgePoint) * pPoint->numPoints);

	case DD_INDEX_POINT: 
	    return (sizeof(ddIndexPoint) * pPoint->numPoints);

	case DD_RGBFLOAT_POINT: 
	    return (sizeof(ddRgbFloatPoint) * pPoint->numPoints);

	case DD_CIE_POINT: 
	    return (sizeof(ddCiePoint) * pPoint->numPoints);

	case DD_HSV_POINT: 
	    return (sizeof(ddHsvPoint) * pPoint->numPoints);

	case DD_HLS_POINT: 
	    return (sizeof(ddHlsPoint) * pPoint->numPoints);

	case DD_RGB8_POINT: 
	    return (sizeof(ddRgb8Point) * pPoint->numPoints);

	case DD_RGB16_POINT: 
	    return (sizeof(ddRgb16Point) * pPoint->numPoints);

	case DD_EDGE_POINT: 
	    return (sizeof(ddEdgePoint) * pPoint->numPoints);

	case DD_3D_POINT: 
	    return (sizeof(ddCoord3D) * pPoint->numPoints);

	default:
	    return(0);
    }
}


OC_COPY_FUNC_HEADER(ColourOC)
{
    COPY_DECL(Colour,miColourStruct);

    switch (srcColour->colourType) {
	case PEXIndexedColour: {
	    DST_STORE_AND_COPY(	dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddIndexedColour)));
	    dstColour->colour.pIndex = (ddIndexedColour *)(dstColour+1);
	    break; }

	case PEXRgbFloatColour : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddRgbFloatColour)));
	    dstColour->colour.pRgbFloat = (ddRgbFloatColour *)(dstColour+1);
	    break; }

	case PEXCieFloatColour : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddCieColour)));
	    dstColour->colour.pCie = (ddCieColour *)(dstColour+1);
	    break; }

	case PEXHsvFloatColour : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddHsvColour)));
	    dstColour->colour.pHsv = (ddHsvColour *)(dstColour+1);
	    break; }

	case PEXHlsFloatColour : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddHlsColour)));
	    dstColour->colour.pHls = (ddHlsColour *)(dstColour+1);
	    break; }

	case PEXRgb8Colour  : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddRgb8Colour)));
	    dstColour->colour.pRgb8 = (ddRgb8Colour *)(dstColour+1);
	    break; }

	case PEXRgb16Colour : {
	    DST_STORE_AND_COPY( dstColour, miColourStruct,
				(sizeof(miColourStruct)
				+ sizeof(ddRgb16Colour)));
	    dstColour->colour.pRgb16 = (ddRgb16Colour *)(dstColour+1);
	    break; }

    }
    
    return(Success);
}

OC_COPY_FUNC_HEADER(ColourIndexOC)
{
    COPY_DECL(Colour,miColourStruct);

    DST_STORE_AND_COPY(	dstColour, miColourStruct, (sizeof(miColourStruct)
			+ sizeof(ddIndexedColour)));
    dstColour->colour.pIndex = (ddIndexedColour *)(dstColour+1);
    
    return(Success);
}

OC_COPY_FUNC_HEADER(LightState)
{
    COPY_DECL(LightState,miLightStateStruct);
    
    DST_STORE_AND_COPY(	dstLightState, miLightStateStruct,
			sizeof(miLightStateStruct)
			+ 2 * sizeof(listofObj)
			+ sizeof(CARD16) *
				(srcLightState->enableList->maxObj
				 + srcLightState->disableList->maxObj));

    dstLightState->enableList = (listofObj *)(dstLightState + 1);
    dstLightState->enableList->pList = (ddPointer)(dstLightState->enableList +1);
    dstLightState->disableList =
	(listofObj *)((dstLightState->enableList->pList +
			sizeof(CARD16) * dstLightState->enableList->maxObj));
    dstLightState->disableList->pList =
			    (ddPointer)(dstLightState->disableList + 1);
    return(Success);
}

/* can use the same for both 3D and 2D */
OC_COPY_FUNC_HEADER(MCVolume)
{
    int listSize = 0;
    COPY_DECL(MCVolume, miMCVolume_Struct);
    listSize = puCountList( DD_HALF_SPACE, srcMCVolume->halfspaces->numObj);
    DST_STORE_AND_COPY(dstMCVolume, miMCVolume_Struct, 
			sizeof(miMCVolume_Struct) + listSize);
    dstMCVolume->halfspaces = (listofObj *)(dstMCVolume+1);
    return(Success);
}

OC_COPY_FUNC_HEADER(Marker)
{
    listofddPoint   *dstPoint;
    COPY_DECL(Marker, miMarkerStruct);
    
    DST_STORE_AND_COPY( dstMarker, miListHeader,
			sizeof(miListHeader) + sizeof(listofddPoint)
			+ srcMarker->ddList->numPoints * sizeof(pexCoord3D));
    dstPoint = (listofddPoint *)(dstMarker+1);
    dstMarker->ddList = dstPoint;
    dstPoint->pts.p3Dpt = (ddCoord3D *)(dstPoint + 1);
    
    return(Success);
}


OC_COPY_FUNC_HEADER(Marker2D)
{
    listofddPoint   *dstPoint;
    COPY_DECL(Marker, miMarkerStruct);
    
    DST_STORE_AND_COPY( dstMarker, miListHeader,
			sizeof(miListHeader) + sizeof(listofddPoint)
			+ srcMarker->ddList->numPoints * sizeof(pexCoord2D));
    dstPoint = (listofddPoint *)(dstMarker+1);
    dstMarker->ddList = dstPoint;
    dstPoint->pts.p2Dpt = (ddCoord2D *)(dstPoint + 1);
    
    return(Success);
}


OC_COPY_FUNC_HEADER(Text)
{
    COPY_DECL(Text, miTextStruct);
    
    DST_STORE_AND_COPY( dstText, miTextStruct, sizeof(miTextStruct)
			+ 3 * sizeof(ddCoord3D)
			+ pSrc->element.pexOClength * sizeof(CARD32)
			- sizeof(pexText));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    dstText->pOrigin = (ddCoord3D *)(dstText + 1);
    dstText->pDirections = (dstText->pOrigin) + 1;
    dstText->pText = (pexMonoEncoding *)((dstText->pDirections) + 2);
    
    return(Success);
}
 

OC_COPY_FUNC_HEADER(Text2D)
{
    COPY_DECL(Text, miText2DStruct);
    
    DST_STORE_AND_COPY( dstText, miText2DStruct, sizeof(miText2DStruct)
			+ sizeof(ddCoord2D)
			+ pSrc->element.pexOClength * sizeof(CARD32)
			- sizeof(pexText2D));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    dstText->pOrigin = (ddCoord2D *)(dstText + 1);
    dstText->pText = (pexMonoEncoding *)((dstText->pOrigin) + 1);
    
    return(Success);

}


OC_COPY_FUNC_HEADER(AnnotationText)
{
    COPY_DECL(Text, miAnnoTextStruct);
    
    DST_STORE_AND_COPY( dstText, miAnnoTextStruct, sizeof(miAnnoTextStruct)
			+ 2 * sizeof(ddCoord3D)
			+ pSrc->element.pexOClength * sizeof(CARD32)
			- sizeof(pexAnnotationText));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    dstText->pOrigin = (ddCoord3D *)(dstText + 1);
    dstText->pOffset = (dstText->pOrigin) + 1;
    dstText->pText = (pexMonoEncoding *)((dstText->pOffset) + 1);
    
    return(Success);

}


OC_COPY_FUNC_HEADER(AnnotationText2D)
{
    COPY_DECL(Text, miAnnoText2DStruct);
    
    DST_STORE_AND_COPY( dstText, miAnnoText2DStruct, sizeof(miAnnoText2DStruct)
			+ 2 * sizeof(ddCoord2D)
			+ pSrc->element.pexOClength * sizeof(CARD32)
			- sizeof(pexAnnotationText2D));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    dstText->pOrigin = (ddCoord2D *)(dstText + 1);
    dstText->pOffset = (dstText->pOrigin) + 1;
    dstText->pText = (pexMonoEncoding *)((dstText->pOffset) + 1);
    
    return(Success);

}


OC_COPY_FUNC_HEADER(Polyline2D)
{
    listofddPoint	*dstPoint;
    COPY_DECL(Poly, miPolylineStruct);

    DST_STORE_AND_COPY( dstPoly, miListHeader,
			sizeof(miListHeader) + sizeof(listofddPoint)
			+ srcPoly->ddList->numPoints * sizeof(ddCoord2D));
    dstPoint           = (listofddPoint *)(dstPoly+1);
    dstPoly->ddList     = dstPoint;
    dstPoint->pts.p2Dpt = (ddCoord2D *)(dstPoint + 1);
    
    return(Success);
    
}



OC_COPY_FUNC_HEADER(Polyline)
{
    listofddPoint	*dstPoint;
    COPY_DECL(Poly, miPolylineStruct);

    DST_STORE_AND_COPY( dstPoly, miListHeader,
			sizeof(miListHeader) + sizeof(listofddPoint)
			+ srcPoly->ddList->numPoints * sizeof(ddCoord3D));
    dstPoint           = (listofddPoint *)(dstPoly+1);
    dstPoly->ddList     = dstPoint;
    dstPoint->pts.p3Dpt = (ddCoord3D *)(dstPoint + 1);
    
    return(Success);

}


OC_COPY_FUNC_HEADER(PolylineSet)
{
    listofddPoint	*pPoint;
    ddUSHORT		i;
    int			vertexSize = 0;
    ddPointer		ddPtr = 0;
    ddpex2rtn		err = Success;
    COPY_DECL(Poly, miPolylineStruct);
    
    for (i=0, pPoint = srcPoly->ddList; i<srcPoly->numLists; i++, pPoint++)
	vertexSize += CountddVertexData(pPoint, srcPoly->type);
    DST_STORE_AND_COPY(	dstPoly, miListHeader,
			(sizeof(miListHeader) + vertexSize
			+ srcPoly->numLists * sizeof(listofddPoint)));

    dstPoly->ddList = (listofddPoint *)(dstPoly+1);
    for (i=0, pPoint = dstPoly->ddList, vertexSize = 0,
	    ddPtr = (ddPointer)(dstPoly->ddList + dstPoly->numLists);
	 i<dstPoly->numLists;
	 i++, pPoint++) {
	vertexSize = CountddVertexData(pPoint, dstPoly->type);
	pPoint->pts.ptr = (char *)ddPtr;
	ddPtr += vertexSize;
	      /* could have subtracted pointers, but this is more maintainable */
    }

    return(Success);
    
}


OC_COPY_FUNC_HEADER(NurbCurve)
{
    ddUSHORT	    pointSize = 0;
    COPY_DECL(Nurb, miNurbStruct);
    
    pointSize = (srcNurb->points.type == DDPT_4D)
		    ? sizeof(ddCoord4D) : sizeof(ddCoord3D);
    DST_STORE_AND_COPY( dstNurb, miNurbStruct,
			sizeof(miNurbStruct) + sizeof(listofddPoint)
			+ srcNurb->numKnots * sizeof(PEXFLOAT)
			+ srcNurb->points.ddList->numPoints * pointSize);
    dstNurb->pKnots = (ddFLOAT *)(dstNurb+1);
    dstNurb->points.ddList =
			(listofddPoint *)(dstNurb->pKnots + srcNurb->numKnots);
    if (srcNurb->points.type == DDPT_4D) {
	dstNurb->points.ddList->pts.p4Dpt =
				    (ddCoord4D *)((dstNurb->points.ddList)+1);
    } else {
	dstNurb->points.ddList->pts.p3Dpt =
				    (ddCoord3D *)((dstNurb->points.ddList)+1);
    }

    return(Success);
}


OC_COPY_FUNC_HEADER(FillArea2D)
{
    COPY_DECL(Fill, miFillAreaStruct);

    DST_STORE_AND_COPY( dstFill, miFillAreaStruct, (sizeof(miFillAreaStruct) +
			sizeof(listofddFacet) + sizeof(listofddPoint)
			+ srcFill->points.ddList->numPoints *sizeof(ddCoord2D)));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);
    dstFill->points.ddList->pts.p2Dpt = (ddCoord2D *)((dstFill->points.ddList) + 1);
    
    return(Success);
}


 

OC_COPY_FUNC_HEADER(FillArea)
{
    COPY_DECL(Fill, miFillAreaStruct);

    DST_STORE_AND_COPY( dstFill, miFillAreaStruct, (sizeof(miFillAreaStruct) +
			sizeof(listofddFacet) + sizeof(listofddPoint)
			+ srcFill->points.ddList->numPoints *sizeof(ddCoord3D)));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);
    dstFill->points.ddList->pts.p3Dpt = (ddCoord3D *)((dstFill->points.ddList) + 1);
    
    return(Success);
}


OC_COPY_FUNC_HEADER(ExtFillArea)
{
    ddpex2rtn	err = Success;
    int		facetSize = 0, vertexSize = 0;
    ddPointer	facetPtr = 0, vertexPtr = 0;
    COPY_DECL(Fill, miFillAreaStruct);

    facetSize = CountddFacetOptData(srcFill->pFacets);
    vertexSize = CountddVertexData(srcFill->points.ddList, srcFill->points.type);
    DST_STORE_AND_COPY(	dstFill, miFillAreaStruct,
			(sizeof(miFillAreaStruct) + facetSize + vertexSize
			+ sizeof(listofddFacet) + sizeof(listofddPoint)));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);
    
    facetPtr = (ddPointer)(dstFill->points.ddList + 1);
    if (facetSize == 0)
	dstFill->pFacets->facets.pNoFacet = 0;
    else
	dstFill->pFacets->facets.pNoFacet = facetPtr;

    vertexPtr = facetPtr + facetSize;
    if (vertexSize == 0)
	dstFill->points.ddList->pts.ptr = 0;
    else
	dstFill->points.ddList->pts.ptr = (char *)vertexPtr;

    return(Success);
}


OC_COPY_FUNC_HEADER(FillAreaSet2D)
{
    listofddPoint   *ddPoint;
    ddpex2rtn	    err = Success;
    int		    listSize = 0;
    ddUSHORT	    i;
    ddPointer	    ddPtr = 0;
    COPY_DECL(Fill, miFillAreaStruct);

    for (   i=0, ddPoint = srcFill->points.ddList;
	    i < srcFill->points.numLists;
	    i++, ddPoint++ )
	listSize += ddPoint->numPoints * sizeof(ddCoord2D);

    DST_STORE_AND_COPY(	dstFill, miFillAreaStruct,
			(sizeof(miFillAreaStruct) + sizeof(listofddFacet)
			+ listSize
			+ (srcFill->points.numLists * sizeof(listofddPoint))));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);

    for (   i=0, ddPoint = dstFill->points.ddList,
		ddPtr = (ddPointer)(ddPoint + dstFill->points.numLists);
	    i<dstFill->points.numLists;
	    i++, ddPoint++ ) {

	ddPoint->pts.p2Dpt = (ddCoord2D *)ddPtr;
	ddPtr += ddPoint->numPoints * sizeof(ddCoord2D);
    }
    
    return(Success);
}


OC_COPY_FUNC_HEADER(FillAreaSet)
{
    listofddPoint   *ddPoint;
    ddpex2rtn	    err = Success;
    int		    listSize = 0;
    ddUSHORT	    i;
    ddPointer	    ddPtr = 0;
    COPY_DECL(Fill, miFillAreaStruct);

    for (   i=0, ddPoint = srcFill->points.ddList;
	    i < srcFill->points.numLists;
	    i++, ddPoint++ )
	listSize += ddPoint->numPoints * sizeof(ddCoord3D);

    DST_STORE_AND_COPY(	dstFill, miFillAreaStruct,
			(sizeof(miFillAreaStruct) + sizeof(listofddFacet)
			+ listSize
			+ (srcFill->points.numLists * sizeof(listofddPoint))));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);

    for (   i=0, ddPoint = dstFill->points.ddList,
		ddPtr = (ddPointer)(ddPoint + dstFill->points.numLists);
	    i<dstFill->points.numLists;
	    i++, ddPoint++ ) {

	ddPoint->pts.p3Dpt = (ddCoord3D *)ddPtr;
	ddPtr += ddPoint->numPoints * sizeof(ddCoord3D);
    }
    
    return(Success);
}


OC_COPY_FUNC_HEADER(ExtFillAreaSet)
{
    listofddPoint   *dstPoint, *srcPoint;
    ddUSHORT	    i;
    int		    facetSize = 0, vertexSize = 0;
    ddPointer	    facetPtr  = 0, vertexPtr = 0;
    ddpex2rtn	    err = Success;
    COPY_DECL(Fill, miFillAreaStruct);

    facetSize = CountddFacetOptData(srcFill->pFacets);
    for (i=0, srcPoint=srcFill->points.ddList;
	 i<srcFill->points.numLists;
	 i++, srcPoint++) {
	vertexSize += CountddVertexData(srcPoint, srcFill->points.type); }

    DST_STORE_AND_COPY(	dstFill, miFillAreaStruct,
			(sizeof(miFillAreaStruct) + sizeof(listofddFacet)
			+ facetSize + vertexSize
			+ (srcFill->points.numLists * sizeof(listofddPoint))));
    dstFill->pFacets = (listofddFacet *)(dstFill+1);
    dstFill->points.ddList = (listofddPoint *)((dstFill->pFacets)+1);

    facetPtr = (ddPointer)(dstFill->points.ddList + dstFill->points.numLists);
    if (facetSize == 0)
	dstFill->pFacets->facets.pNoFacet = 0;
    else
	dstFill->pFacets->facets.pNoFacet = facetPtr;

    vertexPtr = facetPtr + facetSize;

    for (i=0, dstPoint=dstFill->points.ddList, vertexSize = 0;
	 i<dstFill->points.numLists;
	 i++, dstPoint++, srcPoint++) {

	vertexSize = CountddVertexData(dstPoint, dstFill->points.type);
	dstPoint->pts.ptr = (char *)vertexPtr;
	vertexPtr += vertexSize;
    }
    
    return(Success);
}


OC_COPY_FUNC_HEADER(SOFAS)
{
    ddUSHORT	    i,j, k;
    miConnListList  *dstCLL, *srcCLL;
    miConnList	    *dstCList, *srcCList;
    ddpex2rtn	    err = Success;
    int		    vertexSize = 0, facetSize = 0, edgeSize = 0;
    ddPointer	    ptr;
    extern void destroySOFAS();
    COPY_DECL(Fill, miSOFASStruct);

    facetSize = CountddFacetOptData( &srcFill->pFacets);
    vertexSize = CountddVertexData( srcFill->points.ddList,
				    srcFill->points.type);
    if (srcFill->edgeData) {
	edgeSize = srcFill->numEdges * sizeof(ddUCHAR);
	edgeSize += ((4 - (edgeSize & 3)) & 3);
    }

    DST_STORE_AND_COPY(	dstFill, miSOFASStruct,
			sizeof(miSOFASStruct) + sizeof(listofddPoint)
			+ srcFill->numEdges * sizeof(ddUCHAR)
			+ facetSize + vertexSize + edgeSize +
			srcFill->connects.numListLists * sizeof(miConnListList));

    dstFill->points.ddList = (listofddPoint *)(dstFill + 1);
    ptr = (ddPointer)(dstFill->points.ddList + 1);
    if (facetSize == 0)	dstFill->pFacets.facets.pNoFacet = 0;
    else		dstFill->pFacets.facets.pNoFacet = ptr;
    ptr += facetSize;
    if (vertexSize == 0)    dstFill->points.ddList->pts.ptr = 0;
    else		    dstFill->points.ddList->pts.ptr = (char *)ptr;
    ptr += vertexSize;
    if (edgeSize == 0)	dstFill->edgeData = 0;
    else		dstFill->edgeData = ptr;
    ptr += edgeSize;
    dstFill->connects.data = (miConnListList *)ptr;
    for (  i=0, dstCLL = dstFill->connects.data, srcCLL = srcFill->connects.data;
	   i<srcFill->numFAS;
	   i++, dstCLL++, srcCLL++) {
	COPY_MORE(  dstCLL->pConnLists, miConnList,
		    srcCLL->numLists * sizeof(miConnList),
		    srcCLL->pConnLists);
	if (err != Success) {
	    destroySOFAS(dstFill);
	    return(BadAlloc);
	}
	for (	j=0,dstCList = dstCLL->pConnLists, srcCList = srcCLL->pConnLists;
		j<dstCLL->numLists;
		j++, dstCList++, srcCList++) {
	    COPY_MORE(	dstCList->pConnects, ddUSHORT, 
			srcCList->numLists * sizeof(ddUSHORT),
			srcCList->pConnects);
	    if (err != Success) {
		destroySOFAS(dstFill);
		return(BadAlloc);
	    }
	}
    }

    return(Success);
}



OC_COPY_FUNC_HEADER(TriangleStrip)
{
    ddpex2rtn	err = Success;
    int		vertexSize = 0, facetSize = 0;
    COPY_DECL(Triangle, miTriangleStripStruct);
    
    facetSize = CountddFacetOptData(srcTriangle->pFacets);
    vertexSize = CountddVertexData( srcTriangle->points.ddList,
				    srcTriangle->points.type);
    DST_STORE_AND_COPY(	dstTriangle, miTriangleStripStruct,
			(sizeof(miTriangleStripStruct) + sizeof(listofddFacet)
			+ vertexSize + facetSize + sizeof(listofddPoint)));
    dstTriangle->pFacets = (listofddFacet *)(dstTriangle+1);
    dstTriangle->points.ddList = (listofddPoint *)((dstTriangle->pFacets)+1);
    dstTriangle->pFacets->facets.pNoFacet
				= (ddPointer)(dstTriangle->points.ddList + 1);
    dstTriangle->points.ddList->pts.ptr
	    = (char *)(dstTriangle->pFacets->facets.pNoFacet + facetSize);
    
    return(Success);
}


OC_COPY_FUNC_HEADER(QuadrilateralMesh)
{
    ddpex2rtn err = Success;
    int		vertexSize = 0, facetSize = 0;
    COPY_DECL(Quad, miQuadMeshStruct);

    facetSize = CountddFacetOptData(srcQuad->pFacets);
    vertexSize = CountddVertexData(srcQuad->points.ddList, srcQuad->points.type);
    DST_STORE_AND_COPY(	dstQuad, miQuadMeshStruct, (sizeof(miQuadMeshStruct)
			+ vertexSize + facetSize
			+ sizeof(listofddFacet) + sizeof(listofddPoint)));
    dstQuad->pFacets = (listofddFacet *)(dstQuad+1);
    dstQuad->points.ddList = (listofddPoint *)((dstQuad->pFacets)+1);
    dstQuad->pFacets->facets.pNoFacet
		= (ddPointer)(dstQuad->points.ddList + 1);
    dstQuad->points.ddList->pts.ptr
		= (char *)(dstQuad->pFacets->facets.pNoFacet + facetSize);
    
    return(Success);
}


OC_COPY_FUNC_HEADER(NurbSurface)
{
    ddULONG		i, j, k;
    listofTrimCurve	*dstTrim, *srcTrim;
    ddTrimCurve		*dstTC, *srcTC;
    ddpex2rtn		err = Success;
    extern void destroyNurbSurface();
    COPY_DECL(Nurb, miNurbSurfaceStruct);

    DST_STORE_AND_COPY(dstNurb, miNurbSurfaceStruct, (sizeof(miNurbSurfaceStruct)
	+ (srcNurb->numUknots * srcNurb->numVknots) * (sizeof(ddFLOAT))
	+ (sizeof(listofddPoint))
	+ (srcNurb->mPts * srcNurb->nPts * sizeof(ddCoord4D)
	+ (sizeof(listofTrimCurve))
	+ srcNurb->numTrimCurveLists * sizeof(ddTrimCurve))));
    dstNurb->pUknots = (ddFLOAT *)(dstNurb+1);
    dstNurb->pVknots = (ddFLOAT *)((dstNurb->pUknots) + srcNurb->numUknots);
    dstNurb->points.ddList =
		(listofddPoint *)((dstNurb->pVknots) + srcNurb->numVknots);
    dstNurb->points.ddList->pts.ptr = (char *)(dstNurb->points.ddList + 1);
    dstNurb->trimCurves =
	(listofTrimCurve *)((dstNurb->points.ddList->pts.ptr)
			+ (srcNurb->mPts * srcNurb->nPts * sizeof(ddCoord4D)));

    for (i=0, dstTrim = dstNurb->trimCurves, srcTrim = srcNurb->trimCurves;
	 i<dstNurb->numTrimCurveLists;
	 i++, dstTrim++, srcTrim++) {

	dstTrim->pTC = (ddTrimCurve *)xalloc(srcTrim->count*sizeof(ddTrimCurve));
	COPY_MORE(dstTrim->pTC, ddTrimCurve, srcTrim->count, srcTrim->pTC);
	if (err) {
	    destroyNurbSurface(dstNurb);
	    return(BadAlloc);
	}

	for (	k=0, dstTC = dstTrim->pTC, srcTC = srcTrim->pTC;
		k < dstTrim->count;
		k++, dstTC++, srcTC++) {
	    COPY_MORE(dstTC->pKnots, ddFLOAT, dstTC->numKnots, srcTC->pKnots );
	    if (err) {
		dstTC->points.pts.ptr = 0;
		destroyNurbSurface(dstNurb);
		return(BadAlloc);
	    }
	    if (srcTC->pttype == DD_3D_POINT) {
		/* Note that this only works because these points are
		* never transformed */
		COPY_MORE(  dstTC->points.pts.p3Dpt, ddCoord3D,
			    dstTC->points.numPoints, srcTC->points.pts.p3Dpt );
	    } else {
		COPY_MORE(  dstTC->points.pts.p2Dpt, ddCoord2D,
			    dstTC->points.numPoints, srcTC->points.pts.p2Dpt );
	    }
	    if (err) {
		destroyNurbSurface(dstNurb);
		return(BadAlloc);
	    }
	}
    }
    return(Success);

} 

OC_COPY_FUNC_HEADER(CellArray2D)
{
    COPY_DECL(Cell, miCellArrayStruct);
    
    DST_STORE_AND_COPY(	dstCell, miCellArrayStruct, 
			sizeof(miCellArrayStruct) + sizeof(listofddPoint)
			+ 2 * sizeof(ddCoord2D)
			+ srcCell->dx * srcCell->dy * sizeof(ddIndexedColour));
    dstCell->point.ddList = (listofddPoint *)(dstCell+1);
    dstCell->point.ddList->pts.p2Dpt =
				    (ddCoord2D *)((dstCell->point.ddList) + 1);
    dstCell->colours.colour.pIndex =
		    (ddIndexedColour *)((dstCell->point.ddList->pts.p2Dpt) + 2);

    return(Success);
}


OC_COPY_FUNC_HEADER(CellArray)
{
    COPY_DECL(Cell, miCellArrayStruct);
    
    DST_STORE_AND_COPY(	dstCell, miCellArrayStruct, 
			sizeof(miCellArrayStruct) + sizeof(listofddPoint)
			+ 3 * sizeof(ddCoord3D)
			+ srcCell->dx * srcCell->dy * sizeof(ddIndexedColour));
    dstCell->point.ddList = (listofddPoint *)(dstCell+1);
    dstCell->point.ddList->pts.p3Dpt =
				    (ddCoord3D *)((dstCell->point.ddList) + 1);
    dstCell->colours.colour.pIndex =
		    (ddIndexedColour *)((dstCell->point.ddList->pts.p3Dpt) + 3);

    return(Success);

}

OC_COPY_FUNC_HEADER(ExtCellArray)
{
    unsigned long	size;
    COPY_DECL(Cell, miCellArrayStruct);

    size = (((srcCell->colours.colourType==PEXIndexedColour)
	    || (srcCell->colours.colourType==PEXRgb8Colour))  ? 4 :
		((srcCell->colours.colourType==PEXRgb16Colour) ? 8 : 12 ));
    DST_STORE_AND_COPY(	dstCell, miCellArrayStruct,
			sizeof(miCellArrayStruct) + sizeof(listofddPoint)
			+ 3 * sizeof(ddCoord3D)
			+ srcCell->dx * srcCell->dy * size);
    dstCell->point.ddList = (listofddPoint *)(dstCell+1);

    dstCell->point.ddList->pts.p3Dpt =
			    (ddCoord3D *)((dstCell->point.ddList) + 1);
    
    switch (srcCell->colours.colourType) {
	case PEXIndexedColour: {
	    dstCell->colours.colour.pIndex =
		    (ddIndexedColour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXRgbFloatColour : {
	    dstCell->colours.colour.pRgbFloat =
		    (ddRgbFloatColour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXCieFloatColour : {
	    dstCell->colours.colour.pCie =
		    (ddCieColour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXHsvFloatColour : {
	    dstCell->colours.colour.pHsv =
		    (ddHsvColour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXHlsFloatColour : {
	    dstCell->colours.colour.pHls =
		    (ddHlsColour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXRgb8Colour  : {
	    dstCell->colours.colour.pRgb8 =
		    (ddRgb8Colour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
	case PEXRgb16Colour : {
	    dstCell->colours.colour.pRgb16 =
		    (ddRgb16Colour *)((dstCell->point.ddList->pts.p3Dpt)+3);
	    break; }
    }

    return(Success);
    
}
 

OC_COPY_FUNC_HEADER(PSurfaceChars)
{
    COPY_DECL(PSC, miPSurfaceCharsStruct);

    switch (srcPSC->type) {
	case PEXPSCNone:
	case PEXPSCImpDep:
	    DST_STORE_AND_COPY( dstPSC, miPSurfaceCharsStruct,
				sizeof(miPSurfaceCharsStruct));
	    break;

	case PEXPSCIsoCurves: {
	    DST_STORE_AND_COPY( dstPSC, miPSurfaceCharsStruct,
				sizeof(miPSurfaceCharsStruct)
				+ sizeof(ddPSC_IsoparametricCurves));
	    dstPSC->data.pIsoCurves = (ddPSC_IsoparametricCurves *)(dstPSC + 1);
	    break;
	}

	case PEXPSCMcLevelCurves: {
	    DST_STORE_AND_COPY( dstPSC, miPSurfaceCharsStruct,
				sizeof(miPSurfaceCharsStruct)
				+ sizeof(ddPSC_LevelCurves));
	    dstPSC->data.pMcLevelCurves = (ddPSC_LevelCurves *)(dstPSC + 1);
	    break;
	}

	case PEXPSCWcLevelCurves: {
	    DST_STORE_AND_COPY( dstPSC, miPSurfaceCharsStruct,
				sizeof(miPSurfaceCharsStruct)
				+ sizeof(ddPSC_LevelCurves));
	    dstPSC->data.pWcLevelCurves = (ddPSC_LevelCurves *)(dstPSC + 1);
	    break;
	}
    }

    return(Success);
}




OC_COPY_FUNC_HEADER(Gdp2D)
{
    COPY_DECL(Gdp, miGdpStruct);

    DST_STORE_AND_COPY(	dstGdp, miGdpStruct, (sizeof(miGdpStruct)
			+ sizeof(listofddPoint) + srcGdp->numBytes
			+ srcGdp->points.ddList->numPoints * sizeof(ddCoord2D)));
    dstGdp->points.ddList = (listofddPoint *)(dstGdp+1);
    dstGdp->points.ddList->pts.p2Dpt = (ddCoord2D *)((dstGdp->points.ddList) +1);
    dstGdp->pData = ((ddUCHAR *)(dstGdp->points.ddList))
		    + srcGdp->points.ddList->numPoints * sizeof(ddCoord2D);

    return(Success);
}


OC_COPY_FUNC_HEADER(Gdp)
{
    COPY_DECL(Gdp, miGdpStruct);

    DST_STORE_AND_COPY(	dstGdp, miGdpStruct, (sizeof(miGdpStruct)
			+ sizeof(listofddPoint) + srcGdp->numBytes
			+ srcGdp->points.ddList->numPoints * sizeof(ddCoord3D)));
    dstGdp->points.ddList = (listofddPoint *)(dstGdp+1);
    dstGdp->points.ddList->pts.p3Dpt = (ddCoord3D *)((dstGdp->points.ddList) +1);
    dstGdp->pData = ((ddUCHAR *)(dstGdp->points.ddList))
		    + srcGdp->points.ddList->numPoints * sizeof(ddCoord3D);

    return(Success);
}


OC_COPY_FUNC_HEADER(SetAttribute)
{
    COPY_DECL(Attrib, pexElementInfo);

    DST_STORE_AND_COPY(	dstAttrib, pexElementInfo,
			srcAttrib->length * sizeof(CARD32));

    return(Success);
}


OC_COPY_FUNC_HEADER(PropOC)
{
    COPY_DECL(PropOC, pexElementInfo);

    DST_STORE_AND_COPY(	dstPropOC, pexElementInfo,
			srcPropOC->length * sizeof(CARD32));

    return(Success);
}
