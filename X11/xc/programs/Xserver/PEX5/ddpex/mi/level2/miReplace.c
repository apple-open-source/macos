/* $Xorg: miReplace.c,v 1.4 2001/02/09 02:04:10 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/miReplace.c,v 3.8 2001/12/14 19:57:30 dawes Exp $ */


#include "ddpex2.h"
#include "miStruct.h"
#include "pexExtract.h"
#include "pexUtils.h"
#include "miLight.h"
#include "pexos.h"


/**  Replace functions:
 **	Each takes two parameters: a pointer to the element to be 
 **	parsed (in PEX format) and a pointer to a pointer to return the 
 **	parsed element (in server native internal format).
 **
 **	See comments in pexOCParse.c
 **
 **	The routines in this file are exceptions to the symmetry
 **	in parsing that allows us to use the same routines for creation
 **	and replacement.
 **
 **	Note that these routines DO NOT allocate any memory; the calling
 **	routines must ensure that sufficient memory has been allocated
 **	in which to store the parsed element.  Coders adding routines
 **	to this file must obide by this rule.
 **/


extern void ParseFacetOptData();
extern void ParseVertexData();
extern int  CountFacetOptData();
extern int  CountVertexData();

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define CAT(a,b)    a##b
#else
#define CAT(a,b)    a/**/b
#endif

#define OC_REPLACE_FUNC_HEADER(suffix)				    \
    ddpex2rtn CAT(replace,suffix)(pPEXOC, ppExecuteOC)		    \
    ddElementInfo	*pPEXOC;		/* PEX format */    \
    miGenericElementPtr	*ppExecuteOC;		/* internal format */
    
#define OC_REPLACE_RETURN(ANSWER)        \
    return(Success);

#define PARSER_PTR(PTR)  \
    ddPointer PTR = (ddPointer)(pPEXOC + 1)

#define LEN_WO_HEADER(OCtype) (pPEXOC->length * sizeof(CARD32) - sizeof(OCtype))

/* Note this macro assumes that if the old size is the same as the new
   size, then the replace in place can happen.  This may not be sufficient
   proof.  Counterexamples are dealt with on a case by case basis
   (e.g., see replaceLightState below).
 */
#define CHECK_REPLACE(DD_ST, TYPE)				\
    if (!*ppExecuteOC) return (BadAlloc);			\
    if (pPEXOC->length != (*ppExecuteOC)->element.pexOClength)	\
	return (BadAlloc);					\
    (DD_ST) = (TYPE *)((*ppExecuteOC)+1);



OC_REPLACE_FUNC_HEADER(LightState)
{
    miLightStateStruct	*ddLightState;
    pexLightState	*pLight = (pexLightState *)pPEXOC;
    extern ddpex2rtn parseLightState();

    CHECK_REPLACE( ddLightState, miLightStateStruct);

    /*
	may have padding due to pointer alignment insurance
     */
    if ((MAKE_EVEN(pLight->numEnable) + MAKE_EVEN(pLight->numDisable))
	!= (MAKE_EVEN(ddLightState->enableList->numObj)
	    + MAKE_EVEN(ddLightState->disableList->numObj)))
	return(BadAlloc);

    return(parseLightState(pPEXOC, ppExecuteOC));
}


OC_REPLACE_FUNC_HEADER(SOFAS)
{
    miSOFASStruct   *ddFill;
    pexSOFAS	    *pFill = (pexSOFAS *)pPEXOC;
    PARSER_PTR(ptr);
    CARD16	    i,j;
    miConnListList  *pCLL;
    miConnList	    *pCList;
    ddPointer	    rptr = 0;
    ddPointer	    vertexPtr = 0;
    ddPointer	    facetPtr = 0;
    int		    edgeSize = 0;
    int		    facetSize = 0;
    int		    vertexSize = 0;
    ddpex2rtn	    err = Success;

    CHECK_REPLACE( ddFill, miSOFASStruct);
    if (    (pFill->numFAS != ddFill->numFAS)
	||  (pFill->numEdges != ddFill->numEdges)
	||  (pFill->numVertices != ddFill->points.ddList->maxData))
	return(BadAlloc);   /* still not a sufficient check... */

    facetSize = CountFacetOptData(  ptr, (CARD16)(pFill->colourType),
				    (CARD32)(pFill->numFAS),
				    pFill->FAS_Attributes);
    vertexSize = CountVertexData(   ptr, pFill->colourType,
				    (CARD32)(pFill->numVertices),
				    pFill->vertexAttributes);
    if (pFill->edgeAttributes){
	edgeSize = pFill->numEdges * sizeof(ddUCHAR);
	edgeSize += ((4 - (edgeSize & 3)) & 3);
    }

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
    if (pFill->edgeAttributes) {
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
	pCLL->maxData = pCLL->numLists * sizeof(miConnList);
	for (j=0, pCList=pCLL->pConnLists; j<pCLL->numLists; j++, pCList++) {
	    EXTRACT_CARD16(pCList->numLists,ptr);
	    EXTRACT_STRUCT(pCList->numLists, ddUSHORT, pCList->pConnects, ptr);
	    pCList->maxData = pCList->numLists * sizeof(ddUSHORT);
	}
    }
    OC_REPLACE_RETURN(ddFill);

}



OC_REPLACE_FUNC_HEADER(NurbSurface)
{
    miNurbSurfaceStruct	*ddNurb;
    PARSER_PTR(ptr);
    pexNurbSurface	*pNurb = (pexNurbSurface *)pPEXOC;
    ddULONG		 i, j, k;
    listofTrimCurve	*ddTrim;
    ddTrimCurve		*ddtc;
    ddUSHORT		type;

    CHECK_REPLACE( ddNurb, miNurbSurfaceStruct);
    if (    (pNurb->numUknots != ddNurb->numUknots)
	||  (pNurb->numVknots != ddNurb->numVknots)
	||  (pNurb->mPts != ddNurb->mPts)
	||  (pNurb->nPts != ddNurb->nPts)
	||  (pNurb->numLists != ddNurb->numTrimCurveLists)
	||  (pNurb->uOrder != ddNurb->uOrder)
	||  (pNurb->vOrder != ddNurb->vOrder)
	||  (	    (pNurb->type == PEXRational)
		&&  (ddNurb->points.type != DD_HOMOGENOUS_POINT))
	||  (	    (pNurb->type == PEXNonRational)
		&&  (ddNurb->points.type != DD_3D_POINT)))
	return(BadAlloc);	/* still not a sufficient check... */

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
    if (pNurb->type == PEXRational) {
	ddNurb->points.type = DD_HOMOGENOUS_POINT;
	EXTRACT_STRUCT(	ddNurb->mPts * ddNurb->nPts, ddCoord4D, 
			ddNurb->points.ddList->pts.p4Dpt, ptr);
    } else {
	ddNurb->points.type = DD_3D_POINT;
	EXTRACT_STRUCT(	ddNurb->mPts * ddNurb->nPts, ddCoord3D, 
			ddNurb->points.ddList->pts.p3Dpt, ptr);
    }

    for (   i=0, ddTrim = ddNurb->trimCurves;
	    i<ddNurb->numTrimCurveLists;
	    i++, ddTrim++) {
	EXTRACT_CARD32(ddTrim->count, ptr);

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
	    EXTRACT_STRUCT( ddtc->numKnots, PEXFLOAT, ddtc->pKnots, ptr);
	    if (type == PEXRational) {
		/* Note this only works because these points are never
								transformed */
		ddtc->pttype = DD_3D_POINT;
		ddtc->points.pts.p3Dpt = 0;
		EXTRACT_STRUCT(	ddtc->points.numPoints, ddCoord3D,
				ddtc->points.pts.p3Dpt, ptr);
	    } else {
		ddtc->pttype = DD_2D_POINT;
		ddtc->points.pts.p2Dpt = 0;
		EXTRACT_STRUCT(	ddtc->points.numPoints, ddCoord2D,
				ddtc->points.pts.p2Dpt, ptr);
	    }
	}
    }
    OC_REPLACE_RETURN(ddNurb);

}
 
