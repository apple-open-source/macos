/* $Xorg: miInquire.c,v 1.6 2001/02/09 02:04:09 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miInquire.c,v 3.8 2001/12/14 19:57:25 dawes Exp $ */


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
	opposites of parse, although since we know in advance (from storing)
	the size of the original OC, we only need to one memory allocation
	per OC, and since all output is written to the pex buffer, only
	checking for sufficient size and reallocing is necessary
 */

/*
    Please note that any routines added to this file may also cause
    a corresponding modification to the level function tables (miTables.c)
 */

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define CAT(a,b)    a##b
#else
#define CAT(a,b)    a/**/b
#endif

#define OC_INQ_FUNC_HEADER(suffix)		    \
    ddpex2rtn CAT(inquire,suffix)(pExecuteOC, pBuf, ppPEXOC) \
    miGenericElementStr	*pExecuteOC;		/* internal format */ \
    ddBuffer		*pBuf; \
    pexElementInfo	**ppPEXOC;		/* PEX format */
    
#define COPY_PTR(PTR)  ddPointer    PTR;

#define LEN_WO_HEADER(OCtype) (pPEXOC->length * sizeof(CARD32) - sizeof(OCtype))

#define GET_INQ_STORAGE(PEX_ST, PEX_TYPE, DD_ST, DD_TYPE) \
    if (PU_BUF_TOO_SMALL( pBuf, (pExecuteOC->element.pexOClength)<<2)) { \
       if ((puBuffRealloc(pBuf, (pExecuteOC->element.pexOClength)<<2)) != Success) {\
	    return (BadAlloc); } } \
    *ppPEXOC = (pexElementInfo *)(pBuf->pBuf); \
    (*ppPEXOC)->elementType = pExecuteOC->element.elementType; \
    (*ppPEXOC)->length = pExecuteOC->element.pexOClength; \
    (PEX_ST) = (PEX_TYPE *)(*ppPEXOC); \
    DD_ST  = (DD_TYPE *)(pExecuteOC + 1);

#define GET_MORE_STORAGE(DD_ST, TYPE, SIZE) \
    (DD_ST) = (TYPE *)xalloc((unsigned long)(SIZE)); \
    if (!(DD_ST)) return (BadAlloc); 





static void
InqFacetOptData(pFacetList, ptr, rcolourType, rfacetMask, rptr)
listofddFacet	    *pFacetList;
ddPointer	    ptr;
INT16		    *rcolourType;   /* out */
CARD16		    *rfacetMask;    /* out */
ddPointer	    *rptr;	    /* out */
{
    switch (pFacetList->type) {

	case DD_FACET_INDEX_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXIndexedColour;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddIndexNormal,
			    	pFacetList->facets.pFacetIndexN, ptr); 
	    break; }

	case DD_FACET_RGBFLOAT_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXRgbFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgbFloatNormal,
				pFacetList->facets.pFacetRgbFloatN, ptr);
	    break; }

	case DD_FACET_CIE_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXCieFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddCieNormal,
				pFacetList->facets.pFacetCieN, ptr);
	    break; }

	case DD_FACET_HSV_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXHsvFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddHsvNormal,
				pFacetList->facets.pFacetHsvN, ptr);
	    break; }

	case DD_FACET_HLS_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXHlsFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddHlsNormal,
				pFacetList->facets.pFacetHlsN, ptr);
	    break; }

	case DD_FACET_RGB8_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXRgb8Colour  ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgb8Normal,
				pFacetList->facets.pFacetRgb8N, ptr);
	    break; }

	case DD_FACET_RGB16_NORM: {
	    *rfacetMask = PEXGAColour | PEXGANormal;
	    *rcolourType = PEXRgb16Colour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgb16Normal,
				pFacetList->facets.pFacetRgb16N, ptr);
	    break; }

	
	case DD_FACET_NORM: {
	    *rfacetMask = PEXGANormal;
	    /* return an out of range value instead of 0 */
	    *rcolourType = 666;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddVector3D,
				pFacetList->facets.pFacetN, ptr);
	    break; }

	case DD_FACET_INDEX: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXIndexedColour;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddIndexedColour, 
				pFacetList->facets.pFacetIndex, ptr); 

	    break; }

	case DD_FACET_RGBFLOAT: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXRgbFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgbFloatColour, 
				pFacetList->facets.pFacetRgbFloat, ptr);
	    break; }

	case DD_FACET_CIE: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXCieFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddCieColour, 
				pFacetList->facets.pFacetCie, ptr);
	    break; }

	case DD_FACET_HSV: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXHsvFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddHsvColour, 
				pFacetList->facets.pFacetHsv, ptr);
	    break; }

	case DD_FACET_HLS: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXHlsFloatColour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddHlsColour, 
				pFacetList->facets.pFacetHls, ptr);
	    break; }

	case DD_FACET_RGB8: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXRgb8Colour  ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgb8Colour, 
				pFacetList->facets.pFacetRgb8, ptr);
	    break; }

	case DD_FACET_RGB16: {
	    *rfacetMask = PEXGAColour;
	    *rcolourType = PEXRgb16Colour ;
	    PACK_LISTOF_STRUCT(	pFacetList->numFacets, ddRgb16Colour, 
				pFacetList->facets.pFacetRgb16, ptr);
	    break; }
		
	case DD_FACET_NONE: {
	    /* neither Colour nor Normal specified */
	    *rfacetMask = 0;
	    /* return an out of range value instead of 0 */
	    *rcolourType = 666;
	    break; }
    }
    
    *rptr = ptr;
}


static void
InqVertexData(pVertexList, point_type, ptr, rcolourType, rvertexMask, rptr)
listofddPoint	    *pVertexList;
ddPointType	    point_type;
ddPointer	    ptr;
INT16		    *rcolourType;   /* out */
CARD16		    *rvertexMask;   /* out */
ddPointer	    *rptr;	    /* out */
{
    switch (point_type) {
	case DD_INDEX_NORM_EDGE_POINT: {
	    *rcolourType = PEXIndexedColour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddIndexNormEdgePoint,
				pVertexList->pts.pIndexNEpt, ptr);
	    break; }
		
	case DD_RGBFLOAT_NORM_EDGE_POINT: {
	    *rcolourType = PEXRgbFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgbFloatNormEdgePoint,
				pVertexList->pts.pRgbFloatNEpt, ptr);
	    break; }

	case DD_CIE_NORM_EDGE_POINT: {
	    *rcolourType = PEXCieFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddCieNormEdgePoint,
				pVertexList->pts.pCieNEpt, ptr);
	    break; }

	case DD_HSV_NORM_EDGE_POINT: {
	    *rcolourType = PEXHsvFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHsvNormEdgePoint,
				pVertexList->pts.pHsvNEpt, ptr);
	    break; }

	case DD_HLS_NORM_EDGE_POINT: {
	    *rcolourType = PEXHlsFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHlsNormEdgePoint,
				pVertexList->pts.pHlsNEpt, ptr);
	    break; }

	case DD_RGB8_NORM_EDGE_POINT: {
	    *rcolourType = PEXRgb8Colour ;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb8NormEdgePoint,
				pVertexList->pts.pRgb8NEpt, ptr);
	    break; }

	case DD_RGB16_NORM_EDGE_POINT: {
	    *rcolourType = PEXRgb16Colour;
	    *rvertexMask = PEXGAColour | PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb16NormEdgePoint,
				pVertexList->pts.pRgb16NEpt, ptr);
	    break; }
	
	case DD_NORM_EDGE_POINT: {
	   /* take this out to prevent overwrite of valid facet colortype
	    *rcolourType = 0;
	   */
	    *rvertexMask = PEXGANormal | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddNormEdgePoint, 
				pVertexList->pts.pNEpt, ptr);
	    break; }

	case DD_INDEX_NORM_POINT: {
	    *rcolourType = PEXIndexedColour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddIndexNormalPoint,
				pVertexList->pts.pIndexNpt, ptr);
	    break; }
		
	case DD_RGBFLOAT_NORM_POINT: {
	    *rcolourType = PEXRgbFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgbFloatNormalPoint,
				pVertexList->pts.pRgbFloatNpt, ptr);
	    break; }

	case DD_CIE_NORM_POINT: {
	    *rcolourType = PEXCieFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddCieNormalPoint,
				pVertexList->pts.pCieNpt, ptr);
	    break; }

	case DD_HSV_NORM_POINT: {
	    *rcolourType = PEXHsvFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHsvNormalPoint,
				pVertexList->pts.pHsvNpt, ptr);
	    break; }

	case DD_HLS_NORM_POINT: {
	    *rcolourType = PEXHlsFloatColour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHlsNormalPoint,
				pVertexList->pts.pHlsNpt, ptr);
	    break; }

	case DD_RGB8_NORM_POINT: {
	    *rcolourType = PEXRgb8Colour ;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb8NormalPoint,
				pVertexList->pts.pRgb8Npt, ptr);
	    break; }

	case DD_RGB16_NORM_POINT: {
	    *rcolourType = PEXRgb16Colour;
	    *rvertexMask = PEXGAColour | PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb16NormalPoint,
				pVertexList->pts.pRgb16Npt, ptr);
	    break; }
	
	case DD_NORM_POINT: {
	   /* take this out to prevent overwrite of valid facet colortype
	    *rcolourType = 0;
	   */
	    *rvertexMask = PEXGANormal;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddNormalPoint, 
				pVertexList->pts.pNpt, ptr);
	    break; }

	case DD_INDEX_EDGE_POINT: {
	    *rcolourType = PEXIndexedColour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddIndexEdgePoint, 
				    pVertexList->pts.pIndexEpt, ptr);
	    break; }

	case DD_RGBFLOAT_EDGE_POINT: {
	    *rcolourType = PEXRgbFloatColour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgbFloatEdgePoint, 
				    pVertexList->pts.pRgbFloatEpt, ptr);
	    break; }

	case DD_CIE_EDGE_POINT: {
	    *rcolourType = PEXCieFloatColour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddCieEdgePoint, 
				    pVertexList->pts.pCieEpt, ptr);
	    break; }

	case DD_HSV_EDGE_POINT: {
	    *rcolourType = PEXHsvFloatColour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHsvEdgePoint, 
				    pVertexList->pts.pHsvEpt, ptr);
	    break; }

	case DD_HLS_EDGE_POINT: {
	    *rcolourType = PEXHlsFloatColour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHlsEdgePoint, 
				    pVertexList->pts.pHlsEpt, ptr);
	    break; }

	case DD_RGB8_EDGE_POINT: {
	    *rcolourType = PEXRgb8Colour ;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb8EdgePoint, 
				    pVertexList->pts.pRgb8Ept, ptr);
	    break; }

	case DD_RGB16_EDGE_POINT: {
	    *rcolourType = PEXRgb16Colour;
	    *rvertexMask = PEXGAColour | PEXGAEdges;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb16EdgePoint, 
				    pVertexList->pts.pRgb16Ept, ptr);
	    break; }
		
	case DD_EDGE_POINT: {
	    /* neither Colour nor Normal specified */
	   /* take this out to prevent overwrite of valid facet colortype
	    *rcolourType = 0;
	   */
	    *rvertexMask = 0;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddEdgePoint,
				pVertexList->pts.pEpt, ptr);
	    break; }
	case DD_INDEX_POINT: {
	    *rcolourType = PEXIndexedColour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddIndexPoint, 
				    pVertexList->pts.pIndexpt, ptr);
	    break; }

	case DD_RGBFLOAT_POINT: {
	    *rcolourType = PEXRgbFloatColour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgbFloatPoint, 
				    pVertexList->pts.pRgbFloatpt, ptr);
	    break; }

	case DD_CIE_POINT: {
	    *rcolourType = PEXCieFloatColour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddCiePoint, 
				    pVertexList->pts.pCiept, ptr);
	    break; }

	case DD_HSV_POINT: {
	    *rcolourType = PEXHsvFloatColour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHsvPoint, 
				    pVertexList->pts.pHsvpt, ptr);
	    break; }

	case DD_HLS_POINT: {
	    *rcolourType = PEXHlsFloatColour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddHlsPoint, 
				    pVertexList->pts.pHlspt, ptr);
	    break; }

	case DD_RGB8_POINT: {
	    *rcolourType = PEXRgb8Colour ;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb8Point, 
				    pVertexList->pts.pRgb8pt, ptr);
	    break; }

	case DD_RGB16_POINT: {
	    *rcolourType = PEXRgb16Colour;
	    *rvertexMask = PEXGAColour;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddRgb16Point, 
				    pVertexList->pts.pRgb16pt, ptr);
	    break; }
		
	case DD_3D_POINT: {
	    /* neither Colour nor Normal specified */
	   /* take this out to prevent overwrite of valid facet colortype
	    *rcolourType = 0;
	   */
	    *rvertexMask = 0;
	    PACK_LISTOF_STRUCT(	pVertexList->numPoints, ddCoord3D,
				pVertexList->pts.p3Dpt, ptr);
	    break; }
    }

    *rptr = ptr;
    if (DD_IsVertEdge(point_type)) *rvertexMask |= PEXGAEdges;
}


OC_INQ_FUNC_HEADER(ColourOC)
{
    pexMarkerColour	*pColour;
    miColourStruct	*ddColour;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pColour, pexMarkerColour, ddColour, miColourStruct);
    ptr = (ddPointer)&(pColour->colourSpec);
    PACK_CARD16(ddColour->colourType, ptr);
    SKIP_PADDING(ptr,2);

    switch (ddColour->colourType) {
	case PEXIndexedColour: {
	    PACK_STRUCT(pexIndexedColour, ddColour->colour.pIndex, ptr);
	    break; }
	case PEXRgbFloatColour : {
	    PACK_STRUCT(pexRgbFloatColour, ddColour->colour.pRgbFloat, ptr);
	    break; }
	case PEXCieFloatColour : {
	    PACK_STRUCT(pexCieColour, ddColour->colour.pCie, ptr);
	    break; }
	case PEXHsvFloatColour : {
	    PACK_STRUCT(pexHsvColour, ddColour->colour.pHsv, ptr);
	    break; }
	case PEXHlsFloatColour : {
	    PACK_STRUCT(pexHlsColour, ddColour->colour.pHls, ptr);
	    break; }
	case PEXRgb8Colour  : {
	    PACK_STRUCT(pexRgb8Colour, ddColour->colour.pRgb8, ptr);
	    break; }
	case PEXRgb16Colour : {
	    PACK_STRUCT(pexRgb16Colour, ddColour->colour.pRgb16, ptr);
	    break; }
    }
    
    return(Success);
}

OC_INQ_FUNC_HEADER(ColourIndexOC)
{
    miColourStruct	    *ddColour;
    pexMarkerColourIndex    *pColour;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(pColour, pexMarkerColourIndex, ddColour, miColourStruct);
    ptr = (ddPointer)&(pColour->index);
    PACK_CARD16(ddColour->colour.pIndex->index,ptr);
    
    return(Success);
}

OC_INQ_FUNC_HEADER(LightState)
{
    miLightStateStruct  *ddLightState;
    pexLightState	*pLightState;
    ddUSHORT		i, *pi;
    COPY_PTR(ptr);
    
    GET_INQ_STORAGE(pLightState, pexLightState, ddLightState,
		    miLightStateStruct);
    ptr = (ddPointer)(((pexElementInfo *)pLightState)+1);
    PACK_CARD16(ddLightState->enableList->numObj, ptr);
    PACK_CARD16(ddLightState->disableList->numObj, ptr);

        for (   i=0, pi=(ddUSHORT *)(ddLightState->enableList->pList);
	    i<ddLightState->enableList->numObj;
	    i++, pi++ ) {
		PACK_CARD16(*pi, ptr);
	    }

    SKIP_PADDING(ptr, ((ddLightState->enableList->numObj) %2)*sizeof(CARD16));

    for (   i=0, pi=(ddUSHORT *)(ddLightState->disableList->pList);
	    i<ddLightState->disableList->numObj;
	    i++, pi++ ) {
		PACK_CARD16(*pi, ptr);
	    }

    return(Success);
}

OC_INQ_FUNC_HEADER(MCVolume)
{

    miMCVolume_Struct   *ddMCVolume;
    pexModelClipVolume  *pMCVolume;

    ddHalfSpace         *ddHS;
    pexHalfSpace	*pHS;
    ddUSHORT 		i;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pMCVolume, pexModelClipVolume,
                        ddMCVolume, miMCVolume_Struct);
    ptr = (ddPointer)(((pexElementInfo *)pMCVolume)+1);
    PACK_CARD16(ddMCVolume->operator, ptr);
    PACK_CARD16(ddMCVolume->halfspaces->numObj, ptr);

    for (   i=0, ddHS=(ddHalfSpace *)(ddMCVolume->halfspaces->pList);
            i < ddMCVolume->halfspaces->numObj; i++) {

      PACK_COORD3D(&ddHS->orig_point, ptr);
      PACK_VECTOR3D(&ddHS->orig_vector, ptr);
      ddHS++; 
    }

    return(Success);
}


OC_INQ_FUNC_HEADER(MCVolume2D)
{
 
    miMCVolume_Struct	*ddMCVolume2D; 
    pexModelClipVolume2D  *pMCVolume2D;

    ddHalfSpace         *ddHS;
    pexHalfSpace2D	*pHS;
    int			i;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(pMCVolume2D, pexModelClipVolume2D,
                        ddMCVolume2D, miMCVolume_Struct)
    ptr = (ddPointer)(((pexElementInfo *)pMCVolume2D)+1);
    PACK_CARD16(ddMCVolume2D->operator, ptr);	
    PACK_CARD16(ddMCVolume2D->halfspaces->numObj, ptr);

    for (i=0, ddHS=(ddHalfSpace *)(ddMCVolume2D->halfspaces->pList),
	 pHS = (pexHalfSpace2D *)(ptr);
            i < ddMCVolume2D->halfspaces->numObj; i++) {

      PACK_COORD2D(&(ddHS->orig_point), ptr);
      PACK_VECTOR2D(&(ddHS->orig_vector), ptr);
      ddHS++;
    }

    return(Success);
}


OC_INQ_FUNC_HEADER(Marker)
{
    miMarkerStruct  *ddMarker;
    pexMarker	    *pMarker;
    COPY_PTR(ptr);
    
    GET_INQ_STORAGE( pMarker, pexMarker, ddMarker, miListHeader);
    ptr = (ddPointer)(pMarker + 1); 
    PACK_LISTOF_COORD3D(    ddMarker->ddList->numPoints,
			    ddMarker->ddList->pts.p3Dpt, ptr);

    return(Success);
}


OC_INQ_FUNC_HEADER(Marker2D)
{
    miMarkerStruct  *ddMarker;
    pexMarker2D	    *pMarker;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pMarker, pexMarker2D, ddMarker, miListHeader);
    ptr = (ddPointer)(pMarker + 1); 
    PACK_LISTOF_COORD2D(    ddMarker->ddList->numPoints,
			    ddMarker->ddList->pts.p2Dpt, ptr);

    return(Success);
}


OC_INQ_FUNC_HEADER(Text)
{
    miTextStruct    *ddText;
    pexText	    *pText;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pText, pexText, ddText, miTextStruct);
    ptr = (ddPointer)(((pexElementInfo *)pText)+1);
    PACK_COORD3D( ddText->pOrigin, ptr);
    PACK_COORD3D( ddText->pDirections, ptr);
    PACK_COORD3D( (ddText->pDirections) + 1, ptr);
    PACK_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    memcpy( (char *)ptr, (char *)(ddText->pText), 
	    (int)(  sizeof(CARD32) * pText->head.length
		    - sizeof(pexText)));
    
    return(Success);
}
 

OC_INQ_FUNC_HEADER(Text2D)
{
    miText2DStruct  *ddText;
    pexText2D	    *pText;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pText, pexText2D, ddText, miText2DStruct);
    ptr = (ddPointer)(((pexElementInfo *)pText)+1);
    PACK_COORD2D( ddText->pOrigin, ptr);
    PACK_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    memcpy(  (char *)ptr, (char *)(ddText->pText), 
	    (int)( sizeof(CARD32) * pText->head.length
		    - sizeof(pexText2D)));

    return(Success);
}


OC_INQ_FUNC_HEADER(AnnotationText)
{
    miAnnoTextStruct	*ddText;
    pexAnnotationText	*pText;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(pText, pexAnnotationText, ddText, miAnnoTextStruct);
    ptr = (ddPointer)(((pexElementInfo *)pText)+1);
    PACK_COORD3D( ddText->pOrigin, ptr);
    PACK_COORD3D( ddText->pOffset, ptr);
    PACK_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    memcpy(  (char *)ptr, (char *)(ddText->pText), 
	    (int)( sizeof(CARD32) * pText->head.length
		   - sizeof(pexAnnotationText)));

    return(Success);
}


OC_INQ_FUNC_HEADER(AnnotationText2D)
{
    miAnnoText2DStruct  *ddText;
    pexAnnotationText2D	*pText;
    COPY_PTR(ptr);
 
    GET_INQ_STORAGE(pText, pexAnnotationText2D, ddText, miAnnoText2DStruct);
    ptr = (ddPointer)(((pexElementInfo *)pText)+1);
    PACK_COORD2D( ddText->pOrigin, ptr);
    PACK_COORD2D( ddText->pOffset, ptr);
    PACK_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    memcpy(  (char *)ptr, (char *)(ddText->pText), 
	    (int)(  sizeof(CARD32) * pText->head.length
		    - sizeof(pexAnnotationText2D)));

    return(Success);
}


OC_INQ_FUNC_HEADER(Polyline2D)
{
    miPolylineStruct    *ddPoly;
    pexPolyline2D	*pPoly;
    listofddPoint	*ddPoint;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pPoly, pexPolyline2D, ddPoly, miListHeader);
    ptr = (ddPointer)(pPoly + 1); 
    ddPoint           = (listofddPoint *)(ddPoly+1);
    PACK_LISTOF_COORD2D(ddPoint->numPoints,ddPoint->pts.p2Dpt,ptr);

    return(Success);
}



OC_INQ_FUNC_HEADER(Polyline)
{
    miPolylineStruct    *ddPoly;
    pexPolyline		*pPoly;
    listofddPoint	*ddPoint;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pPoly, pexPolyline, ddPoly, miListHeader);
    ptr = (ddPointer)(pPoly + 1); 
    ddPoint           = (listofddPoint *)(ddPoly+1);
    PACK_LISTOF_COORD3D(ddPoint->numPoints,ddPoint->pts.p3Dpt,ptr);

    return(Success);
}


OC_INQ_FUNC_HEADER(PolylineSet)
{
    miPolylineStruct    *ddPoly;
    pexPolylineSet	*pPoly;
    listofddPoint	*ddPoint;
    ddUSHORT		i;
    ddPointer		rptr = 0;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pPoly, pexPolylineSet, ddPoly, miListHeader);
    ptr = (ddPointer)(pPoly + 1); 

    for (i=0, ddPoint = (listofddPoint *)(ddPoly+1);
	 i<ddPoly->numLists;
	 i++, ddPoint++) {

	PACK_CARD32(ddPoint->numPoints, ptr);
	InqVertexData(  ddPoint, ddPoly->type, ptr, &(pPoly->colourType),
			&(pPoly->vertexAttribs), &rptr);
	ptr = rptr;
    }

    pPoly->numLists = ddPoly->numLists;
    return(Success);
}


OC_INQ_FUNC_HEADER(NurbCurve)
{
    miNurbStruct    *ddNurb;
    pexNurbCurve    *pNurb;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pNurb, pexNurbCurve, ddNurb, miNurbStruct);
    ptr = (ddPointer)(((pexElementInfo *)pNurb)+1);
    PACK_CARD16(ddNurb->order, ptr);
    SKIP_PADDING(ptr, 2);	    /* type place holder (see below) */
    PACK_FLOAT(ddNurb->uMin, ptr);
    PACK_FLOAT(ddNurb->uMax, ptr);
    PACK_CARD32(ddNurb->numKnots, ptr);
    PACK_CARD32(ddNurb->points.ddList->numPoints, ptr);

    PACK_LISTOF_STRUCT(	ddNurb->numKnots, PEXFLOAT, ddNurb->pKnots, ptr);

    if (ddNurb->points.type == DDPT_4D) {
	pNurb->coordType = PEXRational;
	PACK_LISTOF_STRUCT( ddNurb->points.ddList->numPoints, ddCoord4D,
			    ddNurb->points.ddList->pts.p4Dpt, ptr);
    } else {
	pNurb->coordType = PEXNonRational;
	PACK_LISTOF_STRUCT( ddNurb->points.ddList->numPoints, ddCoord3D, 
			    ddNurb->points.ddList->pts.p3Dpt, ptr);
    }

    return(Success);
}


OC_INQ_FUNC_HEADER(FillArea2D)
{
    miFillAreaStruct	*ddFill;
    pexFillArea2D	*pFill;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pFill, pexFillArea2D, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    SKIP_PADDING(ptr, 1);

    PACK_LISTOF_COORD2D(    ddFill->points.ddList->numPoints,
			    ddFill->points.ddList->pts.p2Dpt,ptr);

    return(Success);
}


 

OC_INQ_FUNC_HEADER(FillArea)
{
    miFillAreaStruct	*ddFill;
    pexFillArea		*pFill;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pFill, pexFillArea, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    SKIP_PADDING(ptr, 1);
    
    PACK_LISTOF_COORD3D(    ddFill->points.ddList->numPoints,
			    ddFill->points.ddList->pts.p3Dpt, ptr);

    return(Success);
}


OC_INQ_FUNC_HEADER(ExtFillArea)
{
    miFillAreaStruct	*ddFill;
    pexExtFillArea	*pFill;
    ddPointer		rptr = 0;
    COPY_PTR(ptr);
    
    GET_INQ_STORAGE( pFill, pexExtFillArea, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    
    ptr = (ddPointer)(pFill+1);
    PACK_CARD32(ddFill->points.ddList->numPoints, ptr);
    InqFacetOptData(	ddFill->pFacets, ptr, &(pFill->colourType),
			&(pFill->facetAttribs), &rptr);
    ptr = rptr;

    InqVertexData(  ddFill->points.ddList, ddFill->points.type, ptr,
		    &(pFill->colourType), &(pFill->vertexAttribs), &rptr);
    ptr = rptr;

    return(Success);
}


OC_INQ_FUNC_HEADER(FillAreaSet2D)
{
    miFillAreaStruct	*ddFill;
    pexFillAreaSet2D	*pFill;
    ddULONG		 i;
    listofddPoint	*ddPoint;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pFill, pexFillAreaSet2D, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    PACK_CARD8(ddFill->contourHint, ptr);

    PACK_CARD32(ddFill->points.numLists, ptr);

    for (i=0, ddPoint = ddFill->points.ddList;
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {

	PACK_CARD32(ddPoint->numPoints, ptr);
	PACK_LISTOF_STRUCT( ddPoint->numPoints, ddCoord2D,
			    ddPoint->pts.p2Dpt, ptr);
    }

    return(Success);
}


OC_INQ_FUNC_HEADER(FillAreaSet)
{
    miFillAreaStruct	*ddFill;
    pexFillAreaSet	*pFill;
    ddULONG		 i;
    listofddPoint	*ddPoint;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pFill, pexFillAreaSet, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    PACK_CARD8(ddFill->contourHint, ptr);

    PACK_CARD32(ddFill->points.numLists, ptr);

    for (i=0, ddPoint = ddFill->points.ddList;
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {

	PACK_CARD32(	ddPoint->numPoints, ptr);
	PACK_LISTOF_STRUCT( ddPoint->numPoints, ddCoord3D,
			    ddPoint->pts.p3Dpt, ptr);
    }

    return(Success);
}


OC_INQ_FUNC_HEADER(ExtFillAreaSet)
{
    miFillAreaStruct	*ddFill;
    pexExtFillAreaSet	*pFill;
    ddULONG		i;
    listofddPoint	*ddPoint;
    ddPointer		rptr = 0;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(pFill, pexExtFillAreaSet, ddFill, miFillAreaStruct);
    ptr = (ddPointer)(((pexElementInfo *)pFill)+1);
    PACK_CARD16(ddFill->shape, ptr);
    PACK_CARD8(ddFill->ignoreEdges, ptr);
    PACK_CARD8(ddFill->contourHint, ptr);
    pFill->numLists = ddFill->points.numLists;
    ptr = (ddPointer)(pFill+1);

    InqFacetOptData(	ddFill->pFacets, ptr, &(pFill->colourType),
			&(pFill->facetAttribs), &rptr);
    ptr = rptr;

    for (i=0, ddPoint = ddFill->points.ddList;
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {

	PACK_CARD32(	ddPoint->numPoints, ptr);
	InqVertexData(	ddPoint, ddFill->points.type, ptr, &(pFill->colourType),
			&(pFill->vertexAttribs), &rptr);
	ptr = rptr;
    }
    
    return(Success);
}


OC_INQ_FUNC_HEADER(SOFAS)
{
    miSOFASStruct   *ddFill;
    pexSOFAS	    *pFill;
    CARD16 i,j;
    miConnListList  *pCLL;
    miConnList	    *pCList;
    ddPointer	    rptr = 0;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pFill, pexSOFAS, ddFill, miSOFASStruct);
    pFill->shape = ddFill->shape;
    pFill->edgeAttributes = ddFill->edgeAttribs;
    pFill->contourHint = ddFill->contourHint;
    pFill->contourCountsFlag = ddFill->contourCountsFlag;
    pFill->numFAS = ddFill->numFAS;
    pFill->numVertices = ddFill->points.ddList->numPoints;
    pFill->numEdges = ddFill->numEdges;
    pFill->numContours = ddFill->connects.numListLists;

    ptr = (ddPointer)(pFill+1);
    InqFacetOptData(	&(ddFill->pFacets), ptr, &(pFill->colourType),
			&(pFill->FAS_Attributes), &rptr);
    ptr = rptr;

    InqVertexData(  ddFill->points.ddList, ddFill->points.type, ptr,
		    &(pFill->colourType), &(pFill->vertexAttributes), &rptr);
    ptr = rptr;

    if (pFill->edgeAttributes) {
	PACK_LISTOF_STRUCT(ddFill->numEdges, ddUCHAR, ddFill->edgeData, ptr);
	SKIP_PADDING(ptr,((((int)((pFill->numEdges + 3)/4))*4)));
    }

    for (i=0, pCLL = ddFill->connects.data; i<pFill->numContours; i++, pCLL++) {
	PACK_CARD16(pCLL->numLists,ptr);
	for (j=0, pCList=pCLL->pConnLists; j<pCLL->numLists; j++, pCList++) {
	    PACK_CARD16(pCList->numLists,ptr);
	    PACK_LISTOF_STRUCT(	pCList->numLists, ddUSHORT,
				pCList->pConnects, ptr);
	}
    }

    return(Success);
}



OC_INQ_FUNC_HEADER(TriangleStrip)
{
    miTriangleStripStruct   *ddTriangle;
    pexTriangleStrip	    *pTriangle;
    ddPointer		    rptr = 0;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(	pTriangle, pexTriangleStrip, ddTriangle,
			miTriangleStripStruct);
    pTriangle->numVertices = ddTriangle->points.ddList->numPoints;
    ptr = (ddPointer)(pTriangle +1);

    InqFacetOptData(	ddTriangle->pFacets, ptr, &(pTriangle->colourType), 
			&(pTriangle->facetAttribs), &rptr);
    ptr = rptr;

    InqVertexData(  ddTriangle->points.ddList, ddTriangle->points.type, ptr,
		    &(pTriangle->colourType), &(pTriangle->vertexAttribs),
		    &rptr);
    ptr = rptr;

    return(Success);
}


OC_INQ_FUNC_HEADER(QuadrilateralMesh)
{
    miQuadMeshStruct	    *ddQuad;
    pexQuadrilateralMesh    *pQuad;
    ddPointer		    rptr = 0;
    COPY_PTR(ptr);

    GET_INQ_STORAGE(pQuad, pexQuadrilateralMesh, ddQuad, miQuadMeshStruct);
    pQuad->mPts = ddQuad->mPts;
    pQuad->nPts = ddQuad->nPts;
    pQuad->shape = ddQuad->shape;
    ptr = (ddPointer)(pQuad+1);
    
    InqFacetOptData(	ddQuad->pFacets, ptr, &(pQuad->colourType), 
			&(pQuad->facetAttribs), &rptr);
    ptr = rptr;

    InqVertexData(  ddQuad->points.ddList, ddQuad->points.type, ptr,
		    &(pQuad->colourType), &(pQuad->vertexAttribs), &rptr);
    ptr = rptr;

    return(Success);
}


OC_INQ_FUNC_HEADER(NurbSurface)
{
    miNurbSurfaceStruct	*ddNurb;
    pexNurbSurface	*pNurb;
    ddULONG		 i, j;
    listofTrimCurve	*ddTrim;
    ddTrimCurve		*ddTC;
    ddUSHORT		type;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pNurb, pexNurbSurface, ddNurb, miNurbSurfaceStruct);
    ptr = (ddPointer)(((pexElementInfo *)pNurb)+1);

    SKIP_PADDING(ptr, 2);		/* type place holder (see below) */
    PACK_CARD16(ddNurb->uOrder, ptr);
    PACK_CARD16(ddNurb->vOrder, ptr);
    SKIP_PADDING(ptr, 2);
    PACK_CARD32(ddNurb->numUknots, ptr);
    PACK_CARD32(ddNurb->numVknots, ptr);
    PACK_CARD16(ddNurb->mPts, ptr);
    PACK_CARD16(ddNurb->nPts, ptr);
    PACK_CARD32(ddNurb->numTrimCurveLists, ptr);

    PACK_LISTOF_STRUCT(ddNurb->numUknots, PEXFLOAT, ddNurb->pUknots, ptr);
    PACK_LISTOF_STRUCT(ddNurb->numVknots, PEXFLOAT, ddNurb->pVknots, ptr);

    i = ddNurb->mPts * ddNurb->nPts;
    if (ddNurb->points.type == DD_HOMOGENOUS_POINT) {
	pNurb->type = PEXRational;
	PACK_LISTOF_STRUCT(i, ddCoord4D, ddNurb->points.ddList->pts.p4Dpt, ptr);
    } else {
	pNurb->type = PEXNonRational;
	PACK_LISTOF_STRUCT(i, ddCoord3D, ddNurb->points.ddList->pts.p3Dpt, ptr);
    }

    for (i=0, ddTrim = ddNurb->trimCurves;
	 i<ddNurb->numTrimCurveLists;
	 i++, ddTrim++) {

	PACK_CARD32(ddTrim->count, ptr);
	for (j=0, ddTC=ddTrim->pTC; j<ddTrim->count; j++, ddTC++) {
	    PACK_CARD8(ddTC->visibility, ptr);
	    SKIP_PADDING(ptr, 1);
	    PACK_CARD16(ddTC->order, ptr);
	    type = (ddTC->pttype = DD_3D_POINT) ? PEXRational : PEXNonRational;
	    PACK_CARD16(type, ptr);
	    PACK_INT16(ddTC->curveApprox.approxMethod, ptr);	
	    PACK_FLOAT(ddTC->curveApprox.tolerance, ptr);	
	    PACK_FLOAT(ddTC->uMin, ptr);
	    PACK_FLOAT(ddTC->uMax, ptr);
	    PACK_CARD32(ddTC->numKnots, ptr);
	    PACK_CARD32(ddTC->points.numPoints, ptr);
	    PACK_LISTOF_STRUCT(ddTC->numKnots,PEXFLOAT,ddTC->pKnots, ptr);
	    if (ddTC->pttype == DD_3D_POINT) {
		PACK_LISTOF_STRUCT( ddTC->points.numPoints, ddCoord4D,
				    ddTC->points.pts.p4Dpt, ptr);
	    } else {
		PACK_LISTOF_STRUCT( ddTC->points.numPoints, ddCoord3D,
				    ddTC->points.pts.p3Dpt, ptr);
	    }
	    
	}
    }

    return(Success);

}
 

OC_INQ_FUNC_HEADER(CellArray2D)
{
    miCellArrayStruct	*ddCell;
    pexCellArray2D	*pCell;
    int i=2;
    COPY_PTR(ptr);
    
    GET_INQ_STORAGE( pCell, pexCellArray2D, ddCell, miCellArrayStruct);
    ptr = (ddPointer)&(pCell->point1);

    PACK_LISTOF_COORD2D(i, ddCell->point.ddList->pts.p2Dpt, ptr);

    PACK_CARD32(ddCell->dx, ptr);
    PACK_CARD32(ddCell->dy, ptr);
    i = ddCell->dx * ddCell->dy;
    PACK_LISTOF_STRUCT(i, ddIndexedColour, ddCell->colours.colour.pIndex, ptr);
    
    return(Success);
}


OC_INQ_FUNC_HEADER(CellArray)
{
    miCellArrayStruct	*ddCell;
    pexCellArray	*pCell;
    int i=3;
    COPY_PTR(ptr);
    
    GET_INQ_STORAGE( pCell, pexCellArray, ddCell, miCellArrayStruct);
    ptr = (ddPointer)&(pCell->point1);

    PACK_LISTOF_COORD3D(i, ddCell->point.ddList->pts.p3Dpt, ptr);

    PACK_CARD32(ddCell->dx, ptr);
    PACK_CARD32(ddCell->dy, ptr);
    i = ddCell->dx * ddCell->dy;
    PACK_LISTOF_STRUCT(i, ddIndexedColour, ddCell->colours.colour.pIndex, ptr);
    
    return(Success);
}


OC_INQ_FUNC_HEADER(ExtCellArray)
{
    miCellArrayStruct	*ddCell;
    pexExtCellArray	*pCell;
    int i=3;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pCell, pexExtCellArray, ddCell, miCellArrayStruct);
    ptr = (ddPointer)&(pCell->colourType);

    PACK_CARD16(ddCell->colours.colourType, ptr);
    SKIP_PADDING(ptr, 2);
    
    PACK_LISTOF_COORD3D(i, ddCell->point.ddList->pts.p3Dpt, ptr);
    PACK_CARD32(ddCell->dx, ptr);
    PACK_CARD32(ddCell->dy, ptr);
    i = ddCell->dx * ddCell->dy;
    
    switch (pCell->colourType) {
	case PEXIndexedColour: {
	    PACK_LISTOF_STRUCT(	i, ddIndexedColour,
				ddCell->colours.colour.pIndex, ptr);
	    break; }
	case PEXRgbFloatColour : {
	    PACK_LISTOF_STRUCT(	i, ddRgbFloatColour,
				ddCell->colours.colour.pRgbFloat, ptr);
	    break; }
	case PEXCieFloatColour : {
	    PACK_LISTOF_STRUCT(	i, ddCieColour,
				ddCell->colours.colour.pCie, ptr);
	    break; }
	case PEXHsvFloatColour : {
	    PACK_LISTOF_STRUCT(	i, ddHsvColour, 
				ddCell->colours.colour.pHsv, ptr);
	    break; }
	case PEXHlsFloatColour : {
	    PACK_LISTOF_STRUCT(	i, ddHlsColour, 
				ddCell->colours.colour.pHls, ptr);
	    break; }
	case PEXRgb8Colour  : {
	    PACK_LISTOF_STRUCT(	i, ddRgb8Colour, 
				ddCell->colours.colour.pRgb8, ptr);
	    break; }
	case PEXRgb16Colour : {
	    PACK_LISTOF_STRUCT(	i, ddRgb16Colour, 
				ddCell->colours.colour.pRgb16, ptr);
	    break; }
    }

    return(Success);
    
}
 

OC_INQ_FUNC_HEADER(PSurfaceChars)
{
    miPSurfaceCharsStruct	*ddPSC = (miPSurfaceCharsStruct *)(pExecuteOC+1);
    pexParaSurfCharacteristics	*pPSC;
    COPY_PTR(ptr);

    switch (ddPSC->type) {
	case PEXPSCNone:
	case PEXPSCImpDep:
	    GET_INQ_STORAGE(	pPSC, pexParaSurfCharacteristics, ddPSC,
				miPSurfaceCharsStruct);
	    ptr = (ddPointer)(pPSC+1);
	    break;

	case PEXPSCIsoCurves: {
	    GET_INQ_STORAGE(	pPSC, pexParaSurfCharacteristics, ddPSC,
				miPSurfaceCharsStruct);
            pPSC->characteristics = ddPSC->type; 
            pPSC->length = 4 * sizeof(CARD16); 
	    ptr = (ddPointer)(pPSC+1);
	    PACK_CARD16(ddPSC->data.pIsoCurves->placementType, ptr);
	    SKIP_PADDING(ptr,2);
	    PACK_CARD16(ddPSC->data.pIsoCurves->numUcurves, ptr);
	    PACK_CARD16(ddPSC->data.pIsoCurves->numVcurves, ptr);
	    break;
	}

	case PEXPSCMcLevelCurves: {
	    GET_INQ_STORAGE(	pPSC, pexParaSurfCharacteristics, ddPSC,
				miPSurfaceCharsStruct);
            pPSC->characteristics = ddPSC->type; 
            pPSC->length = sizeof(pexCoord3D) + sizeof(pexVector3D) +
			   (2 *sizeof(CARD16)) + (sizeof(PEXFLOAT)* 
			   ddPSC->data.pMcLevelCurves->numberIntersections); 
	    ptr = (ddPointer)(pPSC+1);
	    PACK_STRUCT(pexCoord3D, &(ddPSC->data.pMcLevelCurves->origin),ptr);
	    PACK_STRUCT(pexVector3D, &(ddPSC->data.pMcLevelCurves->direction),ptr);
	    PACK_CARD16(ddPSC->data.pMcLevelCurves->numberIntersections, ptr);
	    SKIP_PADDING(ptr,2);
	    PACK_LISTOF_STRUCT(ddPSC->data.pMcLevelCurves->numberIntersections,
				PEXFLOAT, ddPSC->data.pMcLevelCurves->pPoints,
				ptr);
	    break;
	}

	case PEXPSCWcLevelCurves: {
	    GET_INQ_STORAGE(	pPSC, pexParaSurfCharacteristics, ddPSC,
				miPSurfaceCharsStruct);
            pPSC->characteristics = ddPSC->type; 
            pPSC->length = sizeof(pexCoord3D) + sizeof(pexVector3D) +
			   (2 *sizeof(CARD16)) + (sizeof(PEXFLOAT)* 
			   ddPSC->data.pWcLevelCurves->numberIntersections); 
	    ptr = (ddPointer)(pPSC+1);
	    PACK_STRUCT(pexCoord3D, &(ddPSC->data.pWcLevelCurves->origin),ptr);
	    PACK_STRUCT(pexVector3D, &(ddPSC->data.pWcLevelCurves->direction),ptr);
	    PACK_CARD16(ddPSC->data.pWcLevelCurves->numberIntersections, ptr);
	    SKIP_PADDING(ptr,2);
	    PACK_LISTOF_STRUCT(ddPSC->data.pWcLevelCurves->numberIntersections,
				PEXFLOAT, ddPSC->data.pWcLevelCurves->pPoints,
				ptr);
	    break;
	}
    }

    pPSC->characteristics = ddPSC->type;
    pPSC->length=(CARD16)((*ppPEXOC)->length-sizeof(pexParaSurfCharacteristics));
    return(Success);
}




OC_INQ_FUNC_HEADER(Gdp2D)
{
    miGdpStruct	    *ddGdp;
    pexGdp2D	    *pGdp;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pGdp, pexGdp2D, ddGdp, miGdpStruct);
    pGdp->gdpId = ddGdp->GDPid;
    pGdp->numPoints = ddGdp->points.ddList->numPoints;
    pGdp->numBytes = ddGdp->numBytes;
    ptr = (ddPointer)(pGdp + 1);
    PACK_LISTOF_COORD2D(    ddGdp->points.ddList->numPoints,
			    ddGdp->points.ddList->pts.p2Dpt, ptr);
    PACK_LISTOF_STRUCT( ddGdp->numBytes, ddUCHAR, ddGdp->pData, ptr);

    return(Success);
}


OC_INQ_FUNC_HEADER(Gdp)
{
    miGdpStruct	    *ddGdp;
    pexGdp	    *pGdp;
    COPY_PTR(ptr);

    GET_INQ_STORAGE( pGdp, pexGdp, ddGdp, miGdpStruct);
    pGdp->gdpId = ddGdp->GDPid;
    pGdp->numPoints = ddGdp->points.ddList->numPoints;
    pGdp->numBytes = ddGdp->numBytes;
    ptr = (ddPointer)(pGdp + 1);
    PACK_LISTOF_COORD3D(    ddGdp->points.ddList->numPoints,
			    ddGdp->points.ddList->pts.p3Dpt, ptr);
    PACK_LISTOF_STRUCT( ddGdp->numBytes, ddUCHAR, ddGdp->pData, ptr);

    return(Success);
    
}



OC_INQ_FUNC_HEADER(SetAttribute)
{
    /** The function vector should be set up to have this
     ** SetAttribute function as the entry for all of the OC entries other
     ** than those listed above or those NULL'd out
     **/

    ddElementInfo  *dstAttrib, *srcAttrib;

    GET_INQ_STORAGE( dstAttrib, ddElementInfo, srcAttrib, ddElementInfo);

    memcpy(  (char *)dstAttrib, (char *)srcAttrib, 
	    (int)(srcAttrib->length * sizeof(CARD32)));

    return(Success);
}

OC_INQ_FUNC_HEADER(PropOC)
{
    /** The function Handles Proprietary Vendor OCs
     **/

    ddElementInfo  *dstPropOC, *srcPropOC;

    GET_INQ_STORAGE( dstPropOC, ddElementInfo, srcPropOC, ddElementInfo);

    memcpy(  (char *)dstPropOC, (char *)srcPropOC, 
	    (int)(srcPropOC->length * sizeof(CARD32)));

    return(Success);
}

