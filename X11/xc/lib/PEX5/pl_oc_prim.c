/* $Xorg: pl_oc_prim.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */

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


void
PEXMarkers (display, resource_id, req_type, numPoints, points)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	numPoints;
INPUT PEXCoord 		*points;

{
    register pexMarkers	*req;
    char		*pBuf;
    int			dataLength;
    int			fpConvert;
    int			fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numPoints * SIZEOF (pexCoord3D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexMarkers), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Markers, dataLength, pBuf, req);
    END_OC_HEADER (Markers, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_COORD3D (numPoints, points, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXMarkers2D (display, resource_id, req_type, numPoints, points)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	numPoints;
INPUT PEXCoord2D	*points;

{
    register pexMarkers2D	*req;
    char			*pBuf;
    int				dataLength;
    int				fpConvert;
    int				fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numPoints * SIZEOF (pexCoord2D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexMarkers2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Markers2D, dataLength, pBuf, req);
    END_OC_HEADER (Markers2D, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_COORD2D (numPoints, points, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXText (display, resource_id, req_type, origin, vec1, vec2, count, string)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord		*origin;
INPUT PEXVector		*vec1;
INPUT PEXVector		*vec2;
INPUT int		count;
INPUT char		*string;

{
    register pexText	*req;
    char		*pBuf;
    int			fpConvert;
    int			fpFormat;
    int			dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = LENOF (pexMonoEncoding) + NUMWORDS (count);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexText), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the text request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Text, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (origin->z, req->origin_z, fpFormat);
	FP_CONVERT_HTON (vec1->x, req->vector1_x, fpFormat);
	FP_CONVERT_HTON (vec1->y, req->vector1_y, fpFormat);
	FP_CONVERT_HTON (vec1->z, req->vector1_z, fpFormat);
	FP_CONVERT_HTON (vec2->x, req->vector2_x, fpFormat);
	FP_CONVERT_HTON (vec2->y, req->vector2_y, fpFormat);
	FP_CONVERT_HTON (vec2->z, req->vector2_z, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->origin_z = origin->z;
	req->vector1_x = vec1->x;
	req->vector1_y = vec1->y;
	req->vector1_z = vec1->z;
	req->vector2_x = vec2->x;
	req->vector2_y = vec2->y;
	req->vector2_z = vec2->z;
    }

    req->numEncodings = (CARD16) 1;

    END_OC_HEADER (Text, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_DEFAULT_MONO_STRING (count, string);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXText2D (display, resource_id, req_type, origin, count, string)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord2D	*origin;
INPUT int		count;
INPUT char		*string;

{
    register pexText2D	*req;
    char		*pBuf;
    int			fpConvert;
    int			fpFormat;
    int			dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = LENOF (pexMonoEncoding) + NUMWORDS (count);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexText2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the text header request data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Text2D, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
    }

    req->numEncodings = (CARD16) 1;

    END_OC_HEADER (Text2D, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_DEFAULT_MONO_STRING (count, string);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXAnnotationText (display, resource_id, req_type,
    origin, offset, count, string)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord		*origin;
INPUT PEXCoord		*offset;
INPUT int		count;
INPUT char		*string;

{
    register pexAnnotationText	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = LENOF (pexMonoEncoding) + NUMWORDS (count);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexAnnotationText), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the atext request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (AnnotationText, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (origin->z, req->origin_z, fpFormat);
	FP_CONVERT_HTON (offset->x, req->offset_x, fpFormat);
	FP_CONVERT_HTON (offset->y, req->offset_y, fpFormat);
	FP_CONVERT_HTON (offset->z, req->offset_z, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->origin_z = origin->z;
	req->offset_x = offset->x;
	req->offset_y = offset->y;
	req->offset_z = offset->z;
    }

    req->numEncodings = (CARD16) 1;

    END_OC_HEADER (AnnotationText, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_DEFAULT_MONO_STRING (count, string);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXAnnotationText2D (display, resource_id, req_type,
    origin, offset, count, string)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord2D	*origin;
INPUT PEXCoord2D	*offset;
INPUT int		count;
INPUT char		*string;

{
    register pexAnnotationText2D	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = LENOF (pexMonoEncoding) + NUMWORDS (count);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexAnnotationText2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the atext request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (AnnotationText2D, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (offset->x, req->offset_x, fpFormat);
	FP_CONVERT_HTON (offset->y, req->offset_y, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->offset_x = offset->x;
	req->offset_y = offset->y;
    }

    req->numEncodings = (CARD16) 1;

    END_OC_HEADER (AnnotationText2D, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_DEFAULT_MONO_STRING (count, string);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXEncodedText (display, resource_id, req_type,
    origin, vec1, vec2, numEncodings, encodedTextList)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXCoord			*origin;
INPUT PEXVector			*vec1;
INPUT PEXVector			*vec2;
INPUT unsigned int		numEncodings;
INPUT PEXEncodedTextData	*encodedTextList;

{
    register pexText		*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				lenofStrings;


    /*
     * Get length of mono-encoded strings. 
     */

    GetStringsLength (numEncodings, encodedTextList, lenofStrings);


    /*
     * Initialize the OC request.
     */

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexText), lenofStrings, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the text request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Text, lenofStrings, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (origin->z, req->origin_z, fpFormat);
	FP_CONVERT_HTON (vec1->x, req->vector1_x, fpFormat);
	FP_CONVERT_HTON (vec1->y, req->vector1_y, fpFormat);
	FP_CONVERT_HTON (vec1->z, req->vector1_z, fpFormat);
	FP_CONVERT_HTON (vec2->x, req->vector2_x, fpFormat);
	FP_CONVERT_HTON (vec2->y, req->vector2_y, fpFormat);
	FP_CONVERT_HTON (vec2->z, req->vector2_z, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->origin_z = origin->z;
	req->vector1_x = vec1->x;
	req->vector1_y = vec1->y;
	req->vector1_z = vec1->z;
	req->vector2_x = vec2->x;
	req->vector2_y = vec2->y;
	req->vector2_z = vec2->z;
    }

    req->numEncodings = (CARD16) numEncodings;

    END_OC_HEADER (Text, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_LISTOF_MONO_STRING (numEncodings, encodedTextList);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXEncodedText2D (display, resource_id, req_type,
    origin, numEncodings, encodedTextList)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXCoord2D		*origin;
INPUT unsigned int		numEncodings;
INPUT PEXEncodedTextData	*encodedTextList;

{
    register pexText2D		*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				lenofStrings;


    /*
     * Get length of mono-encoded strings. 
     */

    GetStringsLength (numEncodings, encodedTextList, lenofStrings);


    /*
     * Initialize the OC request.
     */

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexText2D), lenofStrings, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the text header request data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Text2D, lenofStrings, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
    }

    req->numEncodings = (CARD16) numEncodings;

    END_OC_HEADER (Text2D, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_LISTOF_MONO_STRING (numEncodings, encodedTextList);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXEncodedAnnoText (display, resource_id, req_type,
    origin, offset, numEncodings, encodedTextList)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXCoord			*origin;
INPUT PEXCoord			*offset;
INPUT unsigned int		numEncodings;
INPUT PEXEncodedTextData 	*encodedTextList;

{
    register pexAnnotationText	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				lenofStrings;


    /*
     * Get length of mono-encoded strings. 
     */

    GetStringsLength (numEncodings, encodedTextList, lenofStrings);


    /*
     * Initialize the OC request.
     */

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexAnnotationText), lenofStrings, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the atext request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (AnnotationText, lenofStrings, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (origin->z, req->origin_z, fpFormat);
	FP_CONVERT_HTON (offset->x, req->offset_x, fpFormat);
	FP_CONVERT_HTON (offset->y, req->offset_y, fpFormat);
	FP_CONVERT_HTON (offset->z, req->offset_z, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->origin_z = origin->z;
	req->offset_x = offset->x;
	req->offset_y = offset->y;
	req->offset_z = offset->z;
    }

    req->numEncodings = (CARD16) numEncodings;

    END_OC_HEADER (AnnotationText, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_LISTOF_MONO_STRING (numEncodings, encodedTextList);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXEncodedAnnoText2D (display, resource_id, req_type,
    origin, offset, numEncodings, encodedTextList)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT PEXCoord2D		*origin;
INPUT PEXCoord2D		*offset;
INPUT unsigned int		numEncodings;
INPUT PEXEncodedTextData 	*encodedTextList;

{
    register pexAnnotationText2D	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					lenofStrings;


    /*
     * Get length of mono-encoded strings. 
     */

    GetStringsLength (numEncodings, encodedTextList, lenofStrings);


    /*
     * Initialize the OC request.
     */

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexAnnotationText2D), lenofStrings, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the atext request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (AnnotationText2D, lenofStrings, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (origin->x, req->origin_x, fpFormat);
	FP_CONVERT_HTON (origin->y, req->origin_y, fpFormat);
	FP_CONVERT_HTON (offset->x, req->offset_x, fpFormat);
	FP_CONVERT_HTON (offset->y, req->offset_y, fpFormat);
    }
    else
    {
	req->origin_x = origin->x;
	req->origin_y = origin->y;
	req->offset_x = offset->x;
	req->offset_y = offset->y;
    }

    req->numEncodings = (CARD16) numEncodings;

    END_OC_HEADER (AnnotationText2D, pBuf, req);


    /*
     * Store the mono-encoded string.
     */

    OC_LISTOF_MONO_STRING (numEncodings, encodedTextList);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXPolyline (display, resource_id, req_type, numVertices, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	numVertices;
INPUT PEXCoord		*vertices;

{
    register pexPolyline	*req;
    char			*pBuf;
    int				dataLength;
    int				fpConvert;
    int				fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numVertices * SIZEOF (pexCoord3D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexPolyline), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Polyline, dataLength, pBuf, req);
    END_OC_HEADER (Polyline, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_COORD3D (numVertices, vertices, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXPolyline2D (display, resource_id, req_type, numVertices, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	numVertices;
INPUT PEXCoord2D	*vertices;

{
    register pexPolyline2D	*req;
    char			*pBuf;
    int				dataLength;
    int				fpConvert;
    int				fpFormat;

    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (numVertices * SIZEOF (pexCoord2D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexPolyline2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (Polyline2D, dataLength, pBuf, req);
    END_OC_HEADER (Polyline2D, pBuf, req);


    /*
     * Copy the oc data.
     */

    OC_LISTOF_COORD2D (numVertices, vertices, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXPolylineSetWithData (display, resource_id, req_type,
    vertexAttributes, colorType, numPolylines, polylines)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT unsigned int	vertexAttributes;
INPUT int		colorType;
INPUT unsigned int	numPolylines;
INPUT PEXListOfVertex	*polylines;

{
    register pexPolylineSetWithData	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					lenofVertex;
    int					numPoints, i;
    int					dataLength;
    char				*pData;


    /* 
     * Calculate the total number of vertices.
     */

    for (i = 0, numPoints = 0; i < numPolylines; i++)
	numPoints += polylines[i].count;


    /* 
     * Calculate how big each vertex is.
     */

    lenofVertex = LENOF (pexCoord3D) +
	((vertexAttributes & PEXGAColor) ? GetColorLength (colorType) : 0); 


    /*
     * Initialize the OC request.
     */

    dataLength = numPolylines + (numPoints * lenofVertex);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexPolylineSetWithData), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the polyline request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (PolylineSetWithData, dataLength, pBuf, req);

    req->colorType = colorType;
    req->vertexAttribs = vertexAttributes;
    req->numLists = numPolylines;

    END_OC_HEADER (PolylineSetWithData, pBuf, req);


    /* 
     * For each polyline store a count and then the list of vertices. 
     * Note that the vertices are padded to end on a word boundary
     */

    vertexAttributes &= PEXGAColor;	   /* only color allowed at vertices */

    for (i = 0; i < numPolylines; i++)
    {
	pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
	PUT_CARD32 (polylines[i].count, pData);

	OC_LISTOF_VERTEX (polylines[i].count, lenofVertex, colorType,
	    vertexAttributes, polylines[i].vertices, fpConvert, fpFormat);
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXNURBCurve (display, resource_id, req_type,
    rationality, order, knots, numPoints, points, tmin, tmax)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		rationality;
INPUT int		order;
INPUT float		*knots;
INPUT unsigned int	numPoints;
INPUT PEXArrayOfCoord	points;
INPUT double		tmin;
INPUT double		tmax;

{
    register pexNURBCurve 	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;
    int				lenofVertexList;
    int				lenofKnotList;


    /* 
     * Calculate the number of words in the vertex list and knot list.  The 
     * number of knots = order + number of points.
     */

    lenofVertexList = numPoints * ((rationality == PEXRational) ?
	LENOF (pexCoord4D) : LENOF (pexCoord3D));
    lenofKnotList = order + numPoints;


    /*
     * Initialize the OC request.
     */

    dataLength = lenofKnotList + lenofVertexList;

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexNURBCurve), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the nurb request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (NURBCurve, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_DHTON (tmin, req->tmin, fpFormat);
	FP_CONVERT_DHTON (tmax, req->tmax, fpFormat);
    }
    else
    {
	req->tmin = tmin;
	req->tmax = tmax;
    }

    req->curveOrder = order;
    req->coordType = rationality;
    req->numKnots = order + numPoints;
    req->numPoints = numPoints;

    END_OC_HEADER (NURBCurve, pBuf, req);


    /*
     * Store the knot list and the vertex list.
     */

    OC_LISTOF_FLOAT32 (lenofKnotList, knots, fpConvert, fpFormat);

    if (rationality == PEXRational)
    {
	OC_LISTOF_COORD4D (numPoints, points.point_4d,
            fpConvert, fpFormat);
    }
    else
    {
	OC_LISTOF_COORD3D (numPoints, points.point,
            fpConvert, fpFormat);
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillArea (display, resource_id, req_type,
    shape, ignoreEdges, numPoints, points)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT unsigned int	numPoints;
INPUT PEXCoord 		*points;

{
    register pexFillArea	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = numPoints * LENOF (pexCoord3D);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillArea), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area request data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillArea, dataLength, pBuf, req);

    req->shape = shape;
    req->ignoreEdges = ignoreEdges;

    END_OC_HEADER (FillArea, pBuf, req);


    /*
     * Copy the vertex data to the oc.
     */

    OC_LISTOF_COORD3D (numPoints, points, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillArea2D (display, resource_id, req_type,
    shape, ignoreEdges, numPoints, points)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT unsigned int	numPoints;
INPUT PEXCoord2D	*points;

{
    register pexFillArea2D	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = numPoints * LENOF (pexCoord2D);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillArea2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area request data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillArea2D, dataLength, pBuf, req);

    req->shape = shape;
    req->ignoreEdges = ignoreEdges;

    END_OC_HEADER (FillArea2D, pBuf, req);


    /*
     * Copy the vertex data to the oc.
     */

    OC_LISTOF_COORD2D (numPoints, points, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillAreaWithData (display, resource_id, req_type,
    shape, ignoreEdges, facetAttributes, vertexAttributes, colorType,
    facetData, numVertices, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT unsigned int	facetAttributes;
INPUT unsigned int	vertexAttributes;
INPUT int		colorType;
INPUT PEXFacetData	*facetData;
INPUT unsigned int	numVertices;
INPUT PEXArrayOfVertex	vertices;

{
    register pexFillAreaWithData 	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					dataLength;
    int					lenofFacet;
    int					lenofVertex;
    int					lenofColor;
    char				*pData;


    /* 
     * Calculate the number of words in the optional facet data and the
     * number of words per vertex.
     */

    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);


    /*
     * Initialize the OC request.
     */

    dataLength = lenofFacet + 1 /* count */ + numVertices * lenofVertex;
  
    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillAreaWithData), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillAreaWithData, dataLength, pBuf, req);

    req->shape = shape;
    req->ignoreEdges = ignoreEdges;
    req->colorType = colorType;
    req->facetAttribs = facetAttributes;
    req->vertexAttribs = vertexAttributes;

    END_OC_HEADER (FillAreaWithData, pBuf, req);


    /*
     * Copy the facet data.
     */

    if (facetAttributes)
    {
	OC_FACET (colorType, facetAttributes, facetData, fpConvert, fpFormat);
    }


    /*
     * Copy the vertex data.
     */

    pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
    PUT_CARD32 (numVertices, pData);

    OC_LISTOF_VERTEX (numVertices, lenofVertex, colorType,
	vertexAttributes, vertices, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillAreaSet (display, resource_id, req_type,
    shape, ignoreEdges,	contourHint, numFillAreas, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT int		contourHint;
INPUT unsigned int	numFillAreas;
INPUT PEXListOfCoord	*vertices;

{
    register pexFillAreaSet	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;
    int				numPoints, i;
    char			*pData;


    /* 
     * Calculate the total number of vertices 
     */

    for (i = 0, numPoints = 0; i < numFillAreas; i++)
	numPoints += vertices[i].count;


    /*
     * Initialize the OC request.
     */

    dataLength = numFillAreas /* counts */ + (numPoints * LENOF (pexCoord3D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillAreaSet), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area set request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillAreaSet, dataLength, pBuf, req);

    req->shape = shape; 
    req->ignoreEdges = ignoreEdges;
    req->contourHint = contourHint;
    req->numLists = numFillAreas;

    END_OC_HEADER (FillAreaSet, pBuf, req);


    /*
     * Now store the fill area set.  Each fill area in the set consists of
     * a vertex count followed by a polygon.
     */

    for (i = 0; i < numFillAreas; i++)
    {
	pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
	PUT_CARD32 (vertices[i].count, pData);

	OC_LISTOF_COORD3D (vertices[i].count, vertices[i].points,
	    fpConvert, fpFormat);
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillAreaSet2D (display, resource_id, req_type,
    shape, ignoreEdges, contourHint, numFillAreas, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT int		contourHint;
INPUT unsigned int	numFillAreas;
INPUT PEXListOfCoord2D	*vertices;

{
    register pexFillAreaSet2D	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;
    int				numPoints, i;
    char			*pData;


    /* 
     * Calculate the total number of vertices 
     */

    for (i = 0, numPoints = 0; i < numFillAreas; i++)
	numPoints += vertices[i].count;


    /*
     * Initialize the OC request.
     */

    dataLength = numFillAreas /* counts */ + (numPoints * LENOF (pexCoord2D));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillAreaSet2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area set request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillAreaSet2D, dataLength, pBuf, req);

    req->shape = shape;
    req->ignoreEdges = ignoreEdges;
    req->contourHint = contourHint;
    req->numLists = numFillAreas;

    END_OC_HEADER (FillAreaSet2D, pBuf, req);


    /*
     * Now store the fill area set.  Each fill area in the set consists of
     * a vertex count followed by a polygon.
     */

    for (i = 0; i < numFillAreas; i++)
    {
	pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
	PUT_CARD32 (vertices[i].count, pData);

	OC_LISTOF_COORD2D (vertices[i].count, vertices[i].points,
	    fpConvert, fpFormat);
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXFillAreaSetWithData (display, resource_id, req_type,
    shape, ignoreEdges, contourHint, facetAttributes, vertexAttributes,
    colorType, numFillAreas, facetData, vertices)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT int		shape;
INPUT int		ignoreEdges;
INPUT int		contourHint;
INPUT unsigned int	facetAttributes;
INPUT unsigned int	vertexAttributes;
INPUT int		colorType;
INPUT unsigned int	numFillAreas;
INPUT PEXFacetData	*facetData;
INPUT PEXListOfVertex	*vertices;

{
    register pexFillAreaSetWithData	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					dataLength;
    int					lenofColor;
    int					lenofFacet;
    int					lenofVertex;
    int					numVertices, i;
    char				*pData;


    /* 
     * Calculate the size of the facet data and vertex data.
     */

    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);

    if (vertexAttributes & PEXGAEdges)
	lenofVertex++; 			/* edge switch is CARD32 */

    for (i = 0, numVertices = 0; i < numFillAreas; i++)
	numVertices += vertices[i].count;


    /*
     * Initialize the OC request.
     */

    dataLength = lenofFacet + numFillAreas + numVertices * lenofVertex;

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexFillAreaSetWithData), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the fill area set request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (FillAreaSetWithData, dataLength, pBuf, req);

    req->shape = shape;
    req->ignoreEdges = ignoreEdges;
    req->contourHint = contourHint;
    req->colorType = colorType;
    req->facetAttribs = facetAttributes;
    req->vertexAttribs = vertexAttributes;
    req->numLists = numFillAreas;

    END_OC_HEADER (FillAreaSetWithData, pBuf, req);


    /*
     * Copy the facet data
     */

    if (facetAttributes)
    {
	OC_FACET (colorType, facetAttributes, facetData, fpConvert, fpFormat);
    }


    /*
     * Copy the polygon vertices, preceded by a count.
     * Note that the vertices are padded to end on a word boundary.
     */

    for (i = 0; i < numFillAreas; i++)
    {
	pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
	PUT_CARD32 (vertices[i].count, pData);

	OC_LISTOF_VERTEX (vertices[i].count, lenofVertex, colorType,
	    vertexAttributes, vertices[i].vertices, fpConvert, fpFormat);
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXSetOfFillAreaSets (display, resource_id, req_type, shape,
    facetAttributes, vertexAttributes, edgeAttributes, contourHint,
    contoursAllOne, colorType, numFillAreaSets, facetData,
    numVertices, vertices, numIndices, edgeFlags, connectivity)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT int			shape;
INPUT unsigned int		facetAttributes;
INPUT unsigned int		vertexAttributes;
INPUT unsigned int		edgeAttributes;
INPUT int			contourHint;
INPUT int			contoursAllOne;
INPUT int			colorType;
INPUT unsigned int		numFillAreaSets;
INPUT PEXArrayOfFacetData 	facetData;
INPUT unsigned int		numVertices; 
INPUT PEXArrayOfVertex		vertices;
INPUT unsigned int		numIndices; 
INPUT PEXSwitch			*edgeFlags;
INPUT PEXConnectivityData	*connectivity;

{
    register pexSetOfFillAreaSets	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					dataLength;
    PEXConnectivityData 		*pConnectivity;
    PEXListOfUShort 			*pList;
    int 				lenofColor;
    int 				lenofFacet;
    int 				lenofVertex;
    int 				sizeofEdge;
    int 				numContours;
    int 				count, scount;
    int					cbytes;
    int					i, j;
    char				*pData;


    /* 
     * Calculate the total number of contours.
     */

    numContours = 0;
    pConnectivity = connectivity;
    for (i = 0; i < numFillAreaSets; i++, pConnectivity++)
	numContours += pConnectivity->count;


    /* 
     * Calculate the size of the facet data and vertex data
     */

    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);
    sizeofEdge = edgeAttributes ? SIZEOF (CARD8) : 0;

    cbytes = SIZEOF (CARD16) * (numFillAreaSets + numContours + numIndices);


    /*
     * Initialize the OC request.
     */

    dataLength = (lenofFacet * numFillAreaSets) +
	(lenofVertex * numVertices) + 
	NUMWORDS (sizeofEdge * numIndices) + NUMWORDS (cbytes);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexSetOfFillAreaSets), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the SOFA request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (SetOfFillAreaSets, dataLength, pBuf, req);

    req->shape = shape;
    req->colorType = colorType;
    req->FAS_Attributes = facetAttributes;
    req->vertexAttributes = vertexAttributes;
    req->edgeAttributes = edgeAttributes ? PEXOn : PEXOff;
    req->contourHint = contourHint;
    req->contourCountsFlag = contoursAllOne;
    req->numFAS = numFillAreaSets;
    req->numVertices = numVertices;
    req->numEdges = numIndices;
    req->numContours = numContours;

    END_OC_HEADER (SetOfFillAreaSets, pBuf, req);


    /*
     * Copy the facet data.
     */

    if (facetAttributes)
    {
	OC_LISTOF_FACET (numFillAreaSets, lenofFacet, colorType,
	    facetAttributes, facetData, fpConvert, fpFormat);
    }


    /*
     * Copy the vertex data.
     */

    OC_LISTOF_VERTEX (numVertices, lenofVertex, colorType,
	vertexAttributes, vertices, fpConvert, fpFormat);


    /*
     * Copy the edge data.
     */

    if (edgeAttributes)
    {
	OC_LISTOF_CARD8_PAD (numIndices, edgeFlags);
    }


    /*
     * Now add the connectivity data.
     *
     * Unfortunately, the encoding for LISTofLISTofLISTofCARD16
     * is broken - there is no padding within this entity, just
     * at the end.  As a result, display->bufptr can be on a
     * non word aligned boundary when _XSend or _XFlush is called!!!
     *
     * If the connectivity data fits in the X transport buffer,
     * _XSend and _XFlush is not used, so we're safe.
     *
     * If the connectivity data doesn't fit in the X transport buffer,
     * we fix the problem by putting the connectivity data in a scratch
     * buffer, then using _XSend.
     */

    pConnectivity = connectivity;

    if ((cbytes + PAD (cbytes)) <= BytesLeftInXBuffer (display))
    {
	for (i = 0; i < numFillAreaSets; i++)
	{
	    count = pConnectivity->count;
	    pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD16));
	    PUT_CARD16 (count, pData);

	    for (j = 0, pList = pConnectivity->lists; j < count; j++, pList++)
	    {
		scount = pList->count;
		pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD16));
		PUT_CARD16 (scount, pData);
		
		OC_LISTOF_CARD16 (scount, pList->shorts);
	    }
	    
	    pConnectivity++;
	}

	if (PAD (cbytes))
	    PEXGetOCAddr (display, PAD (cbytes));
    }
    else
    {
	char *pStart = _XAllocScratch (display, cbytes + PAD (cbytes));
	pData = pStart;

	for (i = 0; i < numFillAreaSets; i++)
	{
	    count = pConnectivity->count;
	    STORE_CARD16 (count, pData);

	    for (j = 0, pList = pConnectivity->lists; j < count; j++, pList++)
	    {
		scount = pList->count;
		STORE_CARD16 (scount, pData);

		STORE_LISTOF_CARD16 (scount, pList->shorts, pData);
	    }

	    pConnectivity++;
	}

	_XSend (display, pStart, cbytes + PAD (cbytes));
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXTriangleStrip (display, resource_id, req_type,
    facetAttributes, vertexAttributes, colorType,
    facetData, numVertices, vertices)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT unsigned int		facetAttributes;
INPUT unsigned int		vertexAttributes;
INPUT int			colorType;
INPUT PEXArrayOfFacetData 	facetData;
INPUT unsigned int		numVertices;
INPUT PEXArrayOfVertex		vertices;

{
    register pexTriangleStrip	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;
    int				lenofColor;
    int				lenofFacet;
    int				lenofVertex;


    /* 
     * Calculate number of words in the list of facet data and the vertex list.
     */

    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);


    /*
     * Initialize the OC request.
     */

    dataLength = (numVertices - 2) * lenofFacet + numVertices * lenofVertex;

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexTriangleStrip), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the triangle strip request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (TriangleStrip, dataLength, pBuf, req);

    req->colorType = colorType;
    req->facetAttribs = facetAttributes;
    req->vertexAttribs = vertexAttributes;
    req->numVertices = numVertices;

    END_OC_HEADER (TriangleStrip, pBuf, req);


    /*
     * Copy the facet data.
     */

    if (facetAttributes)
    {
	OC_LISTOF_FACET ((numVertices - 2), lenofFacet, colorType,
	    facetAttributes, facetData, fpConvert, fpFormat);
    }


    /*
     * Copy the vertex data.
     */

    OC_LISTOF_VERTEX (numVertices, lenofVertex, colorType,
	vertexAttributes, vertices, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXQuadrilateralMesh (display, resource_id, req_type,
    shape, facetAttributes, vertexAttributes, colorType,
    facetData, colCount, rowCount, vertices)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT int			shape;
INPUT unsigned int		facetAttributes;
INPUT unsigned int		vertexAttributes;
INPUT int			colorType;
INPUT PEXArrayOfFacetData 	facetData;
INPUT unsigned int		colCount;
INPUT unsigned int		rowCount;
INPUT PEXArrayOfVertex		vertices;

{
    register pexQuadrilateralMesh 	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;
    int					dataLength;
    int					lenofColor;
    int					lenofFacet;
    int					lenofVertex;


    /* 
     * Calculate the number of words in the facet data list and vertex list.
     */

    lenofColor = GetColorLength (colorType);
    lenofFacet = GetFacetDataLength (facetAttributes, lenofColor); 
    lenofVertex = GetVertexWithDataLength (vertexAttributes, lenofColor);


    /*
     * Initialize the OC request.
     */

    dataLength = (((rowCount - 1) * (colCount - 1)) * lenofFacet) +
	(rowCount * colCount * lenofVertex);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexQuadrilateralMesh), dataLength, pBuf);

    if (pBuf == NULL) return;


    /* 
     * Store the quad mesh request header data. 
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (QuadrilateralMesh, dataLength, pBuf, req);

    req->colorType = colorType;
    req->mPts = colCount;
    req->nPts = rowCount;
    req->facetAttribs = facetAttributes;
    req->vertexAttribs = vertexAttributes;
    req->shape = shape;

    END_OC_HEADER (QuadrilateralMesh, pBuf, req);


    /*
     * Copy the facet data.
     */

    if (facetAttributes)
    {
	OC_LISTOF_FACET ((rowCount - 1) * (colCount - 1), lenofFacet,
	    colorType, facetAttributes, facetData, fpConvert, fpFormat);
    }


    /*
     * Copy the vertex data.
     */

    OC_LISTOF_VERTEX (rowCount * colCount, lenofVertex, colorType,
	vertexAttributes, vertices, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXNURBSurface (display, resource_id, req_type, rationality, uorder, vorder,
    uknots, vknots, numMPoints, numNPoints, points, numTrimLoops, trimLoops)

INPUT Display			*display;
INPUT XID			resource_id;
INPUT PEXOCRequestType		req_type;
INPUT int			rationality;
INPUT int			uorder;
INPUT int			vorder;
INPUT float			*uknots;
INPUT float			*vknots;
INPUT unsigned int		numMPoints;
INPUT unsigned int		numNPoints;
INPUT PEXArrayOfCoord		points;
INPUT unsigned int		numTrimLoops;
INPUT PEXListOfTrimCurve 	*trimLoops;

{
    register pexNURBSurface	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;
    int				lenofVertexList;
    int				lenofUKnotList;
    int				lenofVKnotList;
    int				lenofTrimData;
    int				thisLength, i;
    int				count;
    pexTrimCurve		*pTCHead;
    PEXTrimCurve		*ptrimCurve;
    PEXListOfTrimCurve		*ptrimLoop;
    char			*pData;
    char			*ocAddr;


    /* 
     * Calculate the number of words in the vertex list and the knot lists.
     */

    lenofVertexList = numMPoints * numNPoints *
        ((rationality == PEXRational) ?
	LENOF (pexCoord4D) : LENOF (pexCoord3D));
    lenofUKnotList = uorder + numMPoints;
    lenofVKnotList = vorder + numNPoints;


    /* 
     * Calculate the number of words in the trim curve data.  Note that the
     * vertices for the trim curve are in parametric space so they are either
     * 3D or 2D.
     */

    lenofTrimData = numTrimLoops * LENOF (CARD32);   /* count per loop */

    for (i = 0, ptrimLoop = trimLoops; i < numTrimLoops; i++, ptrimLoop++)
    {
	ptrimCurve = ptrimLoop->curves;
	count = ptrimLoop->count;
		
	while (count--)
	{
	    lenofTrimData += (LENOF (pexTrimCurve) +
		ptrimCurve->count + ptrimCurve->order +     /* knot list */
		ptrimCurve->count *
		(ptrimCurve->rationality == PEXRational ?
		    LENOF (pexCoord3D) : LENOF (pexCoord2D)));
	    ptrimCurve++;
	}
    }


    /*
     * Initialize the OC request.
     */

    dataLength = lenofUKnotList + lenofVKnotList +
	lenofVertexList + lenofTrimData;

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexNURBSurface), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the nurb surface request header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (NURBSurface, dataLength, pBuf, req);

    req->type = rationality;
    req->uOrder = uorder;
    req->vOrder = vorder;
    req->numUknots = uorder + numMPoints;
    req->numVknots = vorder + numNPoints;
    req->mPts = numMPoints;
    req->nPts = numNPoints;
    req->numLists = numTrimLoops;

    END_OC_HEADER (NURBSurface, pBuf, req);


    /*
     * Now copy the knot lists and the vertex list.
     */

    OC_LISTOF_FLOAT32 (lenofUKnotList, uknots, fpConvert, fpFormat);
    OC_LISTOF_FLOAT32 (lenofVKnotList, vknots, fpConvert, fpFormat);

    if (rationality == PEXRational)
    {
	OC_LISTOF_COORD4D (numMPoints * numNPoints,
            points.point_4d, fpConvert, fpFormat);
    }
    else
    {
	OC_LISTOF_COORD3D (numMPoints * numNPoints,
	    points.point, fpConvert, fpFormat);
    }


    /* 
     * Now add the list of trim curve (LISTofTrimCurve).  A trim curve list
     * consists of a count followed by a list of trim curves. 
     */

    for (i = 0, ptrimLoop = trimLoops; i < numTrimLoops; i++, ptrimLoop++)
    {
	count = ptrimLoop->count;
	pData = (char *) PEXGetOCAddr (display, SIZEOF (CARD32));
	PUT_CARD32 (count, pData);

	ptrimCurve = ptrimLoop->curves;

	while (count--)
	{
	    /*
	     * Add the trim curve header data.
	     */

	    thisLength = ptrimCurve->order + ptrimCurve->count;
	    ocAddr = (char *) PEXGetOCAddr (display, SIZEOF (pexTrimCurve));

	    BEGIN_TRIMCURVE_HEAD (ocAddr, pTCHead);

	    pTCHead->visibility = (pexSwitch) ptrimCurve->visibility;
	    pTCHead->order = (CARD16) ptrimCurve->order;
	    pTCHead->type = (pexCoordType) ptrimCurve->rationality;
	    pTCHead->approxMethod = (INT16) ptrimCurve->approx_method;
	    pTCHead->numKnots = thisLength;
	    pTCHead->numCoord = ptrimCurve->count;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (ptrimCurve->tolerance,
		    pTCHead->tolerance, fpFormat);
		FP_CONVERT_HTON (ptrimCurve->tmin,
		    pTCHead->tMin, fpFormat);
		FP_CONVERT_HTON (ptrimCurve->tmax,
		    pTCHead->tMax, fpFormat);
	    }
	    else
	    {
		pTCHead->tolerance = (float) ptrimCurve->tolerance;
		pTCHead->tMin = (float) ptrimCurve->tmin;
		pTCHead->tMax = (float) ptrimCurve->tmax;
	    }

	    END_TRIMCURVE_HEAD (ocAddr, pTCHead);


	    /*
	     * Add the trim curve knot list and vertex list.
	     */

	    OC_LISTOF_FLOAT32 (thisLength,
		ptrimCurve->knots.floats, fpConvert, fpFormat);

	    if (ptrimCurve->rationality == PEXRational)
	    {
		OC_LISTOF_COORD3D (ptrimCurve->count,
		    ptrimCurve->control_points.point, fpConvert, fpFormat);
	    }
	    else
	    {
		OC_LISTOF_COORD2D (ptrimCurve->count,
		    ptrimCurve->control_points.point_2d, fpConvert, fpFormat);
	    }

	    ptrimCurve++;
	}
    }

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXCellArray (display, resource_id, req_type, pt1, pt2, pt3, dx, dy, icolors)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord		*pt1;
INPUT PEXCoord		*pt2;
INPUT PEXCoord		*pt3;
INPUT unsigned int	dx;
INPUT unsigned int	dy;
INPUT PEXTableIndex 	*icolors;

{
    register pexCellArray	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (dx * dy * SIZEOF (pexTableIndex));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexCellArray), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the cell array header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (CellArray, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (pt1->x, req->point1_x, fpFormat);
	FP_CONVERT_HTON (pt1->y, req->point1_y, fpFormat);
	FP_CONVERT_HTON (pt1->z, req->point1_z, fpFormat);
	FP_CONVERT_HTON (pt2->x, req->point2_x, fpFormat);
	FP_CONVERT_HTON (pt2->y, req->point2_y, fpFormat);
	FP_CONVERT_HTON (pt2->z, req->point2_z, fpFormat);
	FP_CONVERT_HTON (pt3->x, req->point3_x, fpFormat);
	FP_CONVERT_HTON (pt3->y, req->point3_y, fpFormat);
	FP_CONVERT_HTON (pt3->z, req->point3_z, fpFormat);
    }
    else
    {
	req->point1_x = pt1->x;
	req->point1_y = pt1->y;
	req->point1_z = pt1->z;
	req->point2_x = pt2->x;
	req->point2_y = pt2->y;
	req->point2_z = pt2->z;
	req->point3_x = pt3->x;
	req->point3_y = pt3->y;
	req->point3_z = pt3->z;
    }

    req->dx = dx;
    req->dy = dy;

    END_OC_HEADER (CellArray, pBuf, req);


    /*
     * Copy the color data to the oc.
     */

    OC_LISTOF_CARD16_PAD (dx * dy, icolors);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXCellArray2D (display, resource_id, req_type, pt1, pt2, dx, dy, icolors)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord2D	*pt1;
INPUT PEXCoord2D	*pt2;
INPUT unsigned int	dx;
INPUT unsigned int	dy;
INPUT PEXTableIndex	*icolors;

{
    register pexCellArray2D	*req;
    char			*pBuf;
    int				fpConvert;
    int				fpFormat;
    int				dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = NUMWORDS (dx * dy * SIZEOF (pexTableIndex));

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexCellArray2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the cell array header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (CellArray2D, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (pt1->x, req->point1_x, fpFormat);
	FP_CONVERT_HTON (pt1->y, req->point1_y, fpFormat);
	FP_CONVERT_HTON (pt2->x, req->point2_x, fpFormat);
	FP_CONVERT_HTON (pt2->y, req->point2_y, fpFormat);
    }
    else
    {
	req->point1_x = pt1->x;
	req->point1_y = pt1->y;
	req->point2_x = pt2->x;
	req->point2_y = pt2->y;
    }

    req->dx = dx;
    req->dy = dy;

    END_OC_HEADER (CellArray2D, pBuf, req);


    /*
     * Copy the color data to the oc.
     */

    OC_LISTOF_CARD16_PAD (dx * dy, icolors);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXExtendedCellArray (display, resource_id, req_type,
    pt1, pt2, pt3, dx, dy, colorType, colors)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT PEXCoord		*pt1;
INPUT PEXCoord		*pt2;
INPUT PEXCoord		*pt3;
INPUT unsigned int	dx;
INPUT unsigned int	dy;
INPUT int		colorType;
INPUT PEXArrayOfColor 	colors;

{
    register pexExtendedCellArray 	*req;
    char				*pBuf;
    int					lenofColor;
    int					dataLength;
    int					fpConvert;
    int					fpFormat;


    /*
     * Initialize the OC request.
     */

    lenofColor = GetColorLength (colorType);
    dataLength = dx * dy * lenofColor;

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexExtendedCellArray), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the cell array header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (ExtendedCellArray, dataLength, pBuf, req);

    if (fpConvert)
    {
	FP_CONVERT_HTON (pt1->x, req->point1_x, fpFormat);
	FP_CONVERT_HTON (pt1->y, req->point1_y, fpFormat);
	FP_CONVERT_HTON (pt1->z, req->point1_z, fpFormat);
	FP_CONVERT_HTON (pt2->x, req->point2_x, fpFormat);
	FP_CONVERT_HTON (pt2->y, req->point2_y, fpFormat);
	FP_CONVERT_HTON (pt2->z, req->point2_z, fpFormat);
	FP_CONVERT_HTON (pt3->x, req->point3_x, fpFormat);
	FP_CONVERT_HTON (pt3->y, req->point3_y, fpFormat);
	FP_CONVERT_HTON (pt3->z, req->point3_z, fpFormat);
    }
    else
    {
	req->point1_x = pt1->x;
	req->point1_y = pt1->y;
	req->point1_z = pt1->z;
	req->point2_x = pt2->x;
	req->point2_y = pt2->y;
	req->point2_z = pt2->z;
	req->point3_x = pt3->x;
	req->point3_y = pt3->y;
	req->point3_z = pt3->z;
    }

    req->colorType = colorType;
    req->dx = dx;
    req->dy = dy;

    END_OC_HEADER (ExtendedCellArray, pBuf, req);


    /*
     * Copy the color data to the oc.
     */

    OC_LISTOF_COLOR (dx * dy, lenofColor, colorType,
	colors, fpConvert, fpFormat);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXGDP (display, resource_id, req_type, id, numPoints, points, numBytes, data)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT long		id;
INPUT unsigned int	numPoints;
INPUT PEXCoord		*points;
INPUT unsigned long	numBytes;
INPUT char		*data;

{
    register pexGDP	*req;
    char		*pBuf;
    int			fpConvert;
    int			fpFormat;
    int			dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = numPoints * LENOF (pexCoord3D) + NUMWORDS (numBytes);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexGDP), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the gdp header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (GDP, dataLength, pBuf, req);

    req->gdpId = id; 
    req->numPoints = numPoints; 
    req->numBytes = numBytes;

    END_OC_HEADER (GDP, pBuf, req);


    /*
     * Copy the vertices and GDP data to the oc.
     */

    OC_LISTOF_COORD3D (numPoints, points, fpConvert, fpFormat);

    OC_LISTOF_CARD8_PAD (numBytes, data);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}


void
PEXGDP2D (display, resource_id, req_type,
    id, numPoints, points, numBytes, data)

INPUT Display		*display;
INPUT XID		resource_id;
INPUT PEXOCRequestType	req_type;
INPUT long		id;
INPUT unsigned int	numPoints;
INPUT PEXCoord2D	*points;
INPUT unsigned long	numBytes;
INPUT char		*data;

{
    register pexGDP2D	*req;
    char		*pBuf;
    int			fpConvert;
    int			fpFormat;
    int			dataLength;


    /*
     * Initialize the OC request.
     */

    dataLength = numPoints * LENOF (pexCoord2D) + NUMWORDS (numBytes);

    PEXInitOC (display, resource_id, req_type,
	LENOF (pexGDP2D), dataLength, pBuf);

    if (pBuf == NULL) return;


    /*
     * Store the gdp header data.
     */

    CHECK_FP (fpConvert, fpFormat);

    BEGIN_OC_HEADER (GDP2D, dataLength, pBuf, req);

    req->gdpId = id; 
    req->numPoints = numPoints; 
    req->numBytes = numBytes;

    END_OC_HEADER (GDP2D, pBuf, req);


    /*
     * Copy the vertices and GDP data to the oc.
     */

    OC_LISTOF_COORD2D (numPoints, points, fpConvert, fpFormat);

    OC_LISTOF_CARD8_PAD (numBytes, data);

    PEXFinishOC (display);
    PEXSyncHandle (display);
}
