/* $Xorg: pexOCParse.c,v 1.6 2001/02/09 02:04:11 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/pexOCParse.c,v 3.8 2001/12/14 19:57:31 dawes Exp $ */


#include "miStruct.h"
#include "ddpex2.h"
#include "pexExtract.h"
#include "pexUtils.h"
#include "miLight.h"
#include "pexos.h"


/** Parsing functions:
 **	Each function takes two parameters: a pointer to the element to be 
 **	parsed (in PEX format) and a pointer to a pointer to return the 
 **	parsed element (in server native internal format).
 **
 **	These routines may be used in most cases for both creation
 **	and replacement of structure elements.  If the second argument
 **	points to a NULL, then the memory is allocated for the creation
 **	of the structure element.  Otherwise, no new memory is
 **	allocated and the structure element is replaced in place.
 **	This scheme requires that the calling routine ensure
 **	that there is sufficient memory already allocated for replacing
 **	in place.
 **
 **	To support this scheme, parse routine writers must calculate
 **	the amount of memory required to store the element in server
 **	native format before allocating it, and pass this number of
 **	bytes as the 3rd argument to the below macro GET_DD_STORAGE.
 **	Then the parse routine can be put in both the parse and
 **	replace tables as the entry for handling the OC.
 **
 **	Note that there are 3 exceptions to this symmetry:
 **		LightState
 **		SOFAS
 **		NurbSurfaces
 **
 **	For each of these, there are separate parse and replace
 **	routines.
 **
 **	For any OC that follows the above scheme, the corresponding
 **	entry in the destroy table can be the default (destroyOC_PEX),
 **	since memory allocation is therefore in one chunk.  If the
 **	coder of a parse routine cannot write the routine so that all
 **	of the memory is allocated at once, then they must also write
 **	a special destroy routine for this element (to ensure no
 **	memory leakage).
 **
 **	Coders of parsing routines must also provide corresponding
 **	copy and inquire routines (see miCopy.c and miInquire.c).
 **/

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define CAT(a,b)    a##b
#else
#define CAT(a,b)    a/**/b
#endif

#define OC_PARSER_FUNC_HEADER(suffix)		    \
    ddpex2rtn CAT(parse,suffix)(pPEXOC, ppExecuteOC)\
    ddElementInfo	*pPEXOC;		/* PEX format */    \
    miGenericElementPtr	*ppExecuteOC;		/* internal format */
    
#define OC_PARSER_RETURN(ANSWER)        \
    return(Success);

#define PARSER_PTR(PTR)  \
    ddPointer PTR = (ddPointer)(pPEXOC + 1)

#define LEN_WO_HEADER(OCtype) (pPEXOC->length * sizeof(CARD32) - sizeof(OCtype))

#define GET_DD_STORAGE(DD_ST, TYPE, SIZE)				\
    if (*ppExecuteOC == 0) {						\
	*ppExecuteOC =							\
	    (miGenericElementPtr) xalloc((unsigned long)((SIZE)		\
					+ sizeof(miGenericElementStr)));\
      if (!(*ppExecuteOC)) return (BadAlloc);	                        \
    }			                                                \
    (DD_ST) = (TYPE *)((*ppExecuteOC)+1);

#define GET_MORE_STORAGE(DD_ST, TYPE, SIZE)			\
    if ((SIZE) > 0) {						\
	(DD_ST) = (TYPE *)xalloc((unsigned long)(SIZE));	\
	if (!(DD_ST)) err = BadAlloc; }				\
    else DD_ST = 0;


extern void freeTrimCurves();
extern void freeKnots();


/*
    Returns number of bytes required to store the indicated data
 */
int
CountFacetOptData(ptr, colourType, numFacets, facetMask)
ddPointer	    ptr;
CARD16		    colourType;
CARD32		    numFacets;
CARD16		    facetMask;
{
    ASSURE(facetMask <= 3);

    switch (facetMask) {
	case PEXGAColour | PEXGANormal : {

	    switch (colourType) {

		case PEXIndexedColour: 
			return(numFacets * sizeof(ddIndexNormal));

		case PEXRgbFloatColour :
			return(numFacets * sizeof(ddRgbFloatNormal));

		case PEXCieFloatColour : 
			return(numFacets * sizeof(ddCieNormal));

		case PEXHsvFloatColour : 
			return(numFacets * sizeof(ddHsvNormal));

		case PEXHlsFloatColour : 
			return(numFacets * sizeof(ddHlsNormal));

		case PEXRgb8Colour  : 
			return(numFacets * sizeof(ddRgb8Normal));

		case PEXRgb16Colour : 
			return(numFacets * sizeof(ddRgb16Normal));
		default:
			return(0);
	    }
	    break; }
	
	case PEXGANormal :

		return(numFacets * sizeof(ddVector3D));

	case PEXGAColour : {

	    switch (colourType) {

		case PEXIndexedColour:
			return(numFacets * sizeof(ddIndexedColour));
			/* force lword alignment for any adjacent pointers */

		case PEXRgbFloatColour : 
			return(numFacets * sizeof(ddRgbFloatColour));

		case PEXCieFloatColour : 
			return(numFacets * sizeof(ddCieColour));

		case PEXHsvFloatColour : 
			return(numFacets * sizeof(ddHsvColour));

		case PEXHlsFloatColour : 
			return(numFacets * sizeof(ddHlsColour));

		case PEXRgb8Colour  : 
			return(numFacets * sizeof(ddRgb8Colour));

		case PEXRgb16Colour : 
			return(numFacets * sizeof(ddRgb16Colour));

		default:
			return(0);
	    }
	    break; }

	case 0x0000 : 
		return(0);
	default:
		return(0);
    }
    
}


/*
    Parses facet data into already allocated memory (pFacetData)
*/
void
ParseFacetOptData(ptr, colourType, numFacets, facetMask, pFacetList, pFacetData,rptr)
ddPointer	    ptr;
CARD16		    colourType;
CARD32		    numFacets;
CARD16		    facetMask;
listofddFacet	    *pFacetList;
ddPointer	    pFacetData;
ddPointer	    *rptr;
{
    ASSURE(facetMask <= 3);

    switch (facetMask) {
	case PEXGAColour | PEXGANormal : {

	    pFacetList->numFacets = numFacets;

	    switch (colourType) {

		case PEXIndexedColour: {
		    pFacetList->type = DD_FACET_INDEX_NORM;
		    pFacetList->facets.pFacetIndexN
						= (ddIndexNormal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddIndexNormal);
		    EXTRACT_STRUCT( numFacets, ddIndexNormal,
				    pFacetList->facets.pFacetIndexN, ptr); 

		    break; }

		case PEXRgbFloatColour : {
		    pFacetList->type = DD_FACET_RGBFLOAT_NORM;
		    pFacetList->facets.pFacetRgbFloatN
					= (ddRgbFloatNormal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddRgbFloatNormal);
		    EXTRACT_STRUCT( numFacets, ddRgbFloatNormal,
				    pFacetList->facets.pFacetRgbFloatN, ptr);
		    break; }

		case PEXCieFloatColour : {
		    pFacetList->type = DD_FACET_CIE_NORM;
		    pFacetList->facets.pFacetCieN = (ddCieNormal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddCieNormal);
		    EXTRACT_STRUCT( numFacets, ddCieNormal,
				    pFacetList->facets.pFacetCieN, ptr);
		    break; }

		case PEXHsvFloatColour : {
		    pFacetList->type = DD_FACET_HSV_NORM;
		    pFacetList->facets.pFacetHsvN = (ddHsvNormal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddHsvNormal);
		    EXTRACT_STRUCT( numFacets, ddHsvNormal,
				    pFacetList->facets.pFacetHsvN, ptr);
		    break; }

		case PEXHlsFloatColour : {
		    pFacetList->type = DD_FACET_HLS_NORM;
		    pFacetList->facets.pFacetHlsN = (ddHlsNormal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddHlsNormal);
		    EXTRACT_STRUCT( numFacets, ddHlsNormal,
				    pFacetList->facets.pFacetHlsN, ptr);
		    break; }

		case PEXRgb8Colour  : {
		    pFacetList->type = DD_FACET_RGB8_NORM;
		    pFacetList->facets.pFacetRgb8N = (ddRgb8Normal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddRgb8Normal);
		    EXTRACT_STRUCT( numFacets, ddRgb8Normal,
				    pFacetList->facets.pFacetRgb8N, ptr);
		    break; }

		case PEXRgb16Colour : {
		    pFacetList->type = DD_FACET_RGB16_NORM;
		    pFacetList->facets.pFacetRgb16N
					= (ddRgb16Normal *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddRgb16Normal);
		    EXTRACT_STRUCT( numFacets, ddRgb16Normal,
				    pFacetList->facets.pFacetRgb16N, ptr);
		    break; }

	    }
	    break; }
	
	case PEXGANormal : {

	    pFacetList->type = DD_FACET_NORM;
	    pFacetList->numFacets = numFacets;
	    pFacetList->facets.pFacetN = (ddVector3D *)pFacetData;
	    pFacetList->maxData = numFacets * sizeof(ddVector3D);
	    EXTRACT_STRUCT( numFacets, ddVector3D, pFacetList->facets.pFacetN,
			    ptr);
	    break; }

	case PEXGAColour : {

	    pFacetList->numFacets = numFacets;

	    switch (colourType) {

		case PEXIndexedColour: {
		    pFacetList->type = DD_FACET_INDEX;
		    pFacetList->facets.pFacetIndex =
					    (ddIndexedColour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddIndexedColour);
		    EXTRACT_STRUCT( numFacets, ddIndexedColour, 
				    pFacetList->facets.pFacetIndex, ptr); 

		    break; }

		case PEXRgbFloatColour : {
		    pFacetList->type = DD_FACET_RGBFLOAT;
		    pFacetList->facets.pFacetRgbFloat =
					(ddRgbFloatColour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddRgbFloatColour);
		    EXTRACT_STRUCT( numFacets, ddRgbFloatColour, 
				    pFacetList->facets.pFacetRgbFloat, ptr);
		    break; }

		case PEXCieFloatColour : {
		    pFacetList->type = DD_FACET_CIE;
		    pFacetList->facets.pFacetCie = (ddCieColour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddCieColour);
		    EXTRACT_STRUCT( numFacets, ddCieColour, 
				    pFacetList->facets.pFacetCie, ptr);
		    break; }

		case PEXHsvFloatColour : {
		    pFacetList->type = DD_FACET_HSV;
		    pFacetList->facets.pFacetHsv = (ddHsvColour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddHsvColour);
		    EXTRACT_STRUCT( numFacets, ddHsvColour, 
				    pFacetList->facets.pFacetHsv, ptr);
		    break; }

		case PEXHlsFloatColour : {
		    pFacetList->type = DD_FACET_HLS;
		    pFacetList->facets.pFacetHls = (ddHlsColour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddHlsColour);
		    EXTRACT_STRUCT( numFacets, ddHlsColour, 
				    pFacetList->facets.pFacetHls, ptr);
		    break; }

		case PEXRgb8Colour  : {
		    pFacetList->type = DD_FACET_RGB8;
		    pFacetList->facets.pFacetRgb8 = (ddRgb8Colour *)pFacetData;
		    EXTRACT_STRUCT( numFacets, ddRgb8Colour, 
				    pFacetList->facets.pFacetRgb8, ptr);
		    pFacetList->maxData = numFacets * sizeof(ddRgb8Colour);
		    break; }

		case PEXRgb16Colour : {
		    pFacetList->type = DD_FACET_RGB16;
		    pFacetList->facets.pFacetRgb16 = (ddRgb16Colour *)pFacetData;
		    pFacetList->maxData = numFacets * sizeof(ddRgb16Colour);
		    EXTRACT_STRUCT( numFacets, ddRgb16Colour, 
				    pFacetList->facets.pFacetRgb16, ptr);
		    break; }

	    }
	    break; }

	case 0x0000 : {
	    /* neither Colour nor Normal specified */
	    pFacetList->numFacets = 0;
	    pFacetList->type = DD_FACET_NONE;
	    pFacetList->facets.pNoFacet = NULL;
	    pFacetList->maxData = 0;
	    break; }
    }
    
    *rptr = ptr;
}


/*
    Returns number of bytes needed to store this vertex data
 */
int
CountVertexData(ptr, colourType, numVertices, vertexMask)
ddPointer	    ptr;
INT16		    colourType;
CARD32		    numVertices;
CARD16		    vertexMask;
{
    ASSURE(vertexMask <= 7);
    
    switch (vertexMask) {

	case PEXGAColour | PEXGANormal | PEXGAEdges : {

	    switch (colourType) {
		case PEXIndexedColour: 
			return(numVertices * sizeof(ddIndexNormEdgePoint));
		
		case PEXRgbFloatColour : 
			return(numVertices * sizeof(ddRgbFloatNormEdgePoint));

		case PEXCieFloatColour : 
			return(numVertices *sizeof(ddCieNormEdgePoint));

		case PEXHsvFloatColour : 
			return(numVertices *sizeof(ddHsvNormEdgePoint));

		case PEXHlsFloatColour : 
			return(numVertices *sizeof(ddHlsNormEdgePoint));

		case PEXRgb8Colour  : 
			return(numVertices*sizeof(ddRgb8NormEdgePoint));

		case PEXRgb16Colour : 
			return(numVertices * sizeof(ddRgb16NormEdgePoint));

		default:
			return(0);
		}
	    break; }
	
	case PEXGAColour | PEXGANormal : {

	    switch (colourType) {
		case PEXIndexedColour: 
			return(numVertices* sizeof(ddIndexNormalPoint));
		
		case PEXRgbFloatColour : 
			return(numVertices * sizeof(ddRgbFloatNormalPoint));

		case PEXCieFloatColour : 
			return(numVertices * sizeof(ddCieNormalPoint));

		case PEXHsvFloatColour : 
			return(numVertices * sizeof(ddHsvNormalPoint));

		case PEXHlsFloatColour : 
			return(numVertices * sizeof(ddHlsNormalPoint));

		case PEXRgb8Colour  : 
			return(numVertices * sizeof(ddRgb8NormalPoint));

		case PEXRgb16Colour : 
			return(numVertices * sizeof(ddRgb16NormalPoint));

		default:
			return(0);
		}
	    break; }
	
	case PEXGANormal | PEXGAEdges: 
		return(numVertices * sizeof(ddNormEdgePoint));

	case PEXGANormal : 
		return(numVertices * sizeof(ddNormalPoint));

	case PEXGAEdges : 
		return(numVertices * sizeof(ddEdgePoint));

	case PEXGAColour | PEXGAEdges : {

	    switch (colourType) {
		case PEXIndexedColour: 
			return(numVertices * sizeof(ddIndexEdgePoint));

		case PEXRgbFloatColour : 
			return(numVertices*sizeof(ddRgbFloatEdgePoint));

		case PEXCieFloatColour : 
			return(numVertices * sizeof(ddCieEdgePoint));

		case PEXHsvFloatColour : 
			return(numVertices * sizeof(ddHsvEdgePoint));

		case PEXHlsFloatColour : 
			return(numVertices * sizeof(ddHlsEdgePoint));

		case PEXRgb8Colour  : 
			return(numVertices * sizeof(ddRgb8EdgePoint));

		case PEXRgb16Colour : 
			return(numVertices * sizeof(ddRgb16EdgePoint));

		default:
			return(0);
	    }
	    break; }
		
	case PEXGAColour: {

	    switch (colourType) {
		case PEXIndexedColour: 
			return(numVertices * sizeof(ddIndexPoint));

		case PEXRgbFloatColour : 
			return(numVertices * sizeof(ddRgbFloatPoint));

		case PEXCieFloatColour : 
			return(numVertices * sizeof(ddCiePoint));

		case PEXHsvFloatColour : 
			return(numVertices * sizeof(ddHsvPoint));

		case PEXHlsFloatColour : 
			return(numVertices * sizeof(ddHlsPoint));

		case PEXRgb8Colour  : 
			return(numVertices * sizeof(ddRgb8Point));

		case PEXRgb16Colour : 
			return(numVertices * sizeof(ddRgb16Point));

		default:
			return(0);
	    }
	    break; }
		
	case 0x0000 : 
	    /* none of Colour nor Normal nor Edge specified */
	    return(numVertices * sizeof(ddCoord3D));

	default:
	    return(0);
    }

}


/*
    Parses vertex data into already allocated memory (pVertexData)
*/
void
ParseVertexData(ptr, colourType, numVertices, vertexMask, pVertexList, pVertexData, rtype, rptr)
ddPointer	    ptr;
INT16		    colourType;
CARD32		    numVertices;
CARD16		    vertexMask;
listofddPoint	    *pVertexList;   /* out */
ddPointer	    *pVertexData;   /* out */
ddPointType	    *rtype;	    /* out */
ddPointer	    *rptr;	    /* out */
{
    ASSURE(vertexMask <= 7);
    
    pVertexList->numPoints = numVertices;
    switch (vertexMask) {

	case PEXGAColour | PEXGANormal | PEXGAEdges : {

	    switch (colourType) {
		case PEXIndexedColour: {
		    *rtype = DD_INDEX_NORM_EDGE_POINT;
		    pVertexList->pts.pIndexNEpt
				= (ddIndexNormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices* sizeof(ddIndexNormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddIndexNormEdgePoint,
				    pVertexList->pts.pIndexNEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddIndexNormEdgePoint);
		    break; }
		
		case PEXRgbFloatColour : {
		    *rtype = DD_RGBFLOAT_NORM_EDGE_POINT;
		    pVertexList->pts.pRgbFloatNEpt
				= (ddRgbFloatNormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgbFloatNormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgbFloatNormEdgePoint,
				    pVertexList->pts.pRgbFloatNEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgbFloatNormEdgePoint);
		    break; }

		case PEXCieFloatColour : {
		    *rtype = DD_CIE_NORM_EDGE_POINT;
		    pVertexList->pts.pCieNEpt = (ddCieNormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices *sizeof(ddCieNormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddCieNormEdgePoint,
				    pVertexList->pts.pCieNEpt, ptr);
		    pVertexList->maxData = numVertices *sizeof(ddCieNormEdgePoint);
		    break; }

		case PEXHsvFloatColour : {
		    *rtype = DD_HSV_NORM_EDGE_POINT;
		    pVertexList->pts.pHsvNEpt = (ddHsvNormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices *sizeof(ddHsvNormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddHsvNormEdgePoint,
				    pVertexList->pts.pHsvNEpt, ptr);
		    pVertexList->maxData = numVertices *sizeof(ddHsvNormEdgePoint);
		    break; }

		case PEXHlsFloatColour : {
		    *rtype = DD_HLS_NORM_EDGE_POINT;
		    pVertexList->pts.pHlsNEpt = (ddHlsNormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices *sizeof(ddHlsNormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddHlsNormEdgePoint,
				    pVertexList->pts.pHlsNEpt, ptr);
		    pVertexList->maxData = numVertices *sizeof(ddHlsNormEdgePoint);
		    break; }

		case PEXRgb8Colour  : {
		    *rtype = DD_RGB8_NORM_EDGE_POINT;
		    pVertexList->pts.pRgb8NEpt
				    = (ddRgb8NormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices*sizeof(ddRgb8NormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgb8NormEdgePoint,
				    pVertexList->pts.pRgb8NEpt, ptr);
		    pVertexList->maxData = numVertices*sizeof(ddRgb8NormEdgePoint);
		    break; }

		case PEXRgb16Colour : {
		    *rtype = DD_RGB16_NORM_EDGE_POINT;
		    pVertexList->pts.pRgb16NEpt
				    = (ddRgb16NormEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices* sizeof(ddRgb16NormEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgb16NormEdgePoint,
				    pVertexList->pts.pRgb16NEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb16NormEdgePoint);
		    break; }

		}
	    break; }
	
	case PEXGAColour | PEXGANormal : {

	    switch (colourType) {
		case PEXIndexedColour: {
		    *rtype = DD_INDEX_NORM_POINT;
		    pVertexList->pts.pIndexNpt
				    = (ddIndexNormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices* sizeof(ddIndexNormalPoint);
		    EXTRACT_STRUCT( numVertices, ddIndexNormalPoint,
				    pVertexList->pts.pIndexNpt, ptr);
		    pVertexList->maxData = numVertices* sizeof(ddIndexNormalPoint);
		    break; }
		
		case PEXRgbFloatColour : {
		    *rtype = DD_RGBFLOAT_NORM_POINT;
		    pVertexList->pts.pRgbFloatNpt
				= (ddRgbFloatNormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgbFloatNormalPoint);
		    EXTRACT_STRUCT( numVertices, ddRgbFloatNormalPoint,
				    pVertexList->pts.pRgbFloatNpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgbFloatNormalPoint);
		    break; }

		case PEXCieFloatColour : {
		    *rtype = DD_CIE_NORM_POINT;
		    pVertexList->pts.pCieNpt = (ddCieNormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddCieNormalPoint);
		    EXTRACT_STRUCT( numVertices, ddCieNormalPoint,
				    pVertexList->pts.pCieNpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddCieNormalPoint);
		    break; }

		case PEXHsvFloatColour : {
		    *rtype = DD_HSV_NORM_POINT;
		    pVertexList->pts.pHsvNpt = (ddHsvNormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHsvNormalPoint);
		    EXTRACT_STRUCT( numVertices, ddHsvNormalPoint,
				    pVertexList->pts.pHsvNpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHsvNormalPoint);
		    break; }

		case PEXHlsFloatColour : {
		    *rtype = DD_HLS_NORM_POINT;
		    pVertexList->pts.pHlsNpt = (ddHlsNormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHlsNormalPoint);
		    EXTRACT_STRUCT( numVertices, ddHlsNormalPoint,
				    pVertexList->pts.pHlsNpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHlsNormalPoint);
		    break; }

		case PEXRgb8Colour  : {
		    *rtype = DD_RGB8_NORM_POINT;
		    pVertexList->pts.pRgb8Npt = (ddRgb8NormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgb8NormalPoint);
		    EXTRACT_STRUCT( numVertices, ddRgb8NormalPoint,
				    pVertexList->pts.pRgb8Npt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb8NormalPoint);
		    break; }

		case PEXRgb16Colour : {
		    *rtype = DD_RGB16_NORM_POINT;
		    pVertexList->pts.pRgb16Npt = (ddRgb16NormalPoint *)(*pVertexData);
		    (*pVertexData) += numVertices* sizeof(ddRgb16NormalPoint);
		    EXTRACT_STRUCT( numVertices, ddRgb16NormalPoint,
				    pVertexList->pts.pRgb16Npt, ptr);
		    pVertexList->maxData = numVertices* sizeof(ddRgb16NormalPoint);
		    break; }

		}
	    break; }
	
	case PEXGANormal | PEXGAEdges: {
	    *rtype = DD_NORM_EDGE_POINT;
	    pVertexList->pts.pNEpt = (ddNormEdgePoint *)(*pVertexData);
	    (*pVertexData) += numVertices * sizeof(ddNormEdgePoint);
	    EXTRACT_STRUCT( numVertices, ddNormEdgePoint, 
			    pVertexList->pts.pNEpt, ptr);
	    pVertexList->maxData = numVertices * sizeof(ddNormEdgePoint);
	    break; }

	case PEXGANormal : {
	    *rtype = DD_NORM_POINT;
	    pVertexList->pts.pNpt = (ddNormalPoint *)(*pVertexData);
	    (*pVertexData) += numVertices * sizeof(ddNormalPoint);
	    EXTRACT_STRUCT( numVertices, ddNormalPoint,
			    pVertexList->pts.pNpt, ptr);
	    pVertexList->maxData = numVertices * sizeof(ddNormalPoint);
	    break; }

	case PEXGAEdges : {
	    *rtype = DD_EDGE_POINT;
	    pVertexList->pts.pEpt = (ddEdgePoint *)(*pVertexData);
	    (*pVertexData) += numVertices * sizeof(ddEdgePoint);
	    EXTRACT_STRUCT( numVertices, ddEdgePoint,
			    pVertexList->pts.pEpt, ptr);
	    pVertexList->maxData = numVertices * sizeof(ddEdgePoint);
	    break; }

	case PEXGAColour | PEXGAEdges : {

	    switch (colourType) {
		case PEXIndexedColour: {
		    *rtype = DD_INDEX_EDGE_POINT;
		    pVertexList->pts.pIndexEpt = (ddIndexEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddIndexEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddIndexEdgePoint, 
				    pVertexList->pts.pIndexEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddIndexEdgePoint);
		    break; }

		case PEXRgbFloatColour : {
		    *rtype = DD_RGBFLOAT_EDGE_POINT;
		    pVertexList->pts.pRgbFloatEpt = (ddRgbFloatEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices*sizeof(ddRgbFloatEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgbFloatEdgePoint, 
				    pVertexList->pts.pRgbFloatEpt, ptr);
		    pVertexList->maxData = numVertices*sizeof(ddRgbFloatEdgePoint);
		    break; }

		case PEXCieFloatColour : {
		    *rtype = DD_CIE_EDGE_POINT;
		    pVertexList->pts.pCieEpt = (ddCieEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddCieEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddCieEdgePoint, 
				    pVertexList->pts.pCieEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddCieEdgePoint);
		    break; }

		case PEXHsvFloatColour : {
		    *rtype = DD_HSV_EDGE_POINT;
		    pVertexList->pts.pHsvEpt = (ddHsvEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHsvEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddHsvEdgePoint, 
				    pVertexList->pts.pHsvEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHsvEdgePoint);
		    break; }

		case PEXHlsFloatColour : {
		    *rtype = DD_HLS_EDGE_POINT;
		    pVertexList->pts.pHlsEpt = (ddHlsEdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHlsEdgePoint);
		    EXTRACT_STRUCT( numVertices, ddHlsEdgePoint, 
				    pVertexList->pts.pHlsEpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHlsEdgePoint);
		    break; }

		case PEXRgb8Colour  : {
		    *rtype = DD_RGB8_EDGE_POINT;
		    pVertexList->pts.pRgb8Ept = (ddRgb8EdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgb8EdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgb8EdgePoint, 
				    pVertexList->pts.pRgb8Ept, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb8EdgePoint);
		    break; }

		case PEXRgb16Colour : {
		    *rtype = DD_RGB16_EDGE_POINT;
		    pVertexList->pts.pRgb16Ept = (ddRgb16EdgePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgb16EdgePoint);
		    EXTRACT_STRUCT( numVertices, ddRgb16EdgePoint, 
				    pVertexList->pts.pRgb16Ept, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb16EdgePoint);
		    break; }

	    }
	    break; }
		
	case PEXGAColour: {

	    switch (colourType) {
		case PEXIndexedColour: {
		    *rtype = DD_INDEX_POINT;
		    pVertexList->pts.pIndexpt = (ddIndexPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddIndexPoint);
		    EXTRACT_STRUCT( numVertices, ddIndexPoint, 
				    pVertexList->pts.pIndexpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddIndexPoint);
		    break; }

		case PEXRgbFloatColour : {
		    *rtype = DD_RGBFLOAT_POINT;
		    pVertexList->pts.pRgbFloatpt = (ddRgbFloatPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgbFloatPoint);
		    EXTRACT_STRUCT( numVertices, ddRgbFloatPoint, 
				    pVertexList->pts.pRgbFloatpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgbFloatPoint);
		    break; }

		case PEXCieFloatColour : {
		    *rtype = DD_CIE_POINT;
		    pVertexList->pts.pCiept = (ddCiePoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddCiePoint);
		    EXTRACT_STRUCT( numVertices, ddCiePoint, 
				    pVertexList->pts.pCiept, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddCiePoint);
		    break; }

		case PEXHsvFloatColour : {
		    *rtype = DD_HSV_POINT;
		    pVertexList->pts.pHsvpt = (ddHsvPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHsvPoint);
		    EXTRACT_STRUCT( numVertices, ddHsvPoint, 
				    pVertexList->pts.pHsvpt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHsvPoint);
		    break; }

		case PEXHlsFloatColour : {
		    *rtype = DD_HLS_POINT;
		    pVertexList->pts.pHlspt = (ddHlsPoint *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddHlsPoint);
		    EXTRACT_STRUCT( numVertices, ddHlsPoint, 
				    pVertexList->pts.pHlspt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddHlsPoint);
		    break; }

		case PEXRgb8Colour  : {
		    *rtype = DD_RGB8_POINT;
		    pVertexList->pts.pRgb8pt = (ddRgb8Point *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgb8Point);
		    EXTRACT_STRUCT( numVertices, ddRgb8Point, 
				    pVertexList->pts.pRgb8pt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb8Point);
		    break; }

		case PEXRgb16Colour : {
		    *rtype = DD_RGB16_POINT;
		    pVertexList->pts.pRgb16pt = (ddRgb16Point *)(*pVertexData);
		    (*pVertexData) += numVertices * sizeof(ddRgb16Point);
		    EXTRACT_STRUCT( numVertices, ddRgb16Point, 
				    pVertexList->pts.pRgb16pt, ptr);
		    pVertexList->maxData = numVertices * sizeof(ddRgb16Point);
		    break; }

	    }
	    break; }
		
	case 0x0000 : {
	    /* none of Colour nor Normal nor Edge specified */
	    *rtype = DD_3D_POINT;
	    pVertexList->pts.p3Dpt = (ddCoord3D *)(*pVertexData);
	    (*pVertexData) += numVertices * sizeof(ddCoord3D);
	    EXTRACT_STRUCT( numVertices, ddCoord3D, pVertexList->pts.p3Dpt, ptr);
	    pVertexList->maxData = numVertices * sizeof(ddCoord3D);
	    break; }
    }

    *rptr = ptr;

}


OC_PARSER_FUNC_HEADER(ColourOC)
{
    CARD16	    colourType;
    miColourStruct  *ddColour;
    PARSER_PTR(ptr);

    EXTRACT_CARD16(colourType,ptr);/*temp since we haven't alloc'd ddColour yet*/
    SKIP_PADDING(ptr,2);
    switch (colourType) {

	case PEXIndexedColour: {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddIndexedColour)))
	    ddColour->colour.pIndex = (ddIndexedColour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddIndexedColour, ddColour->colour.pIndex, ptr);
	    break; }

	case PEXRgbFloatColour : {
	    GET_DD_STORAGE( ddColour, miColourStruct, 
			    (sizeof(miColourStruct) + sizeof(ddRgbFloatColour)));
	    ddColour->colour.pRgbFloat = (ddRgbFloatColour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddRgbFloatColour, ddColour->colour.pRgbFloat, ptr);
	    break; }

	case PEXCieFloatColour : {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddCieColour)));
	    ddColour->colour.pCie = (ddCieColour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddCieColour, ddColour->colour.pCie, ptr);
	    break; }

	case PEXHsvFloatColour : {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddHsvColour)));
	    ddColour->colour.pHsv = (ddHsvColour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddHsvColour, ddColour->colour.pHsv, ptr);
	    break; }

	case PEXHlsFloatColour : {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddHlsColour)));
	    ddColour->colour.pHls = (ddHlsColour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddHlsColour, ddColour->colour.pHls, ptr);
	    break; }

	case PEXRgb8Colour  : {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddRgb8Colour)));
	    ddColour->colour.pRgb8 = (ddRgb8Colour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddRgb8Colour, ddColour->colour.pRgb8, ptr);
	    break; }

	case PEXRgb16Colour : {
	    GET_DD_STORAGE( ddColour, miColourStruct,
			    (sizeof(miColourStruct) + sizeof(ddRgb16Colour)));
	    ddColour->colour.pRgb16 = (ddRgb16Colour *)(ddColour+1);
	    EXTRACT_STRUCT(1, ddRgb16Colour, ddColour->colour.pRgb16, ptr);
	    break; }
    }
    
    ddColour->colourType = colourType;
    OC_PARSER_RETURN(ddColour);
}

OC_PARSER_FUNC_HEADER(ColourIndexOC)
{
    miColourStruct  *ddColour;
    PARSER_PTR(ptr);

    GET_DD_STORAGE( ddColour, miColourStruct,
		    (sizeof(miColourStruct) + sizeof(ddIndexedColour)));
    ddColour->colour.pIndex = (ddIndexedColour *)(ddColour+1);
    ddColour->colourType = PEXIndexedColour;
    EXTRACT_CARD16(ddColour->colour.pIndex->index,ptr);
    
    OC_PARSER_RETURN(ddColour);
}

OC_PARSER_FUNC_HEADER(LightState)
{
    miLightStateStruct  *ddLightState;
    PARSER_PTR(ptr);
    ddSHORT		enable_count, disable_count;
    ddSHORT		en_count, dis_count;
    int			listSize = 0;
    
    EXTRACT_CARD16(enable_count, ptr);
    EXTRACT_CARD16(disable_count, ptr);

    /* must modify counts so they are always even to guarantee 
     * pointer alignment at 4 byte boundary
     */
    en_count = MAKE_EVEN(enable_count);
    dis_count = MAKE_EVEN(disable_count);   /* probably unnecessary */
    listSize = puCountList(DD_INDEX, en_count) + puCountList(DD_INDEX,dis_count);
    GET_DD_STORAGE( ddLightState,miLightStateStruct,
		    sizeof(miLightStateStruct) + listSize);

    ddLightState->enableList = (listofObj *)(ddLightState + 1);
    puInitList(ddLightState->enableList, DD_INDEX, en_count);
    puAddToList(ptr, enable_count, ddLightState->enableList);

    SKIP_PADDING(ptr, (en_count * sizeof(CARD16)) );

    listSize = puCountList(DD_INDEX, en_count);
    ddLightState->disableList =
	   (listofObj *)(((ddPointer)(ddLightState->enableList)) + listSize);
    puInitList(ddLightState->disableList, DD_INDEX, dis_count);
    puAddToList(ptr, disable_count, ddLightState->disableList);

    OC_PARSER_RETURN(ddLightState);
}

OC_PARSER_FUNC_HEADER(SetMCVolume)
{
    miMCVolume_Struct   *ddMCV;
    PARSER_PTR(ptr);

    ddHalfSpace	ddHS;
    INT16	i, count, op;
    int		listSize = 0;
    ddFLOAT	length;

    EXTRACT_INT16(op, ptr);
    EXTRACT_INT16(count, ptr);
    listSize = puCountList(DD_HALF_SPACE, count);
    GET_DD_STORAGE( ddMCV, miMCVolume_Struct,
		    sizeof(miMCVolume_Struct) + listSize);

    ddMCV->operator = op;
    ddMCV->halfspaces = (listofObj *)(ddMCV + 1);
    puInitList(ddMCV->halfspaces, DD_HALF_SPACE, count);

    for (i = 0; i < count; i++){
      EXTRACT_COORD3D(&ddHS.orig_point, ptr);
      ddHS.orig_point.w = 0.0; 	/* JSH */
      ddHS.point.w = 0.5; 	/* ? */
      EXTRACT_VECTOR3D(&ddHS.orig_vector, ptr);

      puAddToList(&ddHS, 1, ddMCV->halfspaces);
    }

    OC_PARSER_RETURN(ddMCV);
}

OC_PARSER_FUNC_HEADER(SetMCVolume2D)
{

    miMCVolume_Struct	*ddMCV;
    PARSER_PTR(ptr);
    ddHalfSpace         ddHS;
    INT16	i, count, op;
    int			listSize = 0;
    ddFLOAT		length;
 
    EXTRACT_INT16(op, ptr);
    EXTRACT_INT16(count, ptr);
    listSize = puCountList(DD_HALF_SPACE, count);
    GET_DD_STORAGE( ddMCV, miMCVolume_Struct,
		    sizeof(miMCVolume_Struct) + listSize);

    ddMCV->operator = op;
    ddMCV->halfspaces = (listofObj *)(ddMCV + 1);
    puInitList(ddMCV->halfspaces, DD_HALF_SPACE, count);
 
    for (i = 0; i < count; i++){
      EXTRACT_COORD2D(&ddHS.orig_point, ptr);
      ddHS.orig_point.z = 0.0;
      ddHS.orig_point.w = 0.0;
      ddHS.point.w = 0.5;	/* ? */
      EXTRACT_VECTOR2D(&ddHS.orig_vector, ptr);
      ddHS.orig_vector.z = 0.0;

      puAddToList(&ddHS, 1, ddMCV->halfspaces);
    }
 
    OC_PARSER_RETURN(ddMCV);
}

OC_PARSER_FUNC_HEADER(Marker)
{
    miMarkerStruct  *ddMarker;
    listofddPoint   *ddPoint;
    ddULONG	    numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexMarker) / sizeof(pexCoord3D);
    GET_DD_STORAGE( ddMarker, miListHeader, (sizeof(miListHeader) +
		    sizeof(listofddPoint) + numPoints * sizeof(ddCoord3D)));
    ddPoint           = (listofddPoint *)(ddMarker+1);
    ddMarker->type     = DD_3D_POINT;
    ddMarker->flags    = 0;
    ddMarker->numLists = 1;
    ddMarker->maxLists = 1;
    ddMarker->ddList   = ddPoint;
    ddPoint->numPoints = numPoints;
    ddPoint->pts.p3Dpt = (ddCoord3D *)(ddPoint + 1);
    EXTRACT_LISTOF_COORD3D(ddPoint->numPoints,ddPoint->pts.p3Dpt,ptr);
    
    OC_PARSER_RETURN(ddMarker);
}


OC_PARSER_FUNC_HEADER(Marker2D)
{
    miMarkerStruct  *ddMarker;
    listofddPoint   *ddPoint;
    ddULONG	    numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexMarker2D) / sizeof(pexCoord2D);
    GET_DD_STORAGE( ddMarker, miListHeader, (sizeof(miListHeader) +
		    sizeof(listofddPoint) + numPoints * sizeof(ddCoord2D)));
    ddPoint            = (listofddPoint *)(ddMarker+1);
    ddMarker->type     = DD_2D_POINT;
    ddMarker->flags    = 0;
    ddMarker->numLists = 1;
    ddMarker->maxLists = 1;
    ddMarker->ddList   = ddPoint;
    ddPoint->numPoints = numPoints;
    ddPoint->pts.p2Dpt = (ddCoord2D *)(ddPoint + 1);
    EXTRACT_LISTOF_COORD2D(ddPoint->numPoints,ddPoint->pts.p2Dpt,ptr);
    
    OC_PARSER_RETURN(ddMarker);
}


OC_PARSER_FUNC_HEADER(Text)
{
    miTextStruct  *ddText;
    PARSER_PTR(ptr);
    
    GET_DD_STORAGE( ddText, miTextStruct, sizeof(miTextStruct)
		    + 3 * sizeof(ddCoord3D)
		    + ((pexText *)pPEXOC)->head.length * sizeof(CARD32)
		    - sizeof(pexText));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    ddText->pOrigin = (ddCoord3D *)(ddText + 1);
    ddText->pDirections = (ddText->pOrigin) + 1;
    EXTRACT_LISTOF_COORD3D(1, ddText->pOrigin, ptr);
    EXTRACT_LISTOF_COORD3D(2, ddText->pDirections, ptr);
    EXTRACT_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    ddText->pText = (pexMonoEncoding *)((ddText->pDirections) + 2);
    memcpy(  (char *)(ddText->pText), (char *)ptr, 
	    (int)(  sizeof(CARD32) * ((pexText *)pPEXOC)->head.length
		    - sizeof(pexText)));
    
    OC_PARSER_RETURN(ddText);
}
 

OC_PARSER_FUNC_HEADER(Text2D)
{
    miText2DStruct  *ddText;
    PARSER_PTR(ptr);
    
    GET_DD_STORAGE( ddText, miText2DStruct, sizeof(miText2DStruct)
		    + sizeof(ddCoord2D)
		    + ((pexText2D *)pPEXOC)->head.length * sizeof(CARD32)
		    - sizeof(pexText2D));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    ddText->pOrigin = (ddCoord2D *)(ddText + 1);
    EXTRACT_LISTOF_COORD2D(1, ddText->pOrigin, ptr);
    EXTRACT_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    ddText->pText = (pexMonoEncoding *)((ddText->pOrigin) + 1);
    memcpy(  (char *)(ddText->pText), (char *)ptr, 
	    (int)( sizeof(CARD32) * ((pexText2D *)pPEXOC)->head.length
		    - sizeof(pexText2D)));
    
    OC_PARSER_RETURN(ddText);
}


OC_PARSER_FUNC_HEADER(AnnotationText)
{
    miAnnoTextStruct	*ddText;
    PARSER_PTR(ptr);
    
    GET_DD_STORAGE( ddText, miAnnoTextStruct, sizeof(miAnnoTextStruct)
		    + 2 * sizeof(ddCoord3D)
		    + ((pexAnnotationText *)pPEXOC)->head.length
		    * sizeof(CARD32)
		    - sizeof(pexAnnotationText));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    ddText->pOrigin = (ddCoord3D *)(ddText + 1);
    ddText->pOffset = (ddText->pOrigin) + 1;
    EXTRACT_LISTOF_COORD3D(1, ddText->pOrigin, ptr);
    EXTRACT_LISTOF_COORD3D(1, ddText->pOffset, ptr);
    EXTRACT_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    ddText->pText = (pexMonoEncoding *)((ddText->pOffset) + 1);
    memcpy(  (char *)(ddText->pText), (char *)ptr, 
	    (int)( sizeof(CARD32) * ((pexAnnotationText *)pPEXOC)->head.length
		   - sizeof(pexAnnotationText)));

    OC_PARSER_RETURN(ddText);
    
}




OC_PARSER_FUNC_HEADER(AnnotationText2D)
{
    miAnnoText2DStruct  *ddText;
    PARSER_PTR(ptr);
    
    GET_DD_STORAGE( ddText, miAnnoText2DStruct, sizeof(miAnnoText2DStruct)
		    + 2 * sizeof(ddCoord2D)
		    + ((pexAnnotationText2D *)pPEXOC)->head.length
		    * sizeof(CARD32)
		    - sizeof(pexAnnotationText2D));
		    /*	this also allocates any trailing pads, but so
			much the better					*/
    ddText->pOrigin = (ddCoord2D *)(ddText + 1);
    ddText->pOffset = (ddText->pOrigin) + 1;
    EXTRACT_LISTOF_COORD2D(1, ddText->pOrigin, ptr);
    EXTRACT_LISTOF_COORD2D(1, ddText->pOffset, ptr);
    EXTRACT_CARD16(ddText->numEncodings, ptr);
    SKIP_PADDING(ptr, 2);
    ddText->pText = (pexMonoEncoding *)((ddText->pOffset) + 1);
    memcpy(  (char *)(ddText->pText), (char *)ptr, 
	    (int)(  sizeof(CARD32) * ((pexAnnotationText2D *)pPEXOC)->head.length
		    - sizeof(pexAnnotationText2D)));

    OC_PARSER_RETURN(ddText);
    
}


OC_PARSER_FUNC_HEADER(Polyline2D)
{
    miPolylineStruct    *ddPoly;
    listofddPoint	*ddPoint;
    ddULONG		numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexPolyline2D) / sizeof(pexCoord2D);
    GET_DD_STORAGE( ddPoly, miListHeader, sizeof(miListHeader) +
		    sizeof(listofddPoint) + numPoints * sizeof(ddCoord2D));
    ddPoint           = (listofddPoint *)(ddPoly+1);
    ddPoly->type       = DD_2D_POINT;
    ddPoly->flags      = 0;
    ddPoly->numLists   = 1;
    ddPoly->maxLists   = 1;
    ddPoly->ddList     = ddPoint;
    ddPoint->numPoints = numPoints;
    ddPoint->maxData   = numPoints * sizeof(ddCoord2D);
    ddPoint->pts.p2Dpt = (ddCoord2D *)(ddPoint + 1);
    EXTRACT_LISTOF_COORD2D(ddPoint->numPoints,ddPoint->pts.p2Dpt,ptr);
    
    OC_PARSER_RETURN(ddPoly);
    
}



OC_PARSER_FUNC_HEADER(Polyline)
{
    miPolylineStruct    *ddPoly;
    listofddPoint	*ddPoint;
    ddULONG		numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexPolyline) / sizeof(pexCoord3D);
    GET_DD_STORAGE( ddPoly, miListHeader, sizeof(miListHeader) +
		    sizeof(listofddPoint) + numPoints * sizeof(ddCoord3D));
    ddPoint           = (listofddPoint *)(ddPoly+1);
    ddPoly->type       = DD_3D_POINT;
    ddPoly->flags      = 0;
    ddPoly->numLists   = 1;
    ddPoly->maxLists   = 1;
    ddPoly->ddList     = ddPoint;
    ddPoint->numPoints = numPoints;
    ddPoint->maxData   = numPoints * sizeof(ddCoord3D);
    ddPoint->pts.p3Dpt = (ddCoord3D *)(ddPoint + 1);
    EXTRACT_LISTOF_COORD3D(ddPoint->numPoints,ddPoint->pts.p3Dpt,ptr);
    
    OC_PARSER_RETURN(ddPoly);
    
}


OC_PARSER_FUNC_HEADER(PolylineSet)
{
    miPolylineStruct    *ddPoly;
    listofddPoint	*ddPoint;
    pexPolylineSet	*pPoly = (pexPolylineSet *)pPEXOC;
    PARSER_PTR(ptr);
    ddULONG 		i;
    CARD32 		numPoints;
    int			vertexSize = 0;
    ddPointer		vertexPtr = 0;
    ddPointer		rptr = 0;
    ddpex2rtn		err = Success;
    
    ptr = (ddPointer)(pPoly+1);
    for (i=0; i<pPoly->numLists; i++) {
	EXTRACT_CARD32(numPoints, ptr);
	ptr += CountVertexData(	ptr, pPoly->colourType, numPoints,
				pPoly->vertexAttribs); }

    vertexSize = ptr - (ddPointer)(pPoly+1) -
        pPoly->numLists * sizeof(CARD32);

    GET_DD_STORAGE( ddPoly, miListHeader,
		    (sizeof(miListHeader) + vertexSize
		    + pPoly->numLists * sizeof(listofddPoint)));
    ddPoly->numLists   = (ddULONG)(pPoly->numLists);
    ddPoly->maxLists   = ddPoly->numLists;
    ddPoly->flags      = 0;
    ddPoly->ddList     = (listofddPoint *)(ddPoly+1);
    ptr = (ddPointer)(pPoly+1);

    for (i=0, ddPoint = (listofddPoint *)(ddPoly+1),
	    vertexPtr = (ddPointer)(ddPoly->ddList + ddPoly->numLists);
	 i<ddPoly->numLists;
	 i++, ddPoint++) {
				
	EXTRACT_CARD32(numPoints, ptr);
	ParseVertexData(    ptr, pPoly->colourType, numPoints,
			    pPoly->vertexAttribs, ddPoint, 
			    &vertexPtr, &(ddPoly->type), &rptr);
	ptr = rptr;
    }

    OC_PARSER_RETURN(ddPoly);
    
}


OC_PARSER_FUNC_HEADER(NurbCurve)
{
    miNurbStruct    *ddNurb;
    pexNurbCurve    *pNurb = (pexNurbCurve *)pPEXOC;
    ddUSHORT	    pointSize = 0;
    PARSER_PTR(ptr);

    
    pointSize = ((pNurb->coordType == PEXRational)
		    ? sizeof(ddCoord4D) : sizeof(ddCoord3D));
    GET_DD_STORAGE( ddNurb, miNurbStruct, (sizeof(miNurbStruct)
		    + sizeof(listofddPoint) + pNurb->numKnots * sizeof(ddFLOAT)
		    + pNurb->numPoints) * pointSize);
    ddNurb->pKnots = (ddFLOAT *)(ddNurb+1);
    ddNurb->points.ddList = (listofddPoint *)(ddNurb->pKnots + pNurb->numKnots);
    EXTRACT_CARD16(ddNurb->order, ptr);
    SKIP_PADDING(ptr,2);		/* place holder for type */
    EXTRACT_FLOAT(ddNurb->uMin, ptr);
    EXTRACT_FLOAT(ddNurb->uMax, ptr);
    EXTRACT_CARD32(ddNurb->numKnots, ptr);
    EXTRACT_CARD32(ddNurb->points.ddList->numPoints, ptr);

    EXTRACT_STRUCT(ddNurb->numKnots, PEXFLOAT, ddNurb->pKnots, ptr);
    if (pNurb->coordType == PEXRational) {
	ddNurb->points.type = DDPT_4D;
	ddNurb->points.ddList->pts.p4Dpt =
				    (ddCoord4D *)((ddNurb->points.ddList)+1);
	EXTRACT_STRUCT(	ddNurb->points.ddList->numPoints, ddCoord4D,
			ddNurb->points.ddList->pts.p4Dpt, ptr);
    } else {
	ddNurb->points.type = DDPT_3D;
	ddNurb->points.ddList->pts.p3Dpt =
				    (ddCoord3D *)((ddNurb->points.ddList)+1);
	EXTRACT_STRUCT(	ddNurb->points.ddList->numPoints, ddCoord3D, 
			ddNurb->points.ddList->pts.p3Dpt, ptr);
    }

    ddNurb->points.numLists = 1;
    ddNurb->points.maxLists = 1;
    ddNurb->points.flags = 0;

    OC_PARSER_RETURN(ddNurb);
}


OC_PARSER_FUNC_HEADER(FillArea2D)
{
    miFillAreaStruct	*ddFill;
    ddULONG		numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexFillArea2D) / sizeof(pexCoord2D);
    GET_DD_STORAGE( ddFill, miFillAreaStruct, (sizeof(miFillAreaStruct) +
		    sizeof(listofddFacet) + sizeof(listofddPoint)
		    + numPoints * sizeof(ddCoord2D)));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    EXTRACT_CARD16(ddFill->shape, ptr);
    EXTRACT_CARD8(ddFill->ignoreEdges, ptr);
    SKIP_PADDING(ptr, 1);
    
    ddFill->pFacets->type = DD_FACET_NONE;
    ddFill->pFacets->numFacets = 0;
    ddFill->pFacets->facets.pNoFacet = NULL;
    ddFill->contourHint = 0;
    
    ddFill->points.type = DD_2D_POINT;
    ddFill->points.flags = 0;
    ddFill->points.numLists = 1;
    ddFill->points.maxLists = 1;
    ddFill->points.ddList->numPoints = numPoints;
    ddFill->points.ddList->pts.p2Dpt = (ddCoord2D *)((ddFill->points.ddList) +1);
    EXTRACT_LISTOF_COORD2D( ddFill->points.ddList->numPoints,
			    ddFill->points.ddList->pts.p2Dpt,ptr);
    
    OC_PARSER_RETURN(ddFill);
}


 

OC_PARSER_FUNC_HEADER(FillArea)
{
    miFillAreaStruct	*ddFill;
    ddULONG		numPoints;
    PARSER_PTR(ptr);
    
    numPoints = LEN_WO_HEADER(pexFillArea) / sizeof(pexCoord3D);
    GET_DD_STORAGE( ddFill, miFillAreaStruct, (sizeof(miFillAreaStruct) +
		    sizeof(listofddFacet) + sizeof(listofddPoint)
		    + numPoints * sizeof(ddCoord3D)));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    EXTRACT_CARD16(ddFill->shape, ptr);
    EXTRACT_CARD8(ddFill->ignoreEdges, ptr);
    SKIP_PADDING(ptr, 1);
    ddFill->contourHint = 0;
    
    ddFill->pFacets->type = DD_FACET_NONE;
    ddFill->pFacets->numFacets = 0;
    ddFill->pFacets->facets.pNoFacet = NULL;
    
    ddFill->points.type = DD_3D_POINT;
    ddFill->points.flags = 0;
    ddFill->points.numLists = 1;
    ddFill->points.maxLists = 1;
    ddFill->points.ddList->numPoints = numPoints;
    ddFill->points.ddList->pts.p3Dpt = (ddCoord3D *)((ddFill->points.ddList) +1);
    EXTRACT_LISTOF_COORD3D( ddFill->points.ddList->numPoints,
			    ddFill->points.ddList->pts.p3Dpt,ptr);
    
    OC_PARSER_RETURN(ddFill);
}


OC_PARSER_FUNC_HEADER(ExtFillArea)
{
    miFillAreaStruct	*ddFill;
    pexExtFillArea	*pFill = (pexExtFillArea *)pPEXOC;
    PARSER_PTR(ptr);
    ddPointer		rptr = 0;
    ddPointer		facetPtr = 0;
    int			facetSize = 0;
    ddPointer		vertexPtr = 0;
    int			vertexSize = 0;
    ddpex2rtn		err = Success;
    CARD32		totalVertices;
    
    ptr = (ddPointer)(pFill+1);
    facetSize = CountFacetOptData(  ptr, (CARD16)(pFill->colourType),
				    (CARD32)1, pFill->facetAttribs);
    
    ptr += facetSize;	/* this works because dd types == protocol types */
    EXTRACT_CARD32(totalVertices, ptr);
    vertexSize = CountVertexData(   ptr, pFill->colourType, totalVertices,
				    pFill->vertexAttribs);
    GET_DD_STORAGE( ddFill, miFillAreaStruct,
		    (sizeof(miFillAreaStruct) +	sizeof(listofddFacet)
		    + sizeof(listofddPoint) + facetSize + vertexSize));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    ddFill->shape = pFill->shape;
    ddFill->ignoreEdges = pFill->ignoreEdges;
    ddFill->contourHint = 0;
    ddFill->points.numLists = 1;
    ddFill->points.maxLists = 1;
    ddFill->points.flags = 0;
    
    ptr = (ddPointer)(pFill+1);
    facetPtr = (ddPointer)(ddFill->points.ddList + 1);
    ParseFacetOptData(	ptr, (CARD16)(pFill->colourType), (CARD32)1,
			pFill->facetAttribs, ddFill->pFacets,
			facetPtr, &rptr);
    ptr = rptr;

    vertexPtr = facetPtr + facetSize;
    ParseVertexData(	ptr, pFill->colourType, totalVertices,
			pFill->vertexAttribs, ddFill->points.ddList,
			&vertexPtr, &(ddFill->points.type), &rptr);
    ptr = rptr;

    OC_PARSER_RETURN(ddFill);
}


OC_PARSER_FUNC_HEADER(FillAreaSet2D)
{
    miFillAreaStruct	*ddFill;
    PARSER_PTR(ptr);
    ddULONG		i;
    int			listSize = 0, numPoints = 0;
    listofddPoint	*ddPoint;
    ddPointer		ddPtr;
    pexFillAreaSet2D	*pFill = (pexFillAreaSet2D *)pPEXOC;

    ptr = (ddPointer)(pFill+1);
    for (i=0; i<pFill->numLists; i++) {
	EXTRACT_CARD32( numPoints, ptr);
	listSize += numPoints * sizeof(ddCoord2D);
	ptr += numPoints * sizeof(ddCoord2D); }

    GET_DD_STORAGE( ddFill, miFillAreaStruct,
		    (sizeof(miFillAreaStruct) + sizeof(listofddFacet)
		    + (pFill->numLists * sizeof(listofddPoint))
		    + listSize ));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    ddFill->shape = pFill->shape;
    ddFill->ignoreEdges = pFill->ignoreEdges;
    ddFill->contourHint = pFill->contourHint;
    
    ddFill->pFacets->type = DD_FACET_NONE;
    ddFill->pFacets->numFacets = 0;
    ddFill->pFacets->facets.pNoFacet = NULL;
    
    ddFill->points.type = DD_2D_POINT;
    ddFill->points.flags = 0;
    ddFill->points.numLists = pFill->numLists;
    ddFill->points.maxLists = ddFill->points.numLists;

    ptr = (ddPointer)(pFill+1);
    for (i=0, ddPoint = ddFill->points.ddList,
	    ddPtr = (ddPointer)(ddFill->points.ddList + ddFill->points.numLists);
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {
	EXTRACT_CARD32(	ddPoint->numPoints, ptr);
	ddPoint->pts.p2Dpt = (ddCoord2D *)ddPtr;
	ddPtr += ddPoint->numPoints * sizeof(ddCoord2D);
	EXTRACT_STRUCT(	ddPoint->numPoints, ddCoord2D,
			ddPoint->pts.p2Dpt, ptr);
    }
    
    OC_PARSER_RETURN(ddFill);
}


OC_PARSER_FUNC_HEADER(FillAreaSet)
{
    miFillAreaStruct	*ddFill;
    PARSER_PTR(ptr);
    ddULONG		 i;
    int			listSize = 0, numPoints = 0;
    listofddPoint	*ddPoint;
    ddPointer		ddPtr;
    pexFillAreaSet	*pFill = (pexFillAreaSet *)pPEXOC;

    ptr = (ddPointer)(pFill+1);
    for (i=0; i<pFill->numLists; i++) {
	EXTRACT_CARD32( numPoints, ptr);
	listSize += numPoints * sizeof(ddCoord3D);
	ptr += numPoints * sizeof(ddCoord3D); }

    GET_DD_STORAGE( ddFill, miFillAreaStruct,
		    (sizeof(miFillAreaStruct)
		    + sizeof(listofddFacet)
		    + (pFill->numLists * sizeof(listofddPoint))
		    + listSize ));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    ddFill->shape = pFill->shape;
    ddFill->ignoreEdges = pFill->ignoreEdges;
    ddFill->contourHint = pFill->contourHint;
    
    ddFill->pFacets->type = DD_FACET_NONE;
    ddFill->pFacets->numFacets = 0;
    ddFill->pFacets->facets.pNoFacet = NULL;
    
    ddFill->points.type = DD_3D_POINT;
    ddFill->points.flags = 0;
    ddFill->points.numLists = pFill->numLists;
    ddFill->points.maxLists = ddFill->points.numLists;

    ptr = (ddPointer)(pFill+1);
    for (i=0, ddPoint = ddFill->points.ddList,
	    ddPtr = (ddPointer)(ddFill->points.ddList + ddFill->points.numLists);
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {
	EXTRACT_CARD32(	ddPoint->numPoints, ptr);
	ddPoint->pts.p3Dpt = (ddCoord3D *)ddPtr;
	ddPtr += ddPoint->numPoints * sizeof(ddCoord3D);
	EXTRACT_STRUCT(	ddPoint->numPoints, ddCoord3D,
			ddPoint->pts.p3Dpt, ptr);
    }
    
    OC_PARSER_RETURN(ddFill);
}


OC_PARSER_FUNC_HEADER(ExtFillAreaSet)
{
    miFillAreaStruct	*ddFill;
    PARSER_PTR(ptr);
    ddULONG		i;
    ddULONG		numPoints;
    listofddPoint	*ddPoint;
    pexExtFillAreaSet	*pFill = (pexExtFillAreaSet *)(pPEXOC);
    ddPointer		rptr = 0;
    ddPointer		facetPtr = 0;
    int			facetSize = 0;
    ddPointer		vertexPtr = 0;
    int			vertexSize = 0;
    ddpex2rtn		err = Success;

    ptr = (ddPointer)(pFill+1);
    facetSize = CountFacetOptData(  ptr, (CARD16)(pFill->colourType),
				    (CARD32)1, pFill->facetAttribs);
    ptr += facetSize;	/* this works because dd types == protocol types */
    for (i=0; i<pFill->numLists; i++) {
	EXTRACT_CARD32(	numPoints, ptr);
	ptr += CountVertexData(	ptr, pFill->colourType, numPoints,
					pFill->vertexAttribs); }

    vertexSize = ptr - (ddPointer)(pFill+1) - facetSize;
    GET_DD_STORAGE( ddFill, miFillAreaStruct,
		    (sizeof(miFillAreaStruct) + sizeof(listofddFacet)
		    + (pFill->numLists * sizeof(listofddPoint)
		    + facetSize + vertexSize)));
    ddFill->pFacets = (listofddFacet *)(ddFill+1);
    ddFill->points.ddList = (listofddPoint *)((ddFill->pFacets)+1);
    ddFill->shape = pFill->shape;
    ddFill->ignoreEdges = pFill->ignoreEdges;
    ddFill->contourHint = pFill->contourHint;
    ddFill->points.numLists = (ddULONG)(pFill->numLists);
    ddFill->points.maxLists = ddFill->points.numLists;
    ddFill->points.flags = 0;
    ptr = (ddPointer)(pFill+1);

    facetPtr = (ddPointer)(ddFill->points.ddList + pFill->numLists);
    ParseFacetOptData(	ptr, (CARD16)(pFill->colourType), (CARD32)1, 
			pFill->facetAttribs, ddFill->pFacets,
			facetPtr, &rptr);
    ptr = rptr;

    vertexPtr = facetPtr + facetSize;
    for (i=0, ddPoint = ddFill->points.ddList;
	 i<ddFill->points.numLists;
	 i++, ddPoint++) {

	EXTRACT_CARD32(	numPoints, ptr);
	ParseVertexData(    ptr, pFill->colourType, numPoints,
			    pFill->vertexAttribs, ddPoint, &vertexPtr,
			    &(ddFill->points.type), &rptr);
	ptr = rptr;
    }
    

    OC_PARSER_RETURN(ddFill);
}


OC_PARSER_FUNC_HEADER(SOFAS)
{
    miSOFASStruct   *ddFill;
    pexSOFAS	    *pFill = (pexSOFAS *)pPEXOC;
    PARSER_PTR(ptr);
    CARD16	    i,j,k;
    miConnListList  *pCLL;
    miConnList	    *pCList;
    ddPointer	    rptr = 0;
    ddpex2rtn	    err = Success;
    ddPointer	    facetPtr = 0;
    ddPointer	    vertexPtr = 0;
    int		    facetSize = 0;
    int		    vertexSize = 0;
    int		    edgeSize = 0;
    extern void destroySOFAS();

    facetSize = CountFacetOptData(  ptr, (CARD16)(pFill->colourType),
				    (CARD32)(pFill->numFAS),
				    pFill->FAS_Attributes);
    vertexSize = CountVertexData(   ptr, pFill->colourType,
				    (CARD32)(pFill->numVertices),
				    pFill->vertexAttributes);
    if (pFill->edgeAttributes) {
	edgeSize = pFill->numEdges * sizeof(ddUCHAR);
	edgeSize += ((4 - (edgeSize & 3)) & 3);	/* force lword alignment for
						    connects data */
    }

    GET_DD_STORAGE( ddFill, miSOFASStruct, (sizeof(miSOFASStruct) + 
		    sizeof(listofddPoint) + 
		    facetSize + vertexSize + edgeSize +
		    (pFill->numFAS * sizeof(miConnListList))));
    EXTRACT_CARD16(ddFill->shape, ptr);
    ddFill->contourHint = pFill->contourHint;
    ddFill->contourCountsFlag = pFill->contourCountsFlag;
    ddFill->numFAS = pFill->numFAS;
    ddFill->numEdges = pFill->numEdges;
    ptr = (ddPointer)(pFill+1);
    ddFill->points.ddList = (listofddPoint *)(ddFill + 1);
    ddFill->points.flags = 0;
    ddFill->points.numLists = 1;
    ddFill->points.maxLists = 1;

    facetPtr = (ddPointer)(ddFill->points.ddList + 1);
    ParseFacetOptData(	ptr, (CARD16)(pFill->colourType),
			(CARD32)(pFill->numFAS),
			pFill->FAS_Attributes, &(ddFill->pFacets),
			facetPtr, &rptr);
    ptr = rptr;

    vertexPtr = facetPtr + facetSize;
    ParseVertexData(	ptr, pFill->colourType, (CARD32)(pFill->numVertices),
			pFill->vertexAttributes, ddFill->points.ddList,
			&vertexPtr, &(ddFill->points.type), &rptr);
    ptr = rptr;

    ddFill->edgeAttribs = pFill->edgeAttributes;
    if (pFill->edgeAttributes){
	ddFill->edgeData = (ddUCHAR *)vertexPtr;
	EXTRACT_STRUCT(ddFill->numEdges, ddUCHAR, ddFill->edgeData, ptr);
	SKIP_PADDING( ptr, ((4 - (ddFill->numEdges & 3)) & 3) );
    }
    else { ddFill->edgeData = 0; }
    vertexPtr += edgeSize;

    ddFill->connects.numListLists = pFill->numFAS;
    ddFill->connects.data = (miConnListList *)vertexPtr;
    ddFill->connects.maxData = ddFill->numFAS * sizeof(miConnListList);
    for (i=0, pCLL = ddFill->connects.data; i<pFill->numFAS; i++, pCLL++) {
	EXTRACT_CARD16(pCLL->numLists,ptr);
	GET_MORE_STORAGE(   pCLL->pConnLists, miConnList,
			    pCLL->numLists * sizeof(miConnList));
	if (err) { 
	    destroySOFAS(ddFill);
	    return(BadAlloc);
	}
	pCLL->maxData = pCLL->numLists * sizeof(miConnList);
	for (j=0, pCList=pCLL->pConnLists; j<pCLL->numLists; j++, pCList++) {
	    EXTRACT_CARD16(pCList->numLists,ptr);
	    GET_MORE_STORAGE(	pCList->pConnects, ddUSHORT, 
				pCList->numLists * sizeof(ddUSHORT));
	    if (err) {
		destroySOFAS(ddFill);
		return(BadAlloc);
	    }
	    EXTRACT_STRUCT(pCList->numLists, ddUSHORT, pCList->pConnects, ptr);
	    pCList->maxData = pCList->numLists * sizeof(ddUSHORT);
	}
    }

    OC_PARSER_RETURN(ddFill);
}



OC_PARSER_FUNC_HEADER(TriangleStrip)
{
    miTriangleStripStruct   *ddTriangle;
    PARSER_PTR(ptr);
    pexTriangleStrip	    *pTriangle = (pexTriangleStrip *)pPEXOC;
    ddPointer		    rptr = 0;
    ddpex2rtn		    err = Success;
    ddPointer		    facetPtr = 0;
    int			    facetSize = 0;
    ddPointer		    vertexPtr = 0;
    int			    vertexSize = 0;
    
    facetSize = CountFacetOptData(  ptr, (CARD16)(pTriangle->colourType),
				    (CARD32)(pTriangle->numVertices - 2),
				    pTriangle->facetAttribs);
    vertexSize = CountVertexData(   ptr, pTriangle->colourType,
				    pTriangle->numVertices,
				    pTriangle->vertexAttribs);
    GET_DD_STORAGE( ddTriangle, miTriangleStripStruct,
		    (sizeof(miTriangleStripStruct) + sizeof(listofddFacet)
		    + sizeof(listofddPoint) + facetSize + vertexSize));
    ddTriangle->pFacets = (listofddFacet *)(ddTriangle+1);
    ddTriangle->points.numLists = 1;
    ddTriangle->points.maxLists = 1;
    ddTriangle->points.ddList = (listofddPoint *)((ddTriangle->pFacets)+1);
    ptr = (ddPointer)(pTriangle +1);
    
    facetPtr = (ddPointer)(ddTriangle->points.ddList + 1);
    ParseFacetOptData(	ptr, (CARD16)(pTriangle->colourType), 
			(pTriangle->numVertices - 2),
			pTriangle->facetAttribs, ddTriangle->pFacets,
			facetPtr, &rptr);
    ptr = rptr;
				
    vertexPtr = facetPtr + facetSize;
    ParseVertexData(	ptr, pTriangle->colourType, pTriangle->numVertices,
			pTriangle->vertexAttribs, ddTriangle->points.ddList,
			&vertexPtr, &(ddTriangle->points.type), &rptr);
    ptr = rptr;

    OC_PARSER_RETURN(ddTriangle);
}


OC_PARSER_FUNC_HEADER(QuadrilateralMesh)
{
    miQuadMeshStruct	    *ddQuad;
    PARSER_PTR(ptr);
    pexQuadrilateralMesh    *pQuad = (pexQuadrilateralMesh *)pPEXOC;
    ddPointer		    rptr = 0;
    ddpex2rtn		    err = Success;
    ddPointer		    facetPtr = 0;
    int			    facetSize = 0;
    ddPointer		    vertexPtr = 0;
    int			    vertexSize = 0;

    facetSize = CountFacetOptData(  ptr, (CARD16)(pQuad->colourType),
				    (CARD32)((pQuad->mPts-1)*(pQuad->nPts-1)),
				    pQuad->facetAttribs);
    vertexSize = CountVertexData(   ptr, pQuad->colourType,
				    (CARD32)(pQuad->mPts * pQuad->nPts),
				    pQuad->vertexAttribs);
    GET_DD_STORAGE( ddQuad, miQuadMeshStruct,
		    (sizeof(miQuadMeshStruct) + facetSize + vertexSize
		    + sizeof(listofddFacet) + sizeof(listofddPoint)));
    ddQuad->pFacets = (listofddFacet *)(ddQuad+1);
    ddQuad->points.numLists = 1;
    ddQuad->points.maxLists = 1;
    ddQuad->points.ddList = (listofddPoint *)((ddQuad->pFacets)+1);
    ddQuad->mPts = pQuad->mPts;
    ddQuad->nPts = pQuad->nPts;
    ddQuad->shape = (ddUSHORT)(pQuad->shape);
    ptr = (ddPointer)(pQuad+1);
    
    /** Now we should be at the head of the opt data **/
    facetPtr = (ddPointer)(ddQuad->points.ddList + 1);
    ParseFacetOptData(	ptr, (CARD16)(pQuad->colourType), 
			(CARD32)((pQuad->mPts-1)*(pQuad->nPts-1)),
			pQuad->facetAttribs, ddQuad->pFacets,
			facetPtr, &rptr);
    ptr = rptr;

    vertexPtr = facetPtr + facetSize;
    ParseVertexData(	ptr, pQuad->colourType,
			(CARD32)(pQuad->mPts*pQuad->nPts),
			pQuad->vertexAttribs, ddQuad->points.ddList,
			&vertexPtr, &(ddQuad->points.type), &rptr);
    ptr = rptr;

    OC_PARSER_RETURN(ddQuad);
}


OC_PARSER_FUNC_HEADER(NurbSurface)
{
    miNurbSurfaceStruct	*ddNurb;
    PARSER_PTR(ptr);
    pexNurbSurface	*pNurb = (pexNurbSurface *)pPEXOC;
    ddULONG		 i, j;
    listofTrimCurve	*ddTrim;
    ddTrimCurve		*ddtc;
    ddUSHORT		type;
    ddpex2rtn		err = Success;

    GET_DD_STORAGE(ddNurb, miNurbSurfaceStruct, (sizeof(miNurbSurfaceStruct)
	+ (pNurb->numUknots * pNurb->numVknots) * (sizeof(ddFLOAT))
	+ (sizeof(listofddPoint))
	+ pNurb->mPts * pNurb->nPts * sizeof(ddCoord4D)
	+ (sizeof(listofTrimCurve))
	+ pNurb->numLists * sizeof(ddTrimCurve)));

    ddNurb->pUknots = (ddFLOAT *)(ddNurb+1);
    ddNurb->pVknots = (ddFLOAT *)((ddNurb->pUknots) + pNurb->numUknots);
    ddNurb->points.ddList =
		(listofddPoint *)((ddNurb->pVknots) + pNurb->numVknots);
    ddNurb->points.ddList->pts.ptr = (char *)(ddNurb->points.ddList + 1);
    ddNurb->trimCurves =
	  (listofTrimCurve *)((ddNurb->points.ddList->pts.ptr) 
			  + (pNurb->mPts * pNurb->nPts) * sizeof(ddCoord4D));

    SKIP_PADDING(ptr, 2);		/* place holder for type */
    EXTRACT_CARD16(ddNurb->uOrder, ptr);
    EXTRACT_CARD16(ddNurb->vOrder, ptr);
    SKIP_PADDING(ptr, 2);
    EXTRACT_CARD32(ddNurb->numUknots, ptr);
    EXTRACT_CARD32(ddNurb->numVknots, ptr);
    EXTRACT_CARD16(ddNurb->mPts, ptr);
    EXTRACT_CARD16(ddNurb->nPts, ptr);
    EXTRACT_CARD32(ddNurb->numTrimCurveLists, ptr);	/* is pNurb->numLists */

    EXTRACT_STRUCT(ddNurb->numUknots, PEXFLOAT, ddNurb->pUknots, ptr);
    EXTRACT_STRUCT(ddNurb->numVknots, PEXFLOAT, ddNurb->pVknots, ptr);

    ddNurb->points.numLists = 1;
    ddNurb->points.maxLists = 1;
    ddNurb->points.ddList->numPoints = ddNurb->mPts * ddNurb->nPts;
    if (pNurb->type == PEXRational) {
	ddNurb->points.type = DD_HOMOGENOUS_POINT;
	EXTRACT_STRUCT(	ddNurb->points.ddList->numPoints, ddCoord4D, 
			ddNurb->points.ddList->pts.p4Dpt, ptr);
    } else {
	ddNurb->points.type = DD_3D_POINT;
	EXTRACT_STRUCT(	ddNurb->points.ddList->numPoints, ddCoord3D, 
			ddNurb->points.ddList->pts.p3Dpt, ptr);
    }

    for (   i=0, ddTrim = ddNurb->trimCurves;
	    i<ddNurb->numTrimCurveLists;
	    i++, ddTrim++) {
	EXTRACT_CARD32(ddTrim->count, ptr);
	GET_MORE_STORAGE(  ddTrim->pTC, ddTrimCurve,
			    ddTrim->count*sizeof(ddTrimCurve));
	if (err) {
	    destroyNurbSurface(ddNurb);
	    return (BadAlloc);
	}

	for (	j=0, ddtc = ddTrim->pTC;
		j < ddTrim->count;
		j++, ddtc++) {
	    EXTRACT_CARD8(ddtc->visibility, ptr);
	    SKIP_PADDING(ptr, 1);
	    EXTRACT_CARD16(ddtc->order, ptr);
	    EXTRACT_CARD16(type, ptr);
	    EXTRACT_CARD16(ddtc->curveApprox.approxMethod, ptr);
	    EXTRACT_FLOAT(ddtc->curveApprox.tolerance, ptr);
	    EXTRACT_FLOAT(ddtc->uMin, ptr);
	    EXTRACT_FLOAT(ddtc->uMax, ptr);
	    EXTRACT_CARD32(ddtc->numKnots, ptr);
	    EXTRACT_CARD32(ddtc->points.numPoints, ptr);
	    GET_MORE_STORAGE(	ddtc->pKnots, ddFLOAT,
				sizeof(ddFLOAT) * ddtc->numKnots );
	    if (err) {
		ddtc->points.pts.ptr = 0;
		destroyNurbSurface(ddNurb);
		return(BadAlloc);
	    }
	    EXTRACT_STRUCT( ddtc->numKnots, PEXFLOAT, ddtc->pKnots, ptr);
	    if (type == PEXRational) {
		/* Note this only works because these points are never
								transformed */
		ddtc->pttype = DD_3D_POINT;
		ddtc->points.pts.p3Dpt = 0;
		GET_MORE_STORAGE(   ddtc->points.pts.p3Dpt, ddCoord3D,
				    sizeof(ddCoord3D)*ddtc->points.numPoints);
		EXTRACT_STRUCT(	ddtc->points.numPoints, ddCoord3D,
				ddtc->points.pts.p3Dpt, ptr);
	    } else {
		ddtc->pttype = DD_2D_POINT;
		ddtc->points.pts.p2Dpt = 0;
		GET_MORE_STORAGE(   ddtc->points.pts.p2Dpt, ddCoord2D,
				    sizeof(ddCoord2D)*ddtc->points.numPoints );
		EXTRACT_STRUCT(	ddtc->points.numPoints, ddCoord2D,
				ddtc->points.pts.p2Dpt, ptr);
	    }
	    if (err) {
		destroyNurbSurface(ddNurb);
		return(BadAlloc);
	    }
	}
    }

    OC_PARSER_RETURN(ddNurb);

}
 

OC_PARSER_FUNC_HEADER(CellArray2D)
{
    miCellArrayStruct	*ddCell;
    PARSER_PTR(ptr);
    pexCellArray2D	*pCell = (pexCellArray2D *)pPEXOC;
    
    GET_DD_STORAGE( ddCell, miCellArrayStruct, sizeof(miCellArrayStruct)
		    + sizeof(listofddPoint) + 2 * sizeof(ddCoord2D)
		    + pCell->dx * pCell->dy * sizeof(ddIndexedColour));
    ddCell->point.ddList = (listofddPoint *)(ddCell+1);

    ddCell->point.type = DD_2D_POINT;
    ddCell->point.numLists = 1;
    ddCell->point.maxLists = 1;
    ddCell->point.ddList->numPoints = 2;
    ddCell->point.ddList->pts.p2Dpt =
	    (ddCoord2D *)((ddCell->point.ddList) + 1);
    EXTRACT_LISTOF_COORD2D(2, ddCell->point.ddList->pts.p2Dpt, ptr);

    EXTRACT_CARD32(ddCell->dx, ptr);
    EXTRACT_CARD32(ddCell->dy, ptr);
    ddCell->colours.colour.pIndex =
	    (ddIndexedColour *)((ddCell->point.ddList->pts.p2Dpt) + 2);
    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddIndexedColour,
		    ddCell->colours.colour.pIndex, ptr);
    
    OC_PARSER_RETURN(ddCell);
}


OC_PARSER_FUNC_HEADER(CellArray)
{
    miCellArrayStruct	*ddCell;
    PARSER_PTR(ptr);
    pexCellArray	*pCell = (pexCellArray *)pPEXOC;
    
    GET_DD_STORAGE( ddCell, miCellArrayStruct, sizeof(miCellArrayStruct)
		    + sizeof(listofddPoint) + 3 * sizeof(ddCoord3D)
		    + pCell->dx * pCell->dy * sizeof(ddIndexedColour));
    ddCell->point.ddList = (listofddPoint *)(ddCell+1);

    ddCell->point.type = DD_3D_POINT;
    ddCell->point.numLists = 1;
    ddCell->point.maxLists = 1;
    ddCell->point.ddList->numPoints = 3;
    ddCell->point.ddList->pts.p3Dpt =
	    (ddCoord3D *)((ddCell->point.ddList) + 1);
    EXTRACT_LISTOF_COORD3D(3, ddCell->point.ddList->pts.p3Dpt, ptr);

    EXTRACT_CARD32(ddCell->dx, ptr);
    EXTRACT_CARD32(ddCell->dy, ptr);
    ddCell->colours.colour.pIndex =
	    (ddIndexedColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddIndexedColour,
		    ddCell->colours.colour.pIndex, ptr);
    
    OC_PARSER_RETURN(ddCell);
}


OC_PARSER_FUNC_HEADER(ExtCellArray)
{
    miCellArrayStruct	*ddCell;
    PARSER_PTR(ptr);
    pexExtCellArray	*pCell = (pexExtCellArray *)pPEXOC;
    unsigned long	size;

    size = (((pCell->colourType==PEXIndexedColour)
	    || (pCell->colourType==PEXRgb8Colour))  ? 4 :
		((pCell->colourType==PEXRgb16Colour) ? 8 : 12 ));
    GET_DD_STORAGE( ddCell, miCellArrayStruct, sizeof(miCellArrayStruct)
		    + sizeof(listofddPoint) + 3 * sizeof(ddCoord3D)
		    +  pCell->dx * pCell->dy * size);
    ddCell->point.ddList = (listofddPoint *)(ddCell+1);

    EXTRACT_CARD16(ddCell->colours.colourType, ptr);
    SKIP_PADDING(ptr, 2);
    
    ddCell->point.type = DD_3D_POINT;
    ddCell->point.numLists = 1;
    ddCell->point.maxLists = 1;
    ddCell->point.ddList->numPoints = 3;
    ddCell->point.ddList->pts.p3Dpt =
			    (ddCoord3D *)((ddCell->point.ddList) + 1);
    EXTRACT_LISTOF_COORD3D(3, ddCell->point.ddList->pts.p3Dpt, ptr);
    EXTRACT_CARD32(ddCell->dx, ptr);
    EXTRACT_CARD32(ddCell->dy, ptr);
    
    switch (pCell->colourType) {

	case PEXIndexedColour: {
	    ddCell->colours.colour.pIndex =
		    (ddIndexedColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddIndexedColour, 
			    ddCell->colours.colour.pIndex, ptr);
	    break; }

	case PEXRgbFloatColour : {
	    ddCell->colours.colour.pRgbFloat =
		    (ddRgbFloatColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddRgbFloatColour, 
			    ddCell->colours.colour.pRgbFloat, ptr);
	    break; }

	case PEXCieFloatColour : {
	    ddCell->colours.colour.pCie =
		    (ddCieColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddCieColour, 
			    ddCell->colours.colour.pCie, ptr);
	    break; }

	case PEXHsvFloatColour : {
	    ddCell->colours.colour.pHsv =
		    (ddHsvColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddHsvColour, 
			    ddCell->colours.colour.pHsv, ptr);
	    break; }

	case PEXHlsFloatColour : {
	    ddCell->colours.colour.pHls =
		    (ddHlsColour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddHlsColour, 
			    ddCell->colours.colour.pHls, ptr);
	    break; }

	case PEXRgb8Colour  : {
	    ddCell->colours.colour.pRgb8 =
		    (ddRgb8Colour *)((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddRgb8Colour, 
			    ddCell->colours.colour.pRgb8, ptr);
	    break; }

	case PEXRgb16Colour : {
	    ddCell->colours.colour.pRgb16 =
		    (ddRgb16Colour *)	((ddCell->point.ddList->pts.p3Dpt) + 3);
	    EXTRACT_STRUCT( ddCell->dx * ddCell->dy, ddRgb16Colour, 
			    ddCell->colours.colour.pRgb16, ptr);
	    break; }

    }

    OC_PARSER_RETURN(ddCell);
    
}
 

OC_PARSER_FUNC_HEADER(PSurfaceChars)
{
    miPSurfaceCharsStruct   *ddPSC;
    ddULONG		    sType = 0;
    ddPointer		    ptr = (ddPointer)pPEXOC;

    sType = ((pexParaSurfCharacteristics *)pPEXOC)->characteristics;
    SKIP_PADDING(ptr,sizeof(pexParaSurfCharacteristics));

    switch (sType) {
	case PEXPSCNone:
	case PEXPSCImpDep:
	    GET_DD_STORAGE( ddPSC, miPSurfaceCharsStruct,
			    sizeof(miPSurfaceCharsStruct));
	    break;

	case PEXPSCIsoCurves: {
	    GET_DD_STORAGE( ddPSC, miPSurfaceCharsStruct,
			    sizeof(miPSurfaceCharsStruct)
			    + sizeof(ddPSC_IsoparametricCurves));
	    ddPSC->data.pIsoCurves = (ddPSC_IsoparametricCurves *)(ddPSC + 1);
	    EXTRACT_CARD16(ddPSC->data.pIsoCurves->placementType, ptr);
	    SKIP_PADDING(ptr, 2);
	    EXTRACT_CARD16(ddPSC->data.pIsoCurves->numUcurves, ptr);
	    EXTRACT_CARD16(ddPSC->data.pIsoCurves->numVcurves, ptr);
	    break; }

	case PEXPSCMcLevelCurves: {
	    GET_DD_STORAGE( ddPSC, miPSurfaceCharsStruct,
			    sizeof(miPSurfaceCharsStruct)
			    + sizeof(ddPSC_LevelCurves));
	    ddPSC->data.pMcLevelCurves = (ddPSC_LevelCurves *)(ddPSC + 1);
	    EXTRACT_STRUCT( 1,ddPSC_LevelCurves,ddPSC->data.pMcLevelCurves, ptr);
	    break; }

	case PEXPSCWcLevelCurves: {
	    GET_DD_STORAGE( ddPSC, miPSurfaceCharsStruct,
			    sizeof(miPSurfaceCharsStruct)
			    + sizeof(ddPSC_LevelCurves));
	    ddPSC->data.pWcLevelCurves = (ddPSC_LevelCurves *)(ddPSC + 1);
	    EXTRACT_STRUCT( 1,ddPSC_LevelCurves,ddPSC->data.pWcLevelCurves, ptr);
	    break; }
    }

    ddPSC->type = sType;

    OC_PARSER_RETURN(ddPSC);
}




OC_PARSER_FUNC_HEADER(Gdp2D)
{
    miGdpStruct	    *ddGdp;
    pexGdp2D	    *pGdp = (pexGdp2D *)pPEXOC;
    PARSER_PTR(ptr);

    GET_DD_STORAGE( ddGdp, miGdpStruct, (sizeof(miGdpStruct) +
		    sizeof(listofddPoint) + pGdp->numBytes
		    + pGdp->numPoints * sizeof(ddCoord2D)));
    ddGdp->points.ddList = (listofddPoint *)(ddGdp+1);
    ddGdp->GDPid = pGdp->gdpId;
    ddGdp->points.ddList->numPoints = pGdp->numPoints;
    ddGdp->numBytes = pGdp->numBytes;
    ddGdp->points.type = DD_2D_POINT;
    ddGdp->points.numLists = 1;
    ddGdp->points.maxLists = 1;
    ddGdp->points.ddList->pts.p2Dpt = (ddCoord2D *)((ddGdp->points.ddList) + 1);
    ptr = (ddPointer)(pGdp+1);
    EXTRACT_LISTOF_COORD2D(ddGdp->points.ddList->numPoints,
			  ddGdp->points.ddList->pts.p2Dpt, ptr);
    ddGdp->pData = ((ddUCHAR *)(ddGdp->points.ddList))
		    + pGdp->numPoints * sizeof(ddCoord2D);
    EXTRACT_STRUCT( ddGdp->numBytes, ddUCHAR, ddGdp->pData, ptr);

    OC_PARSER_RETURN(ddGdp);

}


OC_PARSER_FUNC_HEADER(Gdp)
{
    miGdpStruct	    *ddGdp;
    pexGdp	    *pGdp = (pexGdp *)pPEXOC;
    PARSER_PTR(ptr);

    GET_DD_STORAGE( ddGdp, miGdpStruct, (sizeof(miGdpStruct) +
		    sizeof(listofddPoint) + pGdp->numBytes
		    + pGdp->numPoints * sizeof(ddCoord3D)));
    ddGdp->points.ddList = (listofddPoint *)(ddGdp+1);
    ddGdp->GDPid = pGdp->gdpId;
    ddGdp->points.ddList->numPoints = pGdp->numPoints;
    ddGdp->numBytes = pGdp->numBytes;
    ddGdp->points.type = DD_3D_POINT;
    ddGdp->points.numLists = 1;
    ddGdp->points.maxLists = 1;
    ddGdp->points.ddList->pts.p3Dpt = (ddCoord3D *)((ddGdp->points.ddList) + 1);
    ptr = (ddPointer)(pGdp+1);
    EXTRACT_LISTOF_COORD3D(ddGdp->points.ddList->numPoints,
			  ddGdp->points.ddList->pts.p3Dpt, ptr);
    ddGdp->pData = ((ddUCHAR *)(ddGdp->points.ddList))
		    + pGdp->numPoints * sizeof(ddCoord3D);
    EXTRACT_STRUCT( ddGdp->numBytes, ddUCHAR, ddGdp->pData, ptr);

    OC_PARSER_RETURN(ddGdp);
    
}


OC_PARSER_FUNC_HEADER(SetAttribute)
{
    /** The function vector should be set up to have this
     ** SetAttribute function as the entry for all of the OC entries other
     ** than those listed above or those NULL'd out
     **/

    ddElementInfo  *dstAttrib;

    GET_DD_STORAGE( dstAttrib, ddElementInfo,
		    pPEXOC->length * sizeof(CARD32));

    memcpy(  (char *)dstAttrib, (char *)pPEXOC, 
	    (int)(pPEXOC->length * sizeof(CARD32)));

    OC_PARSER_RETURN(pPEXOC);
}


OC_PARSER_FUNC_HEADER(PropOC)
{
    /** This handles storing ProprietaryOC 
     **/

    ddElementInfo  *dstPropOC;

    GET_DD_STORAGE( dstPropOC, ddElementInfo,
		    pPEXOC->length * sizeof(CARD32));

    memcpy(  (char *)dstPropOC, (char *)pPEXOC, 
	    (int)(pPEXOC->length * sizeof(CARD32)));

    OC_PARSER_RETURN(pPEXOC);
}

