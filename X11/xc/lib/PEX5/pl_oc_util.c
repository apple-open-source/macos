/* $Xorg: pl_oc_util.c,v 1.5 2001/02/09 02:03:28 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"
#include "pl_oc_util.h"


Status
PEXStartOCs (display, resource_id, req_type, float_format, numOCs, numWords)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		float_format;
INPUT int		numOCs;
INPUT int		numWords;

{
    register PEXDisplayInfo 	*pexDisplayInfo;
    register pexOCRequestHeader *pReq;
    char			*currentReq;


    /*
     * Is the oc larger than the protocol max request size?
     * If so, return an error.
     */

    if (numWords + LENOF (pexOCRequestHeader) > MAX_REQUEST_SIZE)
        return (0);


    /*
     * If possible add the OC to a ROC or StoreElements request,
     * otherwise start a new request.
     */
    
    LockDisplay (display);
    PEXGetDisplayInfo (display, pexDisplayInfo);

    currentReq = (XBufferFlushed (display)) ? (NULL) : (display->last_req);

    if (currentReq == NULL ||
	pexDisplayInfo->lastReqNum != display->request ||
	pexDisplayInfo->lastResID != resource_id ||
	pexDisplayInfo->lastReqType != req_type ||
	req_type == PEXOCRenderSingle || req_type == PEXOCStoreSingle ||
	display->synchandler ||
	(LENOF (pexOCRequestHeader) + numWords > WordsLeftInXBuffer (display)))
    {
	PEXGetOCReq (SIZEOF (pexOCRequestHeader) + NUMBYTES (numWords));
	
	pexDisplayInfo->lastResID = resource_id;
	pexDisplayInfo->lastReqType = req_type;
	pexDisplayInfo->lastReqNum = display->request;
	
	BEGIN_NEW_OCREQ_HEADER (display->bufptr, pReq);

	pReq->extOpcode = pexDisplayInfo->extOpcode;
	pReq->pexOpcode =
	    (req_type == PEXOCStore || req_type == PEXOCStoreSingle) ?
	    PEXRCStoreElements : PEXRCRenderOutputCommands;
	pReq->reqLength = LENOF (pexOCRequestHeader) + numWords;
	pReq->fpFormat = float_format;
	pReq->target = resource_id;
	pReq->numCommands = numOCs;

	END_NEW_OCREQ_HEADER (display->bufptr, pReq);

        display->bufptr += SIZEOF (pexOCRequestHeader);
    }
    else
    {
	BEGIN_UPDATE_OCREQ_HEADER (currentReq, pReq);

	pReq->reqLength += numWords;
	pReq->numCommands += numOCs;

	END_UPDATE_OCREQ_HEADER (currentReq, pReq);
    }
    
    return (1);
}


void
PEXFinishOCs (display)

INPUT Display	*display;

{
    UnlockDisplay (display);
}


 /*
  * PEXlib uses an internal macro called PEXCopyBytesToOC, so undef here.
  * The macro does the same work as this function, but avoids a function call.
  */

#ifdef PEXCopyBytesToOC
#undef PEXCopyBytesToOC
#endif

void
PEXCopyBytesToOC (display, numBytes, data) 

INPUT Display		*display;
INPUT int		numBytes;
INPUT char		*data;

{
    if (numBytes <= BytesLeftInXBuffer (display))
    {
	/*
	 * There is room in the X buffer to do the copy.
	 */

	memcpy (display->bufptr, data, numBytes);
	display->bufptr += numBytes;
    }
    else
    {
	/*
	 * Copying this OC will overflow the transport buffer.  We
	 * can't do a simple bcopy.
	 */

	_PEXSendBytesToOC (display, numBytes, data);
    }
}


char *
PEXGetOCAddr (display, numBytes) 

INPUT Display		*display;
INPUT int		numBytes;

{
    PEXDisplayInfo 	*pexDisplayInfo;
    char		*retPtr;


    /*
     * If numBytes is larger than the max allowed size, return error.
     */

    if (numBytes >  PEXGetOCAddrMaxSize (display))
	return (NULL);


    /*
     * If there isn't enough space in the X buffer, flush it
     * and make sure that the next OC starts a new request.
     */

    if (numBytes > BytesLeftInXBuffer (display))
    {
	_XFlush (display);
	PEXGetDisplayInfo (display, pexDisplayInfo);
	pexDisplayInfo->lastReqNum = -1;
    }


    /*
     * Return a pointer to the bytes, and update the display's bufptr.
     */

    retPtr = display->bufptr;
    display->bufptr += numBytes;

    return (retPtr);
}


void
PEXSendOCs (display, resource_id, req_type, float_format,
    oc_count, numBytes, encoded_ocs)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		float_format;
INPUT unsigned long	oc_count;
INPUT unsigned int	numBytes;
INPUT char		*encoded_ocs;

{
    PEXStartOCs (display, resource_id, req_type, float_format,
	oc_count, NUMWORDS (numBytes));

    _PEXCopyPaddedBytesToOC (display, numBytes, encoded_ocs);

    PEXFinishOCs (display);
}


int
PEXGetSizeOCs (float_format, oc_count, oc_data)

INPUT int		float_format;
INPUT int		oc_count;
INPUT PEXOCData		*oc_data;

{
    int		totalSize, i;


    for (i = totalSize = 0; i < oc_count; i++, oc_data++)
    {
	switch (oc_data->oc_type)
	{
	case PEXOCMarkerType:
	case PEXOCLineType:
	case PEXOCATextStyle:
	case PEXOCBFInteriorStyle:
	case PEXOCBFReflectionModel:
	case PEXOCBFSurfaceInterpMethod:
	case PEXOCInteriorStyle:
	case PEXOCPolylineInterpMethod:
	case PEXOCReflectionModel:
	case PEXOCRenderingColorModel:
	case PEXOCSurfaceEdgeType:
	case PEXOCSurfaceInterpMethod:

	    totalSize += SIZEOF (pexMarkerType);
	    break;

	case PEXOCMarkerColorIndex:
	case PEXOCMarkerBundleIndex:
	case PEXOCTextFontIndex:
	case PEXOCTextColorIndex:
	case PEXOCTextBundleIndex:
	case PEXOCLineColorIndex:
	case PEXOCLineBundleIndex:
	case PEXOCSurfaceColorIndex:
	case PEXOCBFInteriorStyleIndex:
	case PEXOCBFSurfaceColorIndex:
	case PEXOCInteriorBundleIndex:
	case PEXOCInteriorStyleIndex:
	case PEXOCSurfaceEdgeColorIndex:
	case PEXOCEdgeBundleIndex:
	case PEXOCViewIndex:
	case PEXOCDepthCueIndex:
	case PEXOCColorApproxIndex:

	    totalSize += SIZEOF (pexMarkerColorIndex);
	    break;

	case PEXOCMarkerColor:
	case PEXOCTextColor:
	case PEXOCLineColor:
	case PEXOCSurfaceColor:
	case PEXOCBFSurfaceColor:
	case PEXOCSurfaceEdgeColor:

	    totalSize += (SIZEOF (pexMarkerColor) +
	        GetColorSize (oc_data->data.SetMarkerColor.color_type));
	    break;

	case PEXOCMarkerScale:
	case PEXOCCharExpansion:
	case PEXOCCharSpacing:
	case PEXOCCharHeight:
	case PEXOCATextHeight:
	case PEXOCLineWidth:
	case PEXOCSurfaceEdgeWidth:

	    totalSize += SIZEOF (pexMarkerScale);
	    break;

	case PEXOCTextPrecision:
	case PEXOCTextPath:
	case PEXOCATextPath:

	    totalSize += SIZEOF (pexTextPrecision);
	    break;

	case PEXOCCharUpVector:
	case PEXOCATextUpVector:

	    totalSize += SIZEOF (pexCharUpVector);
	    break;

	case PEXOCTextAlignment:
	case PEXOCATextAlignment:

	    totalSize += SIZEOF (pexTextAlignment);
	    break;

	case PEXOCCurveApprox:

	    totalSize += SIZEOF (pexCurveApprox);
	    break;

	case PEXOCReflectionAttributes:
	case PEXOCBFReflectionAttributes:

	    totalSize += (SIZEOF (pexReflectionAttributes) + GetColorSize (
                oc_data->data.SetReflectionAttributes.attributes.specular_color.type));
	    break;

	case PEXOCSurfaceApprox:

	    totalSize += SIZEOF (pexSurfaceApprox);
	    break;

	case PEXOCFacetCullingMode:

	    totalSize += SIZEOF (pexFacetCullingMode);
	    break;

	case PEXOCFacetDistinguishFlag:
	case PEXOCSurfaceEdgeFlag:
	case PEXOCModelClipFlag:

	    totalSize += SIZEOF (pexFacetDistinguishFlag);
	    break;

	case PEXOCPatternSize:

	    totalSize += SIZEOF (pexPatternSize);
	    break;

	case PEXOCPatternAttributes2D:

	    totalSize += SIZEOF (pexPatternAttributes2D);
	    break;

	case PEXOCPatternAttributes:

	    totalSize += SIZEOF (pexPatternAttributes);
	    break;

	case PEXOCIndividualASF:

	    totalSize += SIZEOF (pexIndividualASF);
	    break;

	case PEXOCLocalTransform:

	    totalSize += SIZEOF (pexLocalTransform);
	    break;

	case PEXOCLocalTransform2D:

	    totalSize += SIZEOF (pexLocalTransform2D);
	    break;

	case PEXOCGlobalTransform:

	    totalSize += SIZEOF (pexGlobalTransform);
	    break;

	case PEXOCGlobalTransform2D:

	    totalSize += SIZEOF (pexGlobalTransform2D);
	    break;

	case PEXOCModelClipVolume:

	    totalSize += (SIZEOF (pexModelClipVolume) +
    	        SIZEOF (pexHalfSpace) *
	        oc_data->data.SetModelClipVolume.count);
	    break;

	case PEXOCModelClipVolume2D:

	    totalSize += (SIZEOF (pexModelClipVolume2D) +
    	        SIZEOF (pexHalfSpace2D) *
	        oc_data->data.SetModelClipVolume2D.count);
	    break;

	case PEXOCRestoreModelClipVolume:

	    totalSize += SIZEOF (pexRestoreModelClipVolume);
	    break;

	case PEXOCLightSourceState:

	{
    	    int	sizeofEnableList, sizeofDisableList;

    	    sizeofEnableList = SIZEOF (CARD16) *
	        oc_data->data.SetLightSourceState.enable_count;

    	    sizeofDisableList = SIZEOF (CARD16) *
	        oc_data->data.SetLightSourceState.disable_count;
	    
	    totalSize += (SIZEOF (pexLightSourceState) +
	        PADDED_BYTES (sizeofEnableList) +
	        PADDED_BYTES (sizeofDisableList));
	    break;
	}

	case PEXOCPickID:
	case PEXOCHLHSRID:

	    totalSize += SIZEOF (pexPickID);
	    break;

	case PEXOCParaSurfCharacteristics:

	{
	    int	pscDataSize = 0;

	    switch (oc_data->data.SetParaSurfCharacteristics.psc_type)
	    {
	    case PEXPSCIsoCurves:
	        pscDataSize = SIZEOF (pexPSC_IsoparametricCurves);
	        break;

	    case PEXPSCMCLevelCurves:
	    case PEXPSCWCLevelCurves:
	        pscDataSize = SIZEOF (pexPSC_LevelCurves) + SIZEOF (float) *
	    	oc_data->data.SetParaSurfCharacteristics.characteristics.level_curves.count;
	        break;

	    default:
	        break;
	    }

	    totalSize += (SIZEOF (pexParaSurfCharacteristics) + pscDataSize);
	    break;
  	}

	case PEXOCAddToNameSet:
	case PEXOCRemoveFromNameSet:

	    totalSize += (SIZEOF (pexAddToNameSet) + 
	        oc_data->data.AddToNameSet.count * SIZEOF (pexName));
	    break;

	case PEXOCExecuteStructure:

	    totalSize += SIZEOF (pexExecuteStructure);
	    break;

	case PEXOCLabel:

	    totalSize += SIZEOF (pexLabel);
	    break;

	case PEXOCApplicationData:

	    totalSize += (SIZEOF (pexApplicationData) +
	        PADDED_BYTES (oc_data->data.ApplicationData.length));
	    break;

	case PEXOCGSE:

	    totalSize += (SIZEOF (pexGSE) +
	        PADDED_BYTES (oc_data->data.GSE.length));
	    break;

	case PEXOCMarkers:
	case PEXOCPolyline:

	    totalSize += (SIZEOF (pexMarkers) +
	        oc_data->data.Markers.count * SIZEOF (pexCoord3D));
	    break;

	case PEXOCMarkers2D:
	case PEXOCPolyline2D:

	    totalSize += (SIZEOF (pexMarkers2D) +
	        oc_data->data.Markers2D.count * SIZEOF (pexCoord2D));
	    break;

	case PEXOCText:

	{
	    /* Text is always mono encoded */

	    int	lenofStrings;

	    GetStringsLength (oc_data->data.EncodedText.count,
	        oc_data->data.EncodedText.encoded_text, lenofStrings)

	    totalSize += (SIZEOF (pexText) + NUMBYTES (lenofStrings));
	    break;
	}

	case PEXOCText2D:

	{
	    /* Text is always mono encoded */

	    int	lenofStrings;

	    GetStringsLength (oc_data->data.EncodedText2D.count,
	        oc_data->data.EncodedText2D.encoded_text, lenofStrings)

	    totalSize += (SIZEOF (pexText2D) + NUMBYTES (lenofStrings));
	    break;
	}

	case PEXOCAnnotationText:

	{
	    /* Anno Text is always mono encoded */

	    int	lenofStrings;

	    GetStringsLength (oc_data->data.EncodedAnnoText.count,
	        oc_data->data.EncodedAnnoText.encoded_text, lenofStrings)

	    totalSize += (SIZEOF (pexAnnotationText) +
		NUMBYTES (lenofStrings));
	    break;
	}

	case PEXOCAnnotationText2D:

	{
	    /* Anno Text is always mono encoded */

	    int	lenofStrings;

	    GetStringsLength (oc_data->data.EncodedAnnoText2D.count,
	        oc_data->data.EncodedAnnoText2D.encoded_text, lenofStrings)

	    totalSize += (SIZEOF (pexAnnotationText2D) +
		NUMBYTES (lenofStrings));
	    break;
	}

	case PEXOCPolylineSetWithData:

	{
	    int numPoints, lenofVertex, i;

	    for (i = 0, numPoints = 0;
	         i < oc_data->data.PolylineSetWithData.count; i++)
	        numPoints +=
	        oc_data->data.PolylineSetWithData.vertex_lists[i].count;

	    lenofVertex = LENOF (pexCoord3D) +
	        ((oc_data->data.PolylineSetWithData.vertex_attributes &
	         PEXGAColor) ? GetColorLength (
	         oc_data->data.PolylineSetWithData.color_type) : 0);

	    totalSize += (SIZEOF (pexPolylineSetWithData) + NUMBYTES (
	        oc_data->data.PolylineSetWithData.count +
	        numPoints * lenofVertex));
	    break;
	}

	case PEXOCNURBCurve:

	{
	    int sizeofVertexList, sizeofKnotList;

    	    sizeofVertexList =
	        oc_data->data.NURBCurve.count *
	        ((oc_data->data.NURBCurve.rationality == PEXRational) ?
	        SIZEOF (pexCoord4D) : SIZEOF (pexCoord3D));

	    sizeofKnotList = NUMBYTES (
	        oc_data->data.NURBCurve.order +
	        oc_data->data.NURBCurve.count);

	    totalSize += (SIZEOF (pexNURBCurve) +
	        sizeofVertexList + sizeofKnotList);
	    break;
	}

	case PEXOCFillArea:

	    totalSize += (SIZEOF (pexFillArea) +
	        oc_data->data.FillArea.count * SIZEOF (pexCoord3D));
	    break;

	case PEXOCFillArea2D:

	    totalSize += (SIZEOF (pexFillArea2D) +
	        oc_data->data.FillArea2D.count * SIZEOF (pexCoord2D));
	    break;

	case PEXOCFillAreaWithData:

	{
	    int lenofColor, lenofFacet, lenofVertex;

    	    lenofColor =
	        GetColorLength (oc_data->data.FillAreaWithData.color_type);

	    lenofFacet =
	        GetFacetDataLength (
	    	oc_data->data.FillAreaWithData.facet_attributes, lenofColor); 

	    lenofVertex =
	        GetVertexWithDataLength (
	    	oc_data->data.FillAreaWithData.vertex_attributes, lenofColor);

	    totalSize += (SIZEOF (pexFillAreaWithData) +
	        NUMBYTES (lenofFacet + 1 +
	        oc_data->data.FillAreaWithData.count * lenofVertex));
	    break;
	}

	case PEXOCFillAreaSet:

	{
	    int numPoints, i;

	    for (i = 0, numPoints = 0;
	        i < oc_data->data.FillAreaSet.count; i++)
	        numPoints +=
	        oc_data->data.FillAreaSet.point_lists[i].count;

	    totalSize += (SIZEOF (pexFillAreaSet) +
	        NUMBYTES (oc_data->data.FillAreaSet.count) +
	        numPoints * SIZEOF (pexCoord3D));
	    break;
	}

	case PEXOCFillAreaSet2D:

	{
	    int numPoints, i;

	    for (i = 0, numPoints = 0;
	        i < oc_data->data.FillAreaSet2D.count; i++)
	        numPoints +=
	        oc_data->data.FillAreaSet2D.point_lists[i].count;

	    totalSize += (SIZEOF (pexFillAreaSet2D) +
	        NUMBYTES (oc_data->data.FillAreaSet2D.count) +
	        numPoints * SIZEOF (pexCoord2D));
	    break;
	}

	case PEXOCFillAreaSetWithData:

	{
	    int lenofColor, lenofFacet, lenofVertex;
	    int numVertices, i;

    	    lenofColor = GetColorLength (
	        oc_data->data.FillAreaSetWithData.color_type);

	    lenofFacet =
	        GetFacetDataLength (
	        oc_data->data.FillAreaSetWithData.facet_attributes,
	        lenofColor); 

	    lenofVertex =
	        GetVertexWithDataLength (
	        oc_data->data.FillAreaSetWithData.vertex_attributes,
	        lenofColor);

	    if (oc_data->data.FillAreaSetWithData.vertex_attributes &
	        PEXGAEdges)
	        lenofVertex++;         /* edge switch is CARD32 */

	    for (i = 0, numVertices = 0;
	        i < oc_data->data.FillAreaSetWithData.count; i++)
	        numVertices +=
	        oc_data->data.FillAreaSetWithData.vertex_lists[i].count;

	    totalSize += (SIZEOF (pexFillAreaSetWithData) +
	        NUMBYTES (lenofFacet +
	        oc_data->data.FillAreaSetWithData.count +
	        numVertices * lenofVertex));
	    break;
	}

	case PEXOCTriangleStrip:

	{
	    int lenofColor, lenofFacetList, lenofVertexList;

    	    lenofColor =
	        GetColorLength (oc_data->data.TriangleStrip.color_type);

	    lenofFacetList = (oc_data->data.TriangleStrip.count - 2) *
	        GetFacetDataLength (
	        oc_data->data.TriangleStrip.facet_attributes, lenofColor);

	    lenofVertexList = oc_data->data.TriangleStrip.count *
	        GetVertexWithDataLength (
	        oc_data->data.TriangleStrip.vertex_attributes, lenofColor);

	    totalSize += (SIZEOF (pexTriangleStrip) +
	        NUMBYTES (lenofFacetList + lenofVertexList));
	    break;
	}

	case PEXOCQuadrilateralMesh:

	{
	    int lenofColor, lenofFacetList, lenofVertexList;

    	    lenofColor =
	        GetColorLength (oc_data->data.QuadrilateralMesh.color_type);

	    lenofFacetList =
	        (oc_data->data.QuadrilateralMesh.row_count - 1) *
	        (oc_data->data.QuadrilateralMesh.col_count - 1) *
	        GetFacetDataLength (
	            oc_data->data.QuadrilateralMesh.facet_attributes,
	    	lenofColor);

	    lenofVertexList =
	        oc_data->data.QuadrilateralMesh.row_count *
	        oc_data->data.QuadrilateralMesh.col_count *
	        GetVertexWithDataLength (
	            oc_data->data.QuadrilateralMesh.vertex_attributes,
	            lenofColor);

	    totalSize += (SIZEOF (pexQuadrilateralMesh) +
	        NUMBYTES (lenofFacetList + lenofVertexList));
	    break;
	}

	case PEXOCSetOfFillAreaSets:

	{
	    PEXConnectivityData *pConnectivity;
	    int 	lenofColor, lenofFacet, lenofVertex;
	    int 	sizeofEdge, sofaLength;
	    int 	numContours, numFillAreaSets;
	    int		numIndices, numVertices, i;

	    numFillAreaSets = oc_data->data.SetOfFillAreaSets.set_count;
	    numIndices = oc_data->data.SetOfFillAreaSets.index_count;
	    numVertices = oc_data->data.SetOfFillAreaSets.vertex_count;

	    pConnectivity = oc_data->data.SetOfFillAreaSets.connectivity;
	    numContours = 0;
	    for (i = 0; i < numFillAreaSets; i++, pConnectivity++)
	        numContours += pConnectivity->count;

	    lenofColor = GetColorLength (
	        oc_data->data.SetOfFillAreaSets.color_type);
	    lenofFacet = GetFacetDataLength (
 	        oc_data->data.SetOfFillAreaSets.facet_attributes,
	        lenofColor); 
	    lenofVertex = GetVertexWithDataLength (
	        oc_data->data.SetOfFillAreaSets.vertex_attributes,
	        lenofColor);

	    sizeofEdge = oc_data->data.SetOfFillAreaSets.edge_attributes ?
	        SIZEOF (CARD8) : 0;

	    sofaLength = (lenofFacet * numFillAreaSets) +
	        (lenofVertex * numVertices) + 
	        NUMWORDS (sizeofEdge * numIndices) +
	        NUMWORDS (SIZEOF (CARD16) *
	        (numFillAreaSets + numContours + numIndices));

	    totalSize += (SIZEOF (pexSetOfFillAreaSets) +
		NUMBYTES (sofaLength));
	    break;
	}

	case PEXOCNURBSurface:
	{
	    PEXListOfTrimCurve	*ptrimLoop;
	    PEXTrimCurve	*ptrimCurve;
	    int 	numMPoints, numNPoints, numTrimLoops;
	    int 	uorder, vorder;
	    int 	lenofVertexList, lenofUKnotList, lenofVKnotList;
	    int 	lenofTrimData, count, i;

	    numMPoints = oc_data->data.NURBSurface.col_count;
	    numNPoints = oc_data->data.NURBSurface.row_count;
	    numTrimLoops = oc_data->data.NURBSurface.curve_count;
	    uorder = oc_data->data.NURBSurface.uorder;
	    vorder = oc_data->data.NURBSurface.vorder;

	    lenofVertexList = numMPoints * numNPoints *
                ((oc_data->data.NURBSurface.rationality == PEXRational)
	        ? LENOF (pexCoord4D) : LENOF (pexCoord3D));
	    lenofUKnotList = uorder + numMPoints;
	    lenofVKnotList = vorder + numNPoints;

	    lenofTrimData = numTrimLoops * LENOF (CARD32);

	    ptrimLoop = oc_data->data.NURBSurface.trim_curves;
	    for (i = 0; i < numTrimLoops; i++, ptrimLoop++)
	    {
	        ptrimCurve = ptrimLoop->curves;
	        count = ptrimLoop->count;
	    
	        while (count--)
	        {
		    lenofTrimData += (LENOF (pexTrimCurve) +
	    	        ptrimCurve->count + ptrimCurve->order +
	                ptrimCurve->count *
	                (ptrimCurve->rationality == PEXRational ?
	                LENOF (pexCoord3D) : LENOF (pexCoord2D)));
	            ptrimCurve++;
	        }
	    }

	    totalSize += (SIZEOF (pexNURBSurface) +
	        NUMBYTES (lenofUKnotList + lenofVKnotList +
	        lenofVertexList + lenofTrimData));
	    break;
	}

	case PEXOCCellArray:

	{
	    int bytes;

	    bytes = oc_data->data.CellArray.col_count *
	        oc_data->data.CellArray.row_count * SIZEOF (pexTableIndex);
	    totalSize += (SIZEOF (pexCellArray) + PADDED_BYTES (bytes));
	    break;
	}

	case PEXOCCellArray2D:

	{
	    int bytes;

	    bytes = oc_data->data.CellArray2D.col_count *
	        oc_data->data.CellArray2D.row_count * SIZEOF (pexTableIndex);
	    totalSize += (SIZEOF (pexCellArray2D) + PADDED_BYTES (bytes));
	    break;
	}

	case PEXOCExtendedCellArray:

	{
	    int lenofColorList;

	    lenofColorList = oc_data->data.ExtendedCellArray.col_count *
	        oc_data->data.ExtendedCellArray.row_count * GetColorLength (
	        oc_data->data.ExtendedCellArray.color_type);

	    totalSize += (SIZEOF (pexExtendedCellArray) +
	        NUMBYTES (lenofColorList));
	    break;
	}

	case PEXOCGDP:

	    totalSize += (SIZEOF (pexGDP) +
	        oc_data->data.GDP.count * SIZEOF (pexCoord3D) +
	        PADDED_BYTES (oc_data->data.GDP.length));
	    break;

	case PEXOCGDP2D:

	    totalSize += (SIZEOF (pexGDP2D) +
	        oc_data->data.GDP2D.count * SIZEOF (pexCoord2D) +
	        PADDED_BYTES (oc_data->data.GDP2D.length));
	    break;

	case PEXOCNoop:

	    totalSize += SIZEOF (pexNoop);
	    break;

	default:
	    break;
	}
    }

#ifdef DEBUG
    if (totalSize % 4)
    {
	printf ("PEXlib WARNING : Internal error in PEXGetSizeOCs :\n");
	printf ("Memory allocated is not word aligned.\n");
    }
#endif

    return (totalSize);
}


unsigned long
PEXCountOCs (float_format, length, encoded_ocs)

INPUT int		float_format;
INPUT unsigned long     length;
INPUT char		*encoded_ocs;

{
    char		*pBuf = encoded_ocs;
    char		*pBufStart;
    unsigned long	oc_count = 0;
    int			totalSize = 0;
    pexElementInfo	*elemInfo;


    /*
     * Keep parsing the data until the end of the buffer is reached.
     * Increment the oc count as we go along.
     */

    while (totalSize < length)
    {
	GET_STRUCT_PTR (pexElementInfo, pBuf, elemInfo);

	pBufStart = pBuf;

	switch (elemInfo->elementType)
	{
	case PEXOCMarkerType:
	case PEXOCLineType:
	case PEXOCATextStyle:
	case PEXOCBFInteriorStyle:
	case PEXOCBFReflectionModel:
	case PEXOCBFSurfaceInterpMethod:
	case PEXOCInteriorStyle:
	case PEXOCPolylineInterpMethod:
	case PEXOCReflectionModel:
	case PEXOCRenderingColorModel:
	case PEXOCSurfaceEdgeType:
	case PEXOCSurfaceInterpMethod:

	    pBuf += SIZEOF (pexMarkerType);
	    break;

	case PEXOCMarkerColorIndex:
	case PEXOCMarkerBundleIndex:
	case PEXOCTextFontIndex:
	case PEXOCTextColorIndex:
	case PEXOCTextBundleIndex:
	case PEXOCLineColorIndex:
	case PEXOCLineBundleIndex:
	case PEXOCSurfaceColorIndex:
	case PEXOCBFInteriorStyleIndex:
	case PEXOCBFSurfaceColorIndex:
	case PEXOCInteriorBundleIndex:
	case PEXOCInteriorStyleIndex:
	case PEXOCSurfaceEdgeColorIndex:
	case PEXOCEdgeBundleIndex:
	case PEXOCViewIndex:
	case PEXOCDepthCueIndex:
	case PEXOCColorApproxIndex:

	    pBuf += SIZEOF (pexMarkerColorIndex);
	    break;

	case PEXOCMarkerColor:
	case PEXOCTextColor:
	case PEXOCLineColor:
	case PEXOCSurfaceColor:
	case PEXOCBFSurfaceColor:
	case PEXOCSurfaceEdgeColor:

	{
	    pexMarkerColor *oc;

	    GET_STRUCT_PTR (pexMarkerColor, pBuf, oc);

	    pBuf += (SIZEOF (pexMarkerColor) + GetColorSize (oc->colorType));
	    break;
	}

	case PEXOCMarkerScale:
	case PEXOCCharExpansion:
	case PEXOCCharSpacing:
	case PEXOCCharHeight:
	case PEXOCATextHeight:
	case PEXOCLineWidth:
	case PEXOCSurfaceEdgeWidth:

	    pBuf += SIZEOF (pexMarkerScale);
	    break;

	case PEXOCTextPrecision:
	case PEXOCTextPath:
	case PEXOCATextPath:

	    pBuf += SIZEOF (pexTextPrecision);
	    break;

	case PEXOCCharUpVector:
	case PEXOCATextUpVector:

	    pBuf += SIZEOF (pexCharUpVector);
	    break;

	case PEXOCTextAlignment:
	case PEXOCATextAlignment:

	    pBuf += SIZEOF (pexTextAlignment);
	    break;

	case PEXOCCurveApprox:

	    pBuf += SIZEOF (pexCurveApprox);
	    break;

	case PEXOCReflectionAttributes:
	case PEXOCBFReflectionAttributes:

	{
	    pexReflectionAttributes *oc;

	    GET_STRUCT_PTR (pexReflectionAttributes, pBuf, oc);
    
	    pBuf += (SIZEOF (pexReflectionAttributes) +
		GetColorSize (oc->specular_colorType));
	    break;
	}

	case PEXOCSurfaceApprox:

	    pBuf += SIZEOF (pexSurfaceApprox);
	    break;

	case PEXOCFacetCullingMode:

	    pBuf += SIZEOF (pexFacetCullingMode);
	    break;

	case PEXOCFacetDistinguishFlag:
	case PEXOCSurfaceEdgeFlag:
	case PEXOCModelClipFlag:

	    pBuf += SIZEOF (pexFacetDistinguishFlag);
	    break;

	case PEXOCPatternSize:

	    pBuf += SIZEOF (pexPatternSize);
	    break;

	case PEXOCPatternAttributes2D:

	    pBuf += SIZEOF (pexPatternAttributes2D);
	    break;

	case PEXOCPatternAttributes:

	    pBuf += SIZEOF (pexPatternAttributes);
	    break;

	case PEXOCIndividualASF:

	    pBuf += SIZEOF (pexIndividualASF);
	    break;

	case PEXOCLocalTransform:

	    pBuf += SIZEOF (pexLocalTransform);
	    break;

	case PEXOCLocalTransform2D:

	    pBuf += SIZEOF (pexLocalTransform2D);
	    break;

	case PEXOCGlobalTransform:

	    pBuf += SIZEOF (pexGlobalTransform);
	    break;

	case PEXOCGlobalTransform2D:

	    pBuf += SIZEOF (pexGlobalTransform2D);
	    break;

	case PEXOCModelClipVolume:

	{
	    pexModelClipVolume 	*oc;

	    GET_STRUCT_PTR (pexModelClipVolume, pBuf, oc);
    
	    pBuf += (SIZEOF (pexModelClipVolume) +
		oc->numHalfSpaces * SIZEOF (pexHalfSpace));
	    break;
	}

	case PEXOCModelClipVolume2D:

	{
	    pexModelClipVolume2D *oc;

	    GET_STRUCT_PTR (pexModelClipVolume2D, pBuf, oc);
    
	    pBuf += (SIZEOF (pexModelClipVolume2D) +
		oc->numHalfSpaces * SIZEOF (pexHalfSpace2D));
	    break;
	}

	case PEXOCRestoreModelClipVolume:

	    pBuf += SIZEOF (pexRestoreModelClipVolume);
	    break;

	case PEXOCLightSourceState:

	{
	    pexLightSourceState *oc;
	    int			size;

	    GET_STRUCT_PTR (pexLightSourceState, pBuf, oc);
    
	    size = oc->numEnable * SIZEOF (pexTableIndex);
	    pBuf += (SIZEOF (pexLightSourceState) + PADDED_BYTES (size));
    
	    size = oc->numDisable * SIZEOF (pexTableIndex);
	    pBuf += PADDED_BYTES (size);
	    break;
	}

	case PEXOCPickID:
	case PEXOCHLHSRID:

	    pBuf += SIZEOF (pexPickID);
	    break;

	case PEXOCParaSurfCharacteristics:

	{
	    pexParaSurfCharacteristics *oc;

	    GET_STRUCT_PTR (pexParaSurfCharacteristics, pBuf, oc);
	    pBuf += SIZEOF (pexParaSurfCharacteristics);

	    switch (oc->characteristics)
	    {
	    case PEXPSCIsoCurves:
		pBuf += SIZEOF (pexPSC_IsoparametricCurves);
		break;
	
	    case PEXPSCMCLevelCurves:
	    case PEXPSCWCLevelCurves:
	    {
		pexPSC_LevelCurves *level;

		GET_STRUCT_PTR (pexPSC_LevelCurves, pBuf, level);

		pBuf += (SIZEOF (pexPSC_LevelCurves) +
		    SIZEOF (float) * level->numberIntersections);
		break;
	    }
	
	    default:
		pBuf += PADDED_BYTES (oc->length);
		break;
	    }

	    break;
	}

	case PEXOCAddToNameSet:
	case PEXOCRemoveFromNameSet:

	    pBuf += (SIZEOF (pexAddToNameSet) +
		(elemInfo->length - 1) * SIZEOF (pexName));
	    break;

	case PEXOCExecuteStructure:

	    pBuf += SIZEOF (pexExecuteStructure);
	    break;

	case PEXOCLabel:

	    pBuf += SIZEOF (pexLabel);
	    break;

	case PEXOCApplicationData:

	{
	    pexApplicationData *oc;

	    GET_STRUCT_PTR (pexApplicationData, pBuf, oc);

	    pBuf += (SIZEOF (pexApplicationData) +
		PADDED_BYTES (oc->numElements));
	    break;
	}

	case PEXOCGSE:

	{
	    pexGSE *oc;

	    GET_STRUCT_PTR (pexGSE, pBuf, oc);

	    pBuf += (SIZEOF (pexGSE) + PADDED_BYTES (oc->numElements));
	    break;
	}

	case PEXOCMarkers:
	case PEXOCPolyline:

	    pBuf += (SIZEOF (pexMarkers) + SIZEOF (pexCoord3D) *
	    	(SIZEOF (CARD32) * ((int) elemInfo->length - 1)) /
                SIZEOF (pexCoord3D));
	    break;

	case PEXOCMarkers2D:
	case PEXOCPolyline2D:

	    pBuf += (SIZEOF (pexMarkers2D) + SIZEOF (pexCoord2D) *
	    	(SIZEOF (CARD32) * ((int) elemInfo->length - 1)) /
                SIZEOF (pexCoord2D));
	    break;

	case PEXOCText:

	{
	    /* Text is always mono encoded */

	    pexText 		*oc;
	    pexMonoEncoding	*enc;
	    int			size, i;

	    GET_STRUCT_PTR (pexText, pBuf, oc);
	    pBuf += SIZEOF (pexText);

	    for (i = 0; i < (int) oc->numEncodings; i++)
	    {
		GET_STRUCT_PTR (pexMonoEncoding, pBuf, enc);

		if (enc->characterSetWidth == PEXCSLong)
		    size = enc->numChars * sizeof (long);
		else if (enc->characterSetWidth == PEXCSShort)
		    size = enc->numChars * sizeof (short);
		else /* enc->characterSetWidth == PEXCSByte) */
		    size = enc->numChars;

		pBuf += (SIZEOF (pexMonoEncoding) + PADDED_BYTES (size));
	    }
	    break;
	}

	case PEXOCText2D:

	{
	    /* Text is always mono encoded */

	    pexText2D 		*oc;
	    pexMonoEncoding	*enc;
	    int			size, i;

	    GET_STRUCT_PTR (pexText2D, pBuf, oc);
	    pBuf += SIZEOF (pexText2D);

	    for (i = 0; i < (int) oc->numEncodings; i++)
	    {
		GET_STRUCT_PTR (pexMonoEncoding, pBuf, enc);

		if (enc->characterSetWidth == PEXCSLong)
		    size = enc->numChars * sizeof (long);
		else if (enc->characterSetWidth == PEXCSShort)
		    size = enc->numChars * sizeof (short);
		else /* enc->characterSetWidth == PEXCSByte) */
		    size = enc->numChars;

		pBuf += (SIZEOF (pexMonoEncoding) + PADDED_BYTES (size));
	    }
	    break;
	}

	case PEXOCAnnotationText:

	{
	    /* Anno Text is always mono encoded */

	    pexAnnotationText 	*oc;
	    pexMonoEncoding	*enc;
	    int			size, i;

	    GET_STRUCT_PTR (pexAnnotationText, pBuf, oc);
	    pBuf += SIZEOF (pexAnnotationText);

	    for (i = 0; i < (int) oc->numEncodings; i++)
	    {
		GET_STRUCT_PTR (pexMonoEncoding, pBuf, enc);

		if (enc->characterSetWidth == PEXCSLong)
		    size = enc->numChars * sizeof (long);
		else if (enc->characterSetWidth == PEXCSShort)
		    size = enc->numChars * sizeof (short);
		else /* enc->characterSetWidth == PEXCSByte) */
		    size = enc->numChars;

		pBuf += (SIZEOF (pexMonoEncoding) + PADDED_BYTES (size));
	    }
	    break;
	}

	case PEXOCAnnotationText2D:

	{
	    /* Anno Text is always mono encoded */

	    pexAnnotationText2D	*oc;
	    pexMonoEncoding	*enc;
	    int			size, i;

	    GET_STRUCT_PTR (pexAnnotationText2D, pBuf, oc);
	    pBuf += SIZEOF (pexAnnotationText2D);

	    for (i = 0; i < (int) oc->numEncodings; i++)
	    {
		GET_STRUCT_PTR (pexMonoEncoding, pBuf, enc);

		if (enc->characterSetWidth == PEXCSLong)
		    size = enc->numChars * sizeof (long);
		else if (enc->characterSetWidth == PEXCSShort)
		    size = enc->numChars * sizeof (short);
		else /* enc->characterSetWidth == PEXCSByte) */
		    size = enc->numChars;

		pBuf += (SIZEOF (pexMonoEncoding) + PADDED_BYTES (size));
	    }
	    break;
	}

	case PEXOCPolylineSetWithData:

	{
	    pexPolylineSetWithData 	*oc;
	    int				lenofVertex, i;
	    CARD32			count;

	    GET_STRUCT_PTR (pexPolylineSetWithData, pBuf, oc);

	    lenofVertex = LENOF (pexCoord3D) +
		((oc->vertexAttribs & PEXGAColor) ?
		GetColorLength (oc->colorType) : 0); 

	    pBuf += SIZEOF (pexPolylineSetWithData);

	    for (i = 0; i < oc->numLists; i++)
	    {
		EXTRACT_CARD32 (pBuf, count);
		pBuf += NUMBYTES (count * lenofVertex);
	    }
	    break;
	}

	case PEXOCNURBCurve:

	{
	    pexNURBCurve	*oc;

	    GET_STRUCT_PTR (pexNURBCurve, pBuf, oc);

	    pBuf += (SIZEOF (pexNURBCurve) + oc->numKnots * SIZEOF (float) +
	        (oc->numPoints * ((oc->coordType == PEXRational) ?
		SIZEOF (pexCoord4D) : SIZEOF (pexCoord3D))));
	    break;
	}

	case PEXOCFillArea:

	{
	    int 		count;
    
	    count = (SIZEOF (CARD32) * ((int) elemInfo->length - 2)) /
		SIZEOF (pexCoord3D);
    
	    pBuf += (SIZEOF (pexFillArea) + count * SIZEOF (pexCoord3D));
	    break;
	}

	case PEXOCFillArea2D:

	{
	    int 		count;
    
	    count = (SIZEOF (CARD32) * ((int) elemInfo->length - 2)) /
		SIZEOF (pexCoord2D);
    
	    pBuf += (SIZEOF (pexFillArea2D) + count * SIZEOF (pexCoord2D));
	    break;
	}

	case PEXOCFillAreaWithData:

	{
	    pexFillAreaWithData	*oc;
	    int			lenofFacetData;
	    int			lenofVertex;
	    int			lenofColor;
	    CARD32		count;

	    GET_STRUCT_PTR (pexFillAreaWithData, pBuf, oc);
    
	    lenofColor = GetColorLength (oc->colorType);
	    lenofFacetData = GetFacetDataLength (oc->facetAttribs, lenofColor);
	    lenofVertex = GetVertexWithDataLength (
		oc->vertexAttribs, lenofColor);

	    pBuf += SIZEOF (pexFillAreaWithData);

	    if (oc->facetAttribs)
		pBuf += NUMBYTES (lenofFacetData);

	    EXTRACT_CARD32 (pBuf, count);
	    pBuf += (count * NUMBYTES (lenofVertex));
	    break;
	}

	case PEXOCFillAreaSet:

	{
	    pexFillAreaSet 	*oc;
	    int			i;
	    CARD32		count;

	    GET_STRUCT_PTR (pexFillAreaSet, pBuf, oc);

	    pBuf += SIZEOF (pexFillAreaSet);

	    for (i = 0; i < oc->numLists; i++)
	    {
		EXTRACT_CARD32 (pBuf, count);
		pBuf += (count * SIZEOF (pexCoord3D));
	    }
	    break;
	}

	case PEXOCFillAreaSet2D:

	{
	    pexFillAreaSet2D 	*oc;
	    int			i;
	    CARD32		count;

	    GET_STRUCT_PTR (pexFillAreaSet2D, pBuf, oc);

	    pBuf += SIZEOF (pexFillAreaSet2D);

	    for (i = 0; i < oc->numLists; i++)
	    {
		EXTRACT_CARD32 (pBuf, count);
		pBuf += (count * SIZEOF (pexCoord2D));
	    }
	    break;
	}

	case PEXOCFillAreaSetWithData:

	{
	    pexFillAreaSetWithData 	*oc;
	    int				lenofFacetData;
	    int				lenofVertex;
	    int				lenofColor, i;
	    CARD32			count;

	    GET_STRUCT_PTR (pexFillAreaSetWithData, pBuf, oc);

	    lenofColor = GetColorLength (oc->colorType);
	    lenofFacetData = GetFacetDataLength (oc->facetAttribs, lenofColor);
	    lenofVertex = GetVertexWithDataLength (
		oc->vertexAttribs, lenofColor);
    
	    if (oc->vertexAttribs & PEXGAEdges)
		lenofVertex++; 			/* edge switch is CARD32 */

	    pBuf += SIZEOF (pexFillAreaSetWithData);

	    if (oc->facetAttribs)
		pBuf += NUMBYTES (lenofFacetData);

	    for (i = 0; i < oc->numLists; i++)
	    {
		EXTRACT_CARD32 (pBuf, count);
		pBuf += (count * NUMBYTES (lenofVertex));
	    }
	    break;
	}

	case PEXOCTriangleStrip:

	{
	    pexTriangleStrip 	*oc;
	    int			lenofColor;
	    int			lenofFacetDataList;
	    int			lenofVertexList;

	    GET_STRUCT_PTR (pexTriangleStrip, pBuf, oc);

	    lenofColor = GetColorLength (oc->colorType);
	    lenofFacetDataList = (oc->numVertices - 2) *
		GetFacetDataLength (oc->facetAttribs, lenofColor); 
	    lenofVertexList = oc->numVertices *
		GetVertexWithDataLength (oc->vertexAttribs, lenofColor);

	    pBuf += SIZEOF (pexTriangleStrip);

	    if (oc->facetAttribs)
		pBuf += NUMBYTES (lenofFacetDataList);

	    pBuf += NUMBYTES (lenofVertexList);
	    break;
	}

	case PEXOCQuadrilateralMesh:

	{
	    pexQuadrilateralMesh 	*oc;
	    int				lenofColor;
	    int				lenofFacetDataList;
	    int				lenofVertexList;

	    GET_STRUCT_PTR (pexQuadrilateralMesh, pBuf, oc);

	    lenofColor = GetColorLength (oc->colorType);
	    lenofFacetDataList = ((oc->mPts - 1) * (oc->nPts - 1)) *
		GetFacetDataLength (oc->facetAttribs, lenofColor); 
	    lenofVertexList = oc->mPts * oc->nPts *
		GetVertexWithDataLength (oc->vertexAttribs, lenofColor);

	    pBuf += SIZEOF (pexQuadrilateralMesh);

	    if (oc->facetAttribs)
		pBuf += NUMBYTES (lenofFacetDataList);

	    pBuf += NUMBYTES (lenofVertexList);
	    break;
	}

	case PEXOCSetOfFillAreaSets:

	{
	    pexSetOfFillAreaSets	*oc;
	    int 			lenofColor;
	    int 			lenofFacet;
	    int 			lenofVertex;
	    int				cbytes, i, j;
	    CARD16			count, scount;

	    GET_STRUCT_PTR (pexSetOfFillAreaSets, pBuf, oc);

	    lenofColor = GetColorLength (oc->colorType);
	    lenofFacet = GetFacetDataLength (oc->FAS_Attributes, lenofColor); 
	    lenofVertex = GetVertexWithDataLength (
		oc->vertexAttributes, lenofColor);

	    pBuf += SIZEOF (pexSetOfFillAreaSets);

	    if (oc->FAS_Attributes)
		pBuf += (NUMBYTES (lenofFacet) * oc->numFAS);

	    pBuf += (NUMBYTES (lenofVertex) * oc->numVertices);

	    if (oc->edgeAttributes)
		pBuf += PADDED_BYTES (oc->numEdges * SIZEOF (CARD8));
	
	    for (i = 0; i < (int) oc->numFAS; i++)
	    {
		EXTRACT_CARD16 (pBuf, count);

		for (j = 0; j < (int) count; j++)
		{
		    EXTRACT_CARD16 (pBuf, scount);
		    pBuf += (scount * SIZEOF (CARD16));
		}
	    }
	
	    cbytes = SIZEOF (CARD16) *
		(oc->numFAS + oc->numContours + oc->numEdges);

	    pBuf += PAD (cbytes);
	    break;
	}

	case PEXOCNURBSurface:

	{
	    pexNURBSurface	*oc;
	    int			sizeofVertexList;
	    int			sizeofUKnotList;
	    int			sizeofVKnotList;
	    pexTrimCurve	*trim;
	    int			i, j;
	    CARD32		count;

	    GET_STRUCT_PTR (pexNURBSurface, pBuf, oc);

	    sizeofVertexList = oc->mPts * oc->nPts *
		((oc->type == PEXRational) ?
		SIZEOF (pexCoord4D) : SIZEOF (pexCoord3D));
	    sizeofUKnotList = NUMBYTES (oc->uOrder + oc->mPts);
	    sizeofVKnotList = NUMBYTES (oc->vOrder + oc->nPts);

	    pBuf += (SIZEOF (pexNURBSurface) +
		sizeofUKnotList + sizeofVKnotList + sizeofVertexList);

	    for (i = 0; i < oc->numLists; i++)
	    {
		EXTRACT_CARD32 (pBuf, count);

		for (j = 0; j < count; j++)
		{
		    GET_STRUCT_PTR (pexTrimCurve, pBuf, trim);

		    pBuf += (SIZEOF (pexTrimCurve) +
		        NUMBYTES (trim->order + trim->numCoord) +
		        trim->numCoord * ((trim->type == PEXRational) ?
		        SIZEOF (pexCoord3D) : SIZEOF (pexCoord2D)));
		}
	    }
	    break;
	}

	case PEXOCCellArray:

	{
	    pexCellArray	*oc;
	    int			size;

	    GET_STRUCT_PTR (pexCellArray, pBuf, oc);

	    size = oc->dx * oc->dy * SIZEOF (pexTableIndex);
	    pBuf += (SIZEOF (pexCellArray) + PADDED_BYTES (size));
	    break;
	}

	case PEXOCCellArray2D:

	{
	    pexCellArray2D	*oc;
	    int			size;

	    GET_STRUCT_PTR (pexCellArray2D, pBuf, oc);

	    size = oc->dx * oc->dy * SIZEOF (pexTableIndex);
	    pBuf += (SIZEOF (pexCellArray2D) + PADDED_BYTES (size));
	    break;
	}

	case PEXOCExtendedCellArray:

	{
	    pexExtendedCellArray	*oc;

	    GET_STRUCT_PTR (pexExtendedCellArray, pBuf, oc);

	    pBuf += (SIZEOF (pexExtendedCellArray) + 
		oc->dx * oc->dy * NUMBYTES (GetColorLength (oc->colorType)));
	    break;
	}

	case PEXOCGDP:

	{
	    pexGDP	*oc;

	    GET_STRUCT_PTR (pexGDP, pBuf, oc);

	    pBuf += (SIZEOF (pexGDP) + oc->numPoints * SIZEOF (pexCoord3D) +
	        PADDED_BYTES (oc->numBytes));
	    break;
	}

	case PEXOCGDP2D:

	{
	    pexGDP2D	*oc;

	    GET_STRUCT_PTR (pexGDP2D, pBuf, oc);

	    pBuf += (SIZEOF (pexGDP2D) + oc->numPoints * SIZEOF (pexCoord2D) +
	        PADDED_BYTES (oc->numBytes));
	    break;
	}

	case PEXOCNoop:

	    pBuf += SIZEOF (pexNoop);
	    break;

	default:
	    break;
	}

	totalSize += (pBuf - pBufStart);
	oc_count++;
    }

#ifdef DEBUG
    if (totalSize > length)
    {
	printf ("PEXlib WARNING : Internal error in PEXCountOCs :\n");
	printf ("OC parsing continued past the end of the input buffer.\n");
    }
#endif

    return (oc_count);
}


/*
 *       INTERNAL FUNCTIONS
 */

void
_PEXCopyPaddedBytesToOC (display, numBytes, data) 

INPUT Display		*display;
INPUT int		numBytes;
INPUT char		*data;

{
    PEXDisplayInfo 	*pexDisplayInfo;
    int			paddedBytes = PADDED_BYTES (numBytes);


    if (paddedBytes <= BytesLeftInXBuffer (display))
    {
	/*
	 * There is room in the X buffer to do the copy.
	 */

	memcpy (display->bufptr, data, numBytes);
	display->bufptr += paddedBytes;
    }
    else
    {
	/*
	 * Copying this OC will overflow the transport buffer.  Using
	 * _XSend will take care of splitting the buffer into chunks
	 * small enough to fit in the transport buffer.
	 */
	
	_XSend (display, data, numBytes);


	/*
	 * Make sure that the next oc starts a new request.
	 */
	
	PEXGetDisplayInfo (display, pexDisplayInfo);
	pexDisplayInfo->lastReqNum = -1;
    }
}


void
_PEXSendBytesToOC (display, numBytes, data) 

INPUT Display		*display;
INPUT int		numBytes;
INPUT char		*data;

{
    PEXDisplayInfo 	*pexDisplayInfo;
    int			mod4bytes;


    /*
     * _XSend will take care of splitting the buffer into chunks
     * small enough to fit in the transport buffer.  _XSend will
     * only copy a multiple of 4 bytes, so we must do some extra
     * work if numBytes % 4 != 0.
     */
	
    if ((mod4bytes = numBytes % 4))
    {
	if (mod4bytes > BytesLeftInXBuffer (display))
	    _XFlush (display);

	memcpy (display->bufptr, data, mod4bytes);
	display->bufptr += mod4bytes;

	data += mod4bytes;
	numBytes -= mod4bytes;
    }

    _XSend (display, data, numBytes);


    /*
     * Make sure that the next oc starts a new request.
     */
	
    PEXGetDisplayInfo (display, pexDisplayInfo);
    pexDisplayInfo->lastReqNum = -1;
}


void _PEXOCFacet (display, colorType, facetAttr, facetData, fpFormat)

INPUT Display		*display;
INPUT int		colorType;
INPUT unsigned int	facetAttr;
INPUT PEXFacetData	*facetData;
INPUT int		fpFormat;

{
    int 	lenofFacet;
    char 	*ocAddr;
    PEXVector 	*normal;

    if (!facetData)
	return;

    lenofFacet = GetFacetDataLength (facetAttr, GetColorLength (colorType));
    ocAddr = PEXGetOCAddr (display, NUMBYTES (lenofFacet));

    if (!(facetAttr & PEXGAColor))
    {
        normal = &(facetData->normal);
    }
    else
    {
        switch (colorType)
        {
        case PEXColorTypeIndexed:

            STORE_CARD16 (facetData->index.index, ocAddr);
	    ocAddr += 2;
    	    normal = &(facetData->index_normal.normal);
	    break;

        case PEXColorTypeRGB:
        case PEXColorTypeCIE:
        case PEXColorTypeHSV:
        case PEXColorTypeHLS:

	    FP_CONVERT_HTON_BUFF (facetData->rgb.red, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (facetData->rgb.green, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (facetData->rgb.blue, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);
    	    normal = &(facetData->rgb_normal.normal);
            break;

        case PEXColorTypeRGB8:

            memcpy (ocAddr, &(facetData->rgb8), 4);
            ocAddr += 4;
    	    normal = &(facetData->rgb8_normal.normal);
            break;

        case PEXColorTypeRGB16:

            STORE_CARD16 (facetData->rgb16.red, ocAddr);
            STORE_CARD16 (facetData->rgb16.green, ocAddr);
            STORE_CARD16 (facetData->rgb16.blue, ocAddr);
            ocAddr += 2;
    	    normal = &(facetData->rgb16_normal.normal);
            break;
        }
    }

    if (facetAttr & PEXGANormal)
    {
	FP_CONVERT_HTON_BUFF (normal->x, ocAddr, fpFormat);
	ocAddr += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (normal->y, ocAddr, fpFormat);
	ocAddr += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (normal->z, ocAddr, fpFormat);
    }
}


void _PEXOCListOfFacet (display, count, colorType,
    facetAttr, facetData, fpFormat)

INPUT Display			*display;
INPUT int			count;
INPUT int			colorType;
INPUT unsigned int		facetAttr;
INPUT PEXArrayOfFacetData	facetData;
INPUT int			fpFormat;

{
    int 		lenofFacet;
    int 		maxWords;
    int 		wordsLeft;
    int 		copyWords, i;
    char		*ocAddr;
    char		*data;
    PEXVector		*normal;
    
    if (!(data = (char *) facetData.index))
	return;

    lenofFacet = GetFacetDataLength (facetAttr, GetColorLength (colorType));
    
    maxWords = NUMWORDS (PEXGetOCAddrMaxSize (display));
    wordsLeft = count * lenofFacet;
    copyWords = (wordsLeft < maxWords) ?
	wordsLeft : (maxWords - maxWords % lenofFacet);

    while (copyWords > 0)
    {
	ocAddr = PEXGetOCAddr (display, NUMBYTES (copyWords));
	for (i = 0; i < (copyWords / lenofFacet); i++)
	{
	    if (facetAttr & PEXGAColor)
	    {
		switch (colorType)
		{
		case PEXColorTypeIndexed:
		    {
		    PEXColorIndexed *col = (PEXColorIndexed *) data;
		    data += sizeof (PEXColorIndexed);

		    STORE_CARD16 (col->index, ocAddr);
		    ocAddr += 2;
		    break;
		    }

		case PEXColorTypeRGB:
		case PEXColorTypeCIE:
		case PEXColorTypeHSV:
		case PEXColorTypeHLS:
		    {
		    PEXColorRGB *col = (PEXColorRGB *) data;
		    data += sizeof (PEXColorRGB);

		    FP_CONVERT_HTON_BUFF (col->red, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    FP_CONVERT_HTON_BUFF (col->green, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    FP_CONVERT_HTON_BUFF (col->blue, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    break;
		    }

		case PEXColorTypeRGB8:

		    memcpy (ocAddr, data, 4);
		    ocAddr += 4;
		    data += sizeof (PEXColorRGB8);
		    break;

		case PEXColorTypeRGB16:
		    {
		    PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		    data += sizeof (PEXColorRGB16);

		    STORE_CARD16 (col->red, ocAddr);
		    STORE_CARD16 (col->green, ocAddr);
		    STORE_CARD16 (col->blue, ocAddr);
		    ocAddr += 2;
		    break;
		    }
		}
	    }

	    if (facetAttr & PEXGANormal)
	    {
		normal = (PEXVector *) data;
		data += sizeof (PEXVector);

		FP_CONVERT_HTON_BUFF (normal->x, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (normal->y, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (normal->z, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
	    }
	}

	wordsLeft -= copyWords;
	copyWords = (wordsLeft < maxWords) ?
	    wordsLeft : (maxWords - maxWords % lenofFacet);
    }	
}


void _PEXOCListOfVertex (display, count, colorType,
    vertAttr, vertData, fpFormat)

INPUT Display			*display;
INPUT int			count;
INPUT int			colorType;
INPUT unsigned int		vertAttr;
INPUT PEXArrayOfVertex		vertData;
INPUT int			fpFormat;

{
    int 		lenofVert;
    int 		maxWords;
    int 		wordsLeft;
    int 		copyWords, i;
    char		*ocAddr;
    char		*data;
    PEXVector		*normal;
    unsigned int	*edge;
    
    if (!(data = (char *) vertData.index))
	return;

    lenofVert = GetVertexWithDataLength (vertAttr, GetColorLength (colorType));
    
    maxWords = NUMWORDS (PEXGetOCAddrMaxSize (display));
    wordsLeft = count * lenofVert;
    copyWords = (wordsLeft < maxWords) ?
	wordsLeft : (maxWords - maxWords % lenofVert);

    while (copyWords > 0)
    {
	ocAddr = PEXGetOCAddr (display, NUMBYTES (copyWords));
	for (i = 0; i < (copyWords / lenofVert); i++)
	{
	    PEXCoord *coord = (PEXCoord *) data;
	    data += sizeof (PEXCoord);

	    FP_CONVERT_HTON_BUFF (coord->x, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (coord->y, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (coord->z, ocAddr, fpFormat);
	    ocAddr += SIZEOF (float);

	    if (vertAttr & PEXGAColor)
	    {
		switch (colorType)
		{
		case PEXColorTypeIndexed:
		    {
		    PEXColorIndexed *col = (PEXColorIndexed *) data;
		    data += sizeof (PEXColorIndexed);

		    STORE_CARD16 (col->index, ocAddr);
		    ocAddr += 2;
		    break;
		    }

		case PEXColorTypeRGB:
		case PEXColorTypeCIE:
		case PEXColorTypeHSV:
		case PEXColorTypeHLS:
		    {
		    PEXColorRGB *col = (PEXColorRGB *) data;
		    data += sizeof (PEXColorRGB);

		    FP_CONVERT_HTON_BUFF (col->red, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    FP_CONVERT_HTON_BUFF (col->green, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    FP_CONVERT_HTON_BUFF (col->blue, ocAddr, fpFormat);
		    ocAddr += SIZEOF (float);
		    break;
		    }

		case PEXColorTypeRGB8:

		    memcpy (ocAddr, data, 4);
		    ocAddr += 4;
		    data += sizeof (PEXColorRGB8);
		    break;

		case PEXColorTypeRGB16:
		    {
		    PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		    data += sizeof (PEXColorRGB16);

		    STORE_CARD16 (col->red, ocAddr);
		    STORE_CARD16 (col->green, ocAddr);
		    STORE_CARD16 (col->blue, ocAddr);
		    ocAddr += 2;
		    break;
		    }
		}
	    }

	    if (vertAttr & PEXGANormal)
	    {
		normal = (PEXVector *) data;
		data += sizeof (PEXVector);

		FP_CONVERT_HTON_BUFF (normal->x, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (normal->y, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (normal->z, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
	    }

	    if (vertAttr & PEXGAEdges)
	    {
		edge = (unsigned int *) data;
		data += sizeof (unsigned int);
		STORE_CARD32 (*edge, ocAddr);
	    }
	}

	wordsLeft -= copyWords;
	copyWords = (wordsLeft < maxWords) ?
	    wordsLeft : (maxWords - maxWords % lenofVert);
    }	
}


void _PEXOCListOfColor (display, count, colorType, colors, fpFormat)

INPUT Display			*display;
INPUT int			count;
INPUT int			colorType;
INPUT PEXArrayOfColor		colors;
INPUT int			fpFormat;

{
    int 		lenofColor;
    int 		maxWords;
    int 		wordsLeft;
    int 		copyWords, i;
    char		*ocAddr;
    char		*data;
    
    if (!(data = (char *) colors.indexed))
	return;

    lenofColor = GetColorLength (colorType);

    maxWords = NUMWORDS (PEXGetOCAddrMaxSize (display));
    wordsLeft = count * lenofColor;
    copyWords = (wordsLeft < maxWords) ?
        wordsLeft : (maxWords - maxWords % lenofColor);

    while (copyWords > 0)
    {
        ocAddr = PEXGetOCAddr (display, NUMBYTES (copyWords));
        for (i = 0; i < (copyWords / lenofColor); i++)
        {
	    switch (colorType)
	    {
	    case PEXColorTypeIndexed:
	        {
		PEXColorIndexed *col = (PEXColorIndexed *) data;
		data += sizeof (PEXColorIndexed);

		STORE_CARD16 (col->index, ocAddr);
		ocAddr += 2;
		break;
	        }

	    case PEXColorTypeRGB:
	    case PEXColorTypeCIE:
	    case PEXColorTypeHSV:
	    case PEXColorTypeHLS:
    	        {
		PEXColorRGB *col = (PEXColorRGB *) data;
		data += sizeof (PEXColorRGB);

		FP_CONVERT_HTON_BUFF (col->red, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->green, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->blue, ocAddr, fpFormat);
		ocAddr += SIZEOF (float);
		break;
    	        }

    	    case PEXColorTypeRGB8:

    	        memcpy (ocAddr, data, 4);
    	        ocAddr += 4;
    	        data += sizeof (PEXColorRGB8);
    	        break;

    	    case PEXColorTypeRGB16:
	        {
		PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		data += sizeof (PEXColorRGB16);

		STORE_CARD16 (col->red, ocAddr);
		STORE_CARD16 (col->green, ocAddr);
		STORE_CARD16 (col->blue, ocAddr);
		ocAddr += 2;
		break;
    	        }
    	    }
	}

	wordsLeft -= copyWords;
	copyWords = (wordsLeft < maxWords) ?
	    wordsLeft : (maxWords - maxWords % lenofColor);
    }	
}


void _PEXStoreFacet (colorType, facetAttr, facetData, bufPtr, fpFormat)

INPUT int		colorType;
INPUT unsigned int	facetAttr;
INPUT PEXFacetData	*facetData;
INPUT char		**bufPtr;
INPUT int		fpFormat;

{
    PEXVector 	*normal;
    char	*pBuf = *bufPtr;

    if (!facetData)
	return;

    if (!(facetAttr & PEXGAColor))
    {
        normal = &(facetData->normal);
    }
    else
    {
        switch (colorType)
        {
        case PEXColorTypeIndexed:

            STORE_CARD16 (facetData->index.index, pBuf);
	    pBuf += 2;
    	    normal = &(facetData->index_normal.normal);
	    break;

        case PEXColorTypeRGB:
        case PEXColorTypeCIE:
        case PEXColorTypeHSV:
        case PEXColorTypeHLS:

	    FP_CONVERT_HTON_BUFF (facetData->rgb.red, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (facetData->rgb.green, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (facetData->rgb.blue, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
    	    normal = &(facetData->rgb_normal.normal);
            break;

        case PEXColorTypeRGB8:

            memcpy (pBuf, &(facetData->rgb8), 4);
            pBuf += 4;
    	    normal = &(facetData->rgb8_normal.normal);
            break;

        case PEXColorTypeRGB16:

            STORE_CARD16 (facetData->rgb16.red, pBuf);
            STORE_CARD16 (facetData->rgb16.green, pBuf);
            STORE_CARD16 (facetData->rgb16.blue, pBuf);
            pBuf += 2;
    	    normal = &(facetData->rgb16_normal.normal);
            break;
        }
    }

    if (facetAttr & PEXGANormal)
    {
	FP_CONVERT_HTON_BUFF (normal->x, pBuf, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (normal->y, pBuf, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (normal->z, pBuf, fpFormat);
	pBuf += SIZEOF (float);
    }

    *bufPtr = pBuf;
}


void _PEXStoreListOfFacet (count, colorType,
    facetAttr, facetData, bufPtr, fpFormat)

INPUT int			count;
INPUT int			colorType;
INPUT unsigned int		facetAttr;
INPUT PEXArrayOfFacetData	facetData;
INPUT char			**bufPtr;
INPUT int			fpFormat;

{
    int 	i;
    char	*data;
    PEXVector	*normal;
    char	*pBuf = *bufPtr;
    
    if (!(data = (char *) facetData.index))
	return;

    for (i = 0; i < count; i++)
    {
	if (facetAttr & PEXGAColor)
	{
	    switch (colorType)
	    {
	    case PEXColorTypeIndexed:
	    {
		PEXColorIndexed *col = (PEXColorIndexed *) data;
		data += sizeof (PEXColorIndexed);

		STORE_CARD16 (col->index, pBuf);
		pBuf += 2;
		break;
	    }

	    case PEXColorTypeRGB:
	    case PEXColorTypeCIE:
	    case PEXColorTypeHSV:
	    case PEXColorTypeHLS:
	    {
		PEXColorRGB *col = (PEXColorRGB *) data;
		data += sizeof (PEXColorRGB);

		FP_CONVERT_HTON_BUFF (col->red, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->green, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->blue, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		break;
	    }

	    case PEXColorTypeRGB8:

		memcpy (pBuf, data, 4);
	        pBuf += 4;
		data += sizeof (PEXColorRGB8);
		break;

	    case PEXColorTypeRGB16:
	    {
		PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		data += sizeof (PEXColorRGB16);

		STORE_CARD16 (col->red, pBuf);
		STORE_CARD16 (col->green, pBuf);
		STORE_CARD16 (col->blue, pBuf);
		pBuf += 2;
		break;
	    }
	    }
	}

	if (facetAttr & PEXGANormal)
	{
	    normal = (PEXVector *) data;
	    data += sizeof (PEXVector);

	    FP_CONVERT_HTON_BUFF (normal->x, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (normal->y, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (normal->z, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	}
    }

    *bufPtr = pBuf;
}


void _PEXStoreListOfVertex (count, colorType,
    vertAttr, vertData, bufPtr, fpFormat)

INPUT int			count;
INPUT int			colorType;
INPUT unsigned int		vertAttr;
INPUT PEXArrayOfVertex		vertData;
INPUT char			**bufPtr;
INPUT int			fpFormat;

{
    int 		i;
    char		*data;
    PEXVector		*normal;
    unsigned int	*edge;
    char		*pBuf = *bufPtr;

    if (!(data = (char *) vertData.index))
	return;

    for (i = 0; i < count; i++)
    {
	PEXCoord *coord = (PEXCoord *) data;
	data += sizeof (PEXCoord);

	FP_CONVERT_HTON_BUFF (coord->x, pBuf, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (coord->y, pBuf, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_HTON_BUFF (coord->z, pBuf, fpFormat);
	pBuf += SIZEOF (float);

	if (vertAttr & PEXGAColor)
	{
	    switch (colorType)
	    {
	    case PEXColorTypeIndexed:
	    {
		PEXColorIndexed *col = (PEXColorIndexed *) data;
		data += sizeof (PEXColorIndexed);

		STORE_CARD16 (col->index, pBuf);
		pBuf += 2;
		break;
	    }

	    case PEXColorTypeRGB:
	    case PEXColorTypeCIE:
	    case PEXColorTypeHSV:
	    case PEXColorTypeHLS:
	    {
		PEXColorRGB *col = (PEXColorRGB *) data;
		data += sizeof (PEXColorRGB);

		FP_CONVERT_HTON_BUFF (col->red, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->green, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_HTON_BUFF (col->blue, pBuf, fpFormat);
		pBuf += SIZEOF (float);
		break;
	    }

	    case PEXColorTypeRGB8:

		memcpy (pBuf, data, 4);
		pBuf += 4;
	        data += sizeof (PEXColorRGB8);
		break;

	    case PEXColorTypeRGB16:
	    {
		PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		data += sizeof (PEXColorRGB16);

		STORE_CARD16 (col->red, pBuf);
		STORE_CARD16 (col->green, pBuf);
		STORE_CARD16 (col->blue, pBuf);
		pBuf += 2;
		break;
	    }
	    }
	}

	if (vertAttr & PEXGANormal)
	{
	    normal = (PEXVector *) data;
	    data += sizeof (PEXVector);

	    FP_CONVERT_HTON_BUFF (normal->x, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (normal->y, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (normal->z, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	}

	if (vertAttr & PEXGAEdges)
	{
	    edge = (unsigned int *) data;
	    data += sizeof (unsigned int);
	    STORE_CARD32 (*edge, pBuf);
	}
    }	

    *bufPtr = pBuf;
}


void _PEXStoreListOfColor (count, colorType, colors, bufPtr, fpFormat)

INPUT int			count;
INPUT int			colorType;
INPUT PEXArrayOfColor		colors;
INPUT char			**bufPtr;
INPUT int			fpFormat;

{
    int 	i;
    char	*data;
    char	*pBuf = *bufPtr;
    
    if (!(data = (char *) colors.indexed))
	return;

    for (i = 0; i < count; i++)
    {
	switch (colorType)
	{
	case PEXColorTypeIndexed:
	{
	    PEXColorIndexed *col = (PEXColorIndexed *) data;
	    data += sizeof (PEXColorIndexed);

	    STORE_CARD16 (col->index, pBuf);
	    pBuf += 2;
	    break;
	}

        case PEXColorTypeRGB:
	case PEXColorTypeCIE:
	case PEXColorTypeHSV:
	case PEXColorTypeHLS:
        {
	    PEXColorRGB *col = (PEXColorRGB *) data;
	    data += sizeof (PEXColorRGB);

	    FP_CONVERT_HTON_BUFF (col->red, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (col->green, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_HTON_BUFF (col->blue, pBuf, fpFormat);
	    pBuf += SIZEOF (float);
	    break;
	}

        case PEXColorTypeRGB8:

    	    memcpy (pBuf, data, 4);
    	    pBuf += 4;
    	    data += sizeof (PEXColorRGB8);
    	    break;

         case PEXColorTypeRGB16:
         {
	     PEXColorRGB16 *col = (PEXColorRGB16 *) data;
	     data += sizeof (PEXColorRGB16);

	     STORE_CARD16 (col->red, pBuf);
	     STORE_CARD16 (col->green, pBuf);
	     STORE_CARD16 (col->blue, pBuf);
	     pBuf += 2;
	     break;
	 }
         }
    }

    *bufPtr = pBuf;
}


void _PEXExtractFacet (bufPtr, colorType, facetAttr, facetData, fpFormat)

INPUT char		**bufPtr;
INPUT int		colorType;
INPUT unsigned int	facetAttr;
INPUT PEXFacetData	*facetData;
INPUT int		fpFormat;

{
    PEXVector 	*normal;
    char	*pBuf = *bufPtr;

    if (!facetData)
	return;

    if (!(facetAttr & PEXGAColor))
    {
        normal = &(facetData->normal);
    }
    else
    {
        switch (colorType)
        {
        case PEXColorTypeIndexed:

            EXTRACT_CARD16 (pBuf, facetData->index.index);
	    pBuf += 2;
    	    normal = &(facetData->index_normal.normal);
	    break;

        case PEXColorTypeRGB:
        case PEXColorTypeCIE:
        case PEXColorTypeHSV:
        case PEXColorTypeHLS:

	    FP_CONVERT_NTOH_BUFF (pBuf, facetData->rgb.red, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, facetData->rgb.green, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, facetData->rgb.blue, fpFormat);
	    pBuf += SIZEOF (float);
    	    normal = &(facetData->rgb_normal.normal);
            break;

        case PEXColorTypeRGB8:

            memcpy (&(facetData->rgb8), pBuf, 4);
            pBuf += 4;
    	    normal = &(facetData->rgb8_normal.normal);
            break;

        case PEXColorTypeRGB16:

            EXTRACT_CARD16 (pBuf, facetData->rgb16.red);
            EXTRACT_CARD16 (pBuf, facetData->rgb16.green);
            EXTRACT_CARD16 (pBuf, facetData->rgb16.blue);
            pBuf += 2;
    	    normal = &(facetData->rgb16_normal.normal);
            break;
        }
    }

    if (facetAttr & PEXGANormal)
    {
	FP_CONVERT_NTOH_BUFF (pBuf, normal->x, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_NTOH_BUFF (pBuf, normal->y, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_NTOH_BUFF (pBuf, normal->z, fpFormat);
	pBuf += SIZEOF (float);
    }

    *bufPtr = pBuf;
}


void _PEXExtractListOfFacet (count, bufPtr, colorType,
    facetAttr, facetData, fpFormat)

INPUT int			count;
INPUT char			**bufPtr;
INPUT int			colorType;
INPUT unsigned int		facetAttr;
INPUT PEXArrayOfFacetData	facetData;
INPUT int			fpFormat;

{
    int 	i;
    char	*data;
    PEXVector	*normal;
    char	*pBuf = *bufPtr;
    
    if (!(data = (char *) facetData.index))
	return;

    for (i = 0; i < count; i++)
    {
	if (facetAttr & PEXGAColor)
	{
	    switch (colorType)
	    {
	    case PEXColorTypeIndexed:
	    {
		PEXColorIndexed *col = (PEXColorIndexed *) data;
		data += sizeof (PEXColorIndexed);

		EXTRACT_CARD16 (pBuf, col->index);
		pBuf += 2;
		break;
	    }

	    case PEXColorTypeRGB:
	    case PEXColorTypeCIE:
	    case PEXColorTypeHSV:
	    case PEXColorTypeHLS:
	    {
		PEXColorRGB *col = (PEXColorRGB *) data;
		data += sizeof (PEXColorRGB);

		FP_CONVERT_NTOH_BUFF (pBuf, col->red, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_NTOH_BUFF (pBuf, col->green, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_NTOH_BUFF (pBuf, col->blue, fpFormat);
		pBuf += SIZEOF (float);
		break;
	    }

	    case PEXColorTypeRGB8:

		memcpy (data, pBuf, 4);
	        pBuf += 4;
		data += sizeof (PEXColorRGB8);
		break;

	    case PEXColorTypeRGB16:
	    {
		PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		data += sizeof (PEXColorRGB16);

		EXTRACT_CARD16 (pBuf, col->red);
		EXTRACT_CARD16 (pBuf, col->green);
		EXTRACT_CARD16 (pBuf, col->blue);
		pBuf += 2;
		break;
	    }
	    }
	}

	if (facetAttr & PEXGANormal)
	{
	    normal = (PEXVector *) data;
	    data += sizeof (PEXVector);

	    FP_CONVERT_NTOH_BUFF (pBuf, normal->x, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, normal->y, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, normal->z, fpFormat);
	    pBuf += SIZEOF (float);
	}
    }

    *bufPtr = pBuf;
}


void _PEXExtractListOfVertex (count, bufPtr, colorType,
    vertAttr, vertData, fpFormat)

INPUT int			count;
INPUT char			**bufPtr;
INPUT int			colorType;
INPUT unsigned int		vertAttr;
INPUT PEXArrayOfVertex		vertData;
INPUT int			fpFormat;

{
    int 		i;
    char		*data;
    PEXVector		*normal;
    unsigned int	*edge;
    char		*pBuf = *bufPtr;

    if (!(data = (char *) vertData.index))
	return;

    for (i = 0; i < count; i++)
    {
	PEXCoord *coord = (PEXCoord *) data;
	data += sizeof (PEXCoord);

	FP_CONVERT_NTOH_BUFF (pBuf, coord->x, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_NTOH_BUFF (pBuf, coord->y, fpFormat);
	pBuf += SIZEOF (float);
	FP_CONVERT_NTOH_BUFF (pBuf, coord->z, fpFormat);
	pBuf += SIZEOF (float);

	if (vertAttr & PEXGAColor)
	{
	    switch (colorType)
	    {
	    case PEXColorTypeIndexed:
	    {
		PEXColorIndexed *col = (PEXColorIndexed *) data;
		data += sizeof (PEXColorIndexed);

		EXTRACT_CARD16 (pBuf, col->index);
		pBuf += 2;
		break;
	    }

	    case PEXColorTypeRGB:
	    case PEXColorTypeCIE:
	    case PEXColorTypeHSV:
	    case PEXColorTypeHLS:
	    {
		PEXColorRGB *col = (PEXColorRGB *) data;
		data += sizeof (PEXColorRGB);

		FP_CONVERT_NTOH_BUFF (pBuf, col->red, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_NTOH_BUFF (pBuf, col->green, fpFormat);
		pBuf += SIZEOF (float);
		FP_CONVERT_NTOH_BUFF (pBuf, col->blue, fpFormat);
		pBuf += SIZEOF (float);
		break;
	    }

	    case PEXColorTypeRGB8:

		memcpy (data, pBuf, 4);
		pBuf += 4;
	        data += sizeof (PEXColorRGB8);
		break;

	    case PEXColorTypeRGB16:
	    {
		PEXColorRGB16 *col = (PEXColorRGB16 *) data;
		data += sizeof (PEXColorRGB16);

		EXTRACT_CARD16 (pBuf, col->red);
		EXTRACT_CARD16 (pBuf, col->green);
		EXTRACT_CARD16 (pBuf, col->blue);
		pBuf += 2;
		break;
	    }
	    }
	}

	if (vertAttr & PEXGANormal)
	{
	    normal = (PEXVector *) data;
	    data += sizeof (PEXVector);

	    FP_CONVERT_NTOH_BUFF (pBuf, normal->x, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, normal->y, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, normal->z, fpFormat);
	    pBuf += SIZEOF (float);
	}

	if (vertAttr & PEXGAEdges)
	{
	    edge = (unsigned int *) data;
	    data += sizeof (unsigned int);
	    EXTRACT_CARD32 (pBuf, *edge);
	}
    }	

    *bufPtr = pBuf;
}


void _PEXExtractListOfColor (count, bufPtr, colorType, colors, fpFormat)

INPUT int			count;
INPUT char			**bufPtr;
INPUT int			colorType;
INPUT PEXArrayOfColor		colors;
INPUT int			fpFormat;

{
    int 	i;
    char	*data;
    char	*pBuf = *bufPtr;
    
    if (!(data = (char *) colors.indexed))
	return;

    for (i = 0; i < count; i++)
    {
	switch (colorType)
	{
	case PEXColorTypeIndexed:
	{
	    PEXColorIndexed *col = (PEXColorIndexed *) data;
	    data += sizeof (PEXColorIndexed);

	    EXTRACT_CARD16 (pBuf, col->index);
	    pBuf += 2;
	    break;
	}

        case PEXColorTypeRGB:
	case PEXColorTypeCIE:
	case PEXColorTypeHSV:
	case PEXColorTypeHLS:
        {
	    PEXColorRGB *col = (PEXColorRGB *) data;
	    data += sizeof (PEXColorRGB);

	    FP_CONVERT_NTOH_BUFF (pBuf, col->red, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, col->green, fpFormat);
	    pBuf += SIZEOF (float);
	    FP_CONVERT_NTOH_BUFF (pBuf, col->blue, fpFormat);
	    pBuf += SIZEOF (float);
	    break;
	}

        case PEXColorTypeRGB8:

    	    memcpy (data, pBuf, 4);
    	    pBuf += 4;
    	    data += sizeof (PEXColorRGB8);
    	    break;

         case PEXColorTypeRGB16:
         {
	     PEXColorRGB16 *col = (PEXColorRGB16 *) data;
	     data += sizeof (PEXColorRGB16);

	     EXTRACT_CARD16 (pBuf, col->red);
	     EXTRACT_CARD16 (pBuf, col->green);
	     EXTRACT_CARD16 (pBuf, col->blue);
	     pBuf += 2;
	     break;
	 }
         }
    }

    *bufPtr = pBuf;
}


void _PEXGenOCBadLengthError (display, resource_id, req_type)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;

{
    PEXDisplayInfo 	*pexDisplayInfo;
    pexOCRequestHeader 	*req;


    /*
     * Generate an OC request with a zero request length.
     */

    LockDisplay (display);

    PEXGetDisplayInfo (display, pexDisplayInfo);
    PEXGetOCReq (0);

    BEGIN_NEW_OCREQ_HEADER (display->bufptr, req);

    req->extOpcode = pexDisplayInfo->extOpcode;
    req->pexOpcode =
	(req_type == PEXOCStore || req_type == PEXOCStoreSingle) ?
	PEXRCStoreElements : PEXRCRenderOutputCommands;
    req->reqLength = 0;
    req->fpFormat = pexDisplayInfo->fpFormat;
    req->target = resource_id;
    req->numCommands = 1;

    END_NEW_OCREQ_HEADER (display->bufptr, req);

    /*
     * Make sure that the next oc starts a new request.
     */

    pexDisplayInfo->lastReqNum = -1;
    pexDisplayInfo->lastResID = resource_id;
    pexDisplayInfo->lastReqType = req_type;
	
    UnlockDisplay (display);
}
