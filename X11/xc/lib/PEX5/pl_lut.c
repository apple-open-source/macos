/* $Xorg: pl_lut.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */

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
#include "pl_lut.h"


PEXLookupTable
PEXCreateLookupTable (display, drawable, type)

INPUT Display	*display;
INPUT Drawable	drawable;
INPUT int	type;

{
    register pexCreateLookupTableReq	*req;
    char				*pBuf;
    PEXLookupTable			id;


    /*
     * Get a lookup table resource id from X.
     */

    id = XAllocID (display);


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreateLookupTable, pBuf);

    BEGIN_REQUEST_HEADER (CreateLookupTable, pBuf, req);

    PEXStoreReqHead (CreateLookupTable, req);
    req->drawableExample = drawable;
    req->lut = id;
    req->tableType = type;

    END_REQUEST_HEADER (CreateLookupTable, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (id);
}


void
PEXFreeLookupTable (display, lut)

INPUT Display		*display;
INPUT PEXLookupTable	lut;

{
    register pexFreeLookupTableReq     	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreeLookupTable, pBuf);

    BEGIN_REQUEST_HEADER (FreeLookupTable, pBuf, req);

    PEXStoreReqHead (FreeLookupTable, req);
    req->id = lut;

    END_REQUEST_HEADER (FreeLookupTable, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopyLookupTable (display, srcLut, destLut)

INPUT Display		*display;
INPUT PEXLookupTable	srcLut;
INPUT PEXLookupTable	destLut;

{
    register pexCopyLookupTableReq	*req;
    char				*pBuf;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopyLookupTable, pBuf);

    BEGIN_REQUEST_HEADER (CopyLookupTable, pBuf, req);

    PEXStoreReqHead (CopyLookupTable, req);
    req->src = srcLut;
    req->dst = destLut;

    END_REQUEST_HEADER (CopyLookupTable, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXGetTableInfo (display, drawable, type, info)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT int		type;
INPUT PEXTableInfo	*info;

{
    register pexGetTableInfoReq     	*req;
    char				*pBuf;
    pexGetTableInfoReply   		rep;


    /*
     * Lock around critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetTableInfo, pBuf);

    BEGIN_REQUEST_HEADER (GetTableInfo, pBuf, req);

    PEXStoreReqHead (GetTableInfo, req);
    req->drawableExample = drawable;
    req->tableType = type;

    END_REQUEST_HEADER (GetTableInfo, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xTrue) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (0);            /* return an error */
    }

    info->definable_entries = rep.definableEntries;
    info->predefined_count = rep.numPredefined;
    info->predefined_min = rep.predefinedMin;
    info->predefined_max = rep.predefinedMax;


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetPredefinedEntries (display, drawable, type, start, count, entriesReturn)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT int		type;
INPUT unsigned int	start;
INPUT unsigned int	count;
OUTPUT PEXPointer	*entriesReturn;

{
    register pexGetPredefinedEntriesReq		*req;
    char					*pBuf;
    pexGetPredefinedEntriesReply		rep;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetPredefinedEntries, pBuf);

    BEGIN_REQUEST_HEADER (GetPredefinedEntries, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetPredefinedEntries, fpFormat, req);
    req->drawableExample = drawable;
    req->tableType = type;
    req->start = start;
    req->count = count;

    END_REQUEST_HEADER (GetPredefinedEntries, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*entriesReturn = NULL;
	return (0);          /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);


    /*
     * Repack the entries into a buffer allocated for the application.
     */

    *entriesReturn = _PEXRepackLUTEntries (pBuf, (int) rep.numEntries,
	type, fpConvert, fpFormat);

    FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetDefinedIndices (display, lut, numIndicesReturn, indicesReturn)

INPUT Display		*display;
INPUT PEXLookupTable	lut;
OUTPUT unsigned long	*numIndicesReturn;
OUTPUT PEXTableIndex	**indicesReturn;

{
    register pexGetDefinedIndicesReq	*req;
    char				*pBuf;
    pexGetDefinedIndicesReply		rep;
    unsigned				count;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetDefinedIndices, pBuf);

    BEGIN_REQUEST_HEADER (GetDefinedIndices, pBuf, req);

    PEXStoreReqHead (GetDefinedIndices, req);
    req->id = lut;

    END_REQUEST_HEADER (GetDefinedIndices, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*numIndicesReturn = 0;
	*indicesReturn = NULL;
	return (0);              /* return an error */
    }

    *numIndicesReturn = rep.numIndices;


    /*
     * Allocate a buffer for the replies to pass back to the user.
     */

    count = (rep.numIndices & 1) ? (rep.numIndices + 1) : rep.numIndices;
    *indicesReturn = (PEXTableIndex *) Xmalloc (
	count * sizeof (PEXTableIndex));

    XREAD_LISTOF_CARD16 (display, count, *indicesReturn);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


PEXPointer
PEXGetTableEntry (display, lut, index, valueType,
    statusReturn, table_type_return)

INPUT Display		*display;
INPUT PEXLookupTable	lut;
INPUT unsigned int	index;
INPUT int		valueType;
OUTPUT int		*statusReturn;
OUTPUT int		*table_type_return;

{
    register pexGetTableEntryReq	*req;
    char				*pBuf;
    pexGetTableEntryReply		rep;
    int					fpConvert;
    int					fpFormat;
    PEXPointer				entryReturn;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetTableEntry, pBuf);

    BEGIN_REQUEST_HEADER (GetTableEntry, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetTableEntry, fpFormat, req);
    req->valueType = valueType;
    req->lut = lut;
    req->index = index;

    END_REQUEST_HEADER (GetTableEntry, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (NULL);               /* return an error */
    }

    *statusReturn = rep.status;
    *table_type_return = rep.tableType;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);


    /*
     * Repack the entries into a buffer allocated for the application.
     */

    entryReturn = _PEXRepackLUTEntries (pBuf, 1,
	(int) rep.tableType, fpConvert, fpFormat);

    FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (entryReturn);
}


Status
PEXGetTableEntries (display, lut, start, count, valueType,
    table_type_return, entriesReturn)

INPUT Display		*display;
INPUT PEXLookupTable	lut;
INPUT unsigned int	start;
INPUT unsigned int	count;
INPUT int		valueType;
OUTPUT int		*table_type_return;
OUTPUT PEXPointer	*entriesReturn;

{
    register pexGetTableEntriesReq	*req;
    char				*pBuf;
    pexGetTableEntriesReply		rep;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetTableEntries, pBuf);

    BEGIN_REQUEST_HEADER (GetTableEntries, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetTableEntries, fpFormat, req);
    req->valueType = valueType;
    req->lut = lut;
    req->start = start;
    req->count = count;

    END_REQUEST_HEADER (GetTableEntries, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	*entriesReturn = NULL;
	return (0);         /* return an error */
    }

    *table_type_return = rep.tableType;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBuf, rep.length << 2);


    /*
     * Repack the entries into a buffer allocated for the application.
     */

    *entriesReturn = _PEXRepackLUTEntries (pBuf, (int) rep.numEntries,
	(int) rep.tableType, fpConvert, fpFormat);

    FINISH_WITH_SCRATCH (display, pBuf, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


void
PEXSetTableEntries (display, lut, start, count, type, entries)

INPUT Display		*display;
INPUT PEXLookupTable	lut;
INPUT unsigned int	start;
INPUT unsigned int	count;
INPUT int		type;
INPUT PEXPointer	entries;

{
    register pexSetTableEntriesReq	*req;
    char				*pBuf;
    char				*scratch;
    char				*firstEntry;
    int					fpConvert;
    int					fpFormat;
    int					size, i;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetTableEntries, pBuf);

    BEGIN_REQUEST_HEADER (SetTableEntries, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (SetTableEntries, fpFormat, req);
    req->lut = lut;
    req->start = start;
    req->count = count;

    switch (type)
    {
    case PEXLUTLineBundle:
    {
	PEXLineBundleEntry *src = (PEXLineBundleEntry *) entries;
	pexLineBundleEntry *dst;
	
	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXLineBundleEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexLineBundleEntry, scratch, dst);

	    dst->lineType = src->type;
	    dst->polylineInterp = src->interp_method;
	    dst->curveApprox_method = src->curve_approx.method;
	    dst->lineColorType = src->color.type;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->curve_approx.tolerance,
		    dst->curveApprox_tolerance, fpFormat);
		FP_CONVERT_HTON (src->width, dst->lineWidth, fpFormat);
	    }
	    else
	    {
		dst->curveApprox_tolerance = src->curve_approx.tolerance;
		dst->lineWidth = src->width;
	    }

	    END_LUTENTRY_HEADER (pexLineBundleEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTMarkerBundle:
    {
	PEXMarkerBundleEntry *src = (PEXMarkerBundleEntry *) entries;
	pexMarkerBundleEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXMarkerBundleEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexMarkerBundleEntry, scratch, dst);

	    dst->markerType = src->type;
	    dst->markerColorType = src->color.type;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->scale, dst->markerScale, fpFormat);
	    }
	    else
		dst->markerScale = src->scale;
	    
	    END_LUTENTRY_HEADER (pexMarkerBundleEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTTextBundle:
    {
	PEXTextBundleEntry *src = (PEXTextBundleEntry *) entries;
	pexTextBundleEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXTextBundleEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexTextBundleEntry, scratch, dst);

	    dst->textFontIndex = src->font_index;
	    dst->textPrecision = src->precision;
	    dst->textColorType = src->color.type;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->char_expansion,
		    dst->charExpansion, fpFormat);
		FP_CONVERT_HTON (src->char_spacing,
		    dst->charSpacing, fpFormat);
	    }
	    else
	    {
		dst->charExpansion = src->char_expansion;
		dst->charSpacing = src->char_spacing;
	    }

	    END_LUTENTRY_HEADER (pexTextBundleEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTInteriorBundle:
    {
	PEXInteriorBundleEntry *src = (PEXInteriorBundleEntry *) entries;
	pexInteriorBundleEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXInteriorBundleEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexInteriorBundleEntry, scratch, dst);

	    dst->interiorStyle = src->style;
	    dst->interiorStyleIndex = src->style_index;
	    dst->reflectionModel = src->reflection_model;
	    dst->surfaceInterp = src->interp_method;
	    dst->bfInteriorStyle = src->bf_style;
	    dst->bfInteriorStyleIndex =	src->bf_style_index;
	    dst->bfReflectionModel = src->bf_reflection_model;
	    dst->bfSurfaceInterp = src->bf_interp_method;
	    dst->surfaceApprox_method = src->surface_approx.method;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->surface_approx.u_tolerance,
		    dst->surfaceApproxuTolerance, fpFormat);
		FP_CONVERT_HTON (src->surface_approx.v_tolerance,
		    dst->surfaceApproxvTolerance, fpFormat);
	    }
	    else
	    {
		dst->surfaceApproxuTolerance = src->surface_approx.u_tolerance;
		dst->surfaceApproxvTolerance = src->surface_approx.v_tolerance;
	    }

	    END_LUTENTRY_HEADER (pexInteriorBundleEntry, scratch, dst);

	    /* copy surfaceColor */

	    STORE_COLOR_SPEC (src->color, scratch, fpConvert, fpFormat);

	    /* copy reflectionAttr */

	    STORE_REFLECTION_ATTR (src->reflection_attr, scratch,
		fpConvert, fpFormat);

	    /* copy bfSurfaceColor */

	    STORE_COLOR_SPEC (src->bf_color, scratch, fpConvert, fpFormat);

	    /* copy bfReflectionAttr */

	    STORE_REFLECTION_ATTR (src->bf_reflection_attr, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTEdgeBundle:
    {
	PEXEdgeBundleEntry *src = (PEXEdgeBundleEntry *) entries;
	pexEdgeBundleEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXEdgeBundleEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexEdgeBundleEntry, scratch, dst);

	    dst->edges = src->edge_flag;
	    dst->edgeType = src->type;
	    dst->edgeColorType = src->color.type;

	    if (fpConvert)
	    {		
		FP_CONVERT_HTON (src->width, dst->edgeWidth, fpFormat);
	    }
	    else
		dst->edgeWidth = src->width;

	    END_LUTENTRY_HEADER (pexEdgeBundleEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTPattern:
    {
	PEXPatternEntry *src = (PEXPatternEntry *) entries;
	pexPatternEntry *dst;

	size = count * sizeof (PEXPatternEntry);
	for (i = 0; i < count; i++)
	    size += (src[i].col_count *	src[i].row_count * sizeof (PEXColor));

	scratch = firstEntry = (char *) _XAllocScratch (display, size);

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexPatternEntry, scratch, dst);

	    dst->colorType = src->color_type;
	    dst->numx = src->col_count;
	    dst->numy = src->row_count;

	    END_LUTENTRY_HEADER (pexPatternEntry, scratch, dst);

	    STORE_LISTOF_COLOR_VAL (src->col_count * src->row_count,
		src->color_type, src->colors, scratch, fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTTextFont:
    {
	PEXTextFontEntry *src = (PEXTextFontEntry *) entries;
	pexTextFontEntry *dst;

	size = count * sizeof (PEXTextFontEntry);
	for (i = 0; i < count; i++)
	    size += src[i].count * sizeof (PEXFont);

	scratch = firstEntry = (char *) _XAllocScratch (display, size);

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexTextFontEntry, scratch, dst);

	    dst->numFonts = src->count;

	    END_LUTENTRY_HEADER (pexTextFontEntry, scratch, dst);

	    STORE_LISTOF_CARD32 (src->count, src->fonts, scratch);
	}
	break;
    }

    case PEXLUTColor:
    {
	PEXColorEntry 	  *src = (PEXColorEntry *) entries;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXColorEntry));

	STORE_LISTOF_COLOR_SPEC (count, src, scratch, fpConvert, fpFormat);
	break;
    }

    case PEXLUTView:
    {
	PEXViewEntry *src = (PEXViewEntry *) entries;
	pexViewEntry *dst;
	char	     *tPtr;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXViewEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexViewEntry, scratch, dst);

	    dst->clipFlags = src->clip_flags;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->clip_limits.min.x,
		    dst->clipLimits_xmin, fpFormat);
		FP_CONVERT_HTON (src->clip_limits.min.y,
		    dst->clipLimits_ymin, fpFormat);
		FP_CONVERT_HTON (src->clip_limits.min.z,
		    dst->clipLimits_zmin, fpFormat);
		FP_CONVERT_HTON (src->clip_limits.max.x,
		    dst->clipLimits_xmax, fpFormat);
		FP_CONVERT_HTON (src->clip_limits.max.y,
		    dst->clipLimits_ymax, fpFormat);
		FP_CONVERT_HTON (src->clip_limits.max.z,
		    dst->clipLimits_zmax, fpFormat);
	    }
	    else
	    {
		dst->clipLimits_xmin = src->clip_limits.min.x;
		dst->clipLimits_ymin = src->clip_limits.min.y;
		dst->clipLimits_zmin = src->clip_limits.min.z;
		dst->clipLimits_xmax = src->clip_limits.max.x;
		dst->clipLimits_ymax = src->clip_limits.max.y;
		dst->clipLimits_zmax = src->clip_limits.max.z;
	    }

	    tPtr = (char *) dst->orientation;
	    STORE_LISTOF_FLOAT32 (16, src->orientation, tPtr,
		fpConvert, fpFormat);

	    tPtr = (char *) dst->mapping;
	    STORE_LISTOF_FLOAT32 (16, src->mapping, tPtr,
		fpConvert, fpFormat);
	    
	    END_LUTENTRY_HEADER (pexViewEntry, scratch, dst);
	}
	break;
    }

    case PEXLUTLight:
    {
	PEXLightEntry *src = (PEXLightEntry *) entries;
	pexLightEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXLightEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexLightEntry, scratch, dst);

	    dst->lightType = src->type;
	    dst->lightColorType = src->color.type;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->direction.x, dst->direction_x, fpFormat);
		FP_CONVERT_HTON (src->direction.y, dst->direction_y, fpFormat);
		FP_CONVERT_HTON (src->direction.z, dst->direction_z, fpFormat);

		FP_CONVERT_HTON (src->point.x, dst->point_x, fpFormat);
		FP_CONVERT_HTON (src->point.y, dst->point_y, fpFormat);
		FP_CONVERT_HTON (src->point.z, dst->point_z, fpFormat);

		FP_CONVERT_HTON (src->concentration,
		    dst->concentration, fpFormat);
		FP_CONVERT_HTON (src->spread_angle,
		    dst->spreadAngle, fpFormat);
		FP_CONVERT_HTON (src->attenuation1,
		    dst->attenuation1, fpFormat);
		FP_CONVERT_HTON (src->attenuation2,
		    dst->attenuation2, fpFormat);
	    }
	    else
	    {
		dst->direction_x = src->direction.x;
		dst->direction_y = src->direction.y;
		dst->direction_z = src->direction.z;

		dst->point_x = src->point.x;
		dst->point_y = src->point.y;
		dst->point_z = src->point.z;

		dst->concentration = src->concentration;
		dst->spreadAngle = src->spread_angle;
		dst->attenuation1 = src->attenuation1;
		dst->attenuation2 = src->attenuation2;
	    }

	    END_LUTENTRY_HEADER (pexLightEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
	        fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTDepthCue:
    {
	PEXDepthCueEntry *src = (PEXDepthCueEntry *) entries;
	pexDepthCueEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXDepthCueEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexDepthCueEntry, scratch, dst);

	    dst->mode = src->mode;
	    dst->depthCueColorType = src->color.type;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->front_plane, dst->frontPlane, fpFormat);
		FP_CONVERT_HTON (src->back_plane, dst->backPlane, fpFormat);

		FP_CONVERT_HTON (src->front_scaling,
		    dst->frontScaling, fpFormat);
		FP_CONVERT_HTON (src->back_scaling,
		    dst->backScaling, fpFormat);
	    }
	    else
	    {
		dst->frontPlane = src->front_plane;
		dst->backPlane = src->back_plane;

		dst->frontScaling = src->front_scaling;
		dst->backScaling = src->back_scaling;
	    }
	     
	    END_LUTENTRY_HEADER (pexDepthCueEntry, scratch, dst);

	    STORE_COLOR_VAL (src->color.type, src->color.value, scratch,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTColorApprox:
    {
	PEXColorApproxEntry *src = (PEXColorApproxEntry *) entries;
	pexColorApproxEntry *dst;

	scratch = firstEntry = (char *) _XAllocScratch (display,
	    count * sizeof (PEXColorApproxEntry));

	for (i = 0; i < count; i++, src++)
	{
	    BEGIN_LUTENTRY_HEADER (pexColorApproxEntry, scratch, dst);

	    dst->approxType = src->type;
	    dst->approxModel = src->model;
	    dst->max1 = src->max1;
	    dst->max2 = src->max2;
	    dst->max3 = src->max3;
	    dst->dither = src->dither;
	    dst->mult1 = src->mult1;
	    dst->mult2 = src->mult2;
	    dst->mult3 = src->mult3;
	    dst->basePixel = src->base_pixel;

	    if (fpConvert)
	    {
		FP_CONVERT_HTON (src->weight1, dst->weight1, fpFormat);
		FP_CONVERT_HTON (src->weight2, dst->weight2, fpFormat);
		FP_CONVERT_HTON (src->weight3, dst->weight3, fpFormat);
	    }
	    else
	    {
		dst->weight1 = src->weight1;
		dst->weight2 = src->weight2;
		dst->weight3 = src->weight3;
	    }

	    END_LUTENTRY_HEADER (pexColorApproxEntry, scratch, dst);
	}
	break;
    }
    }


    /*
     * Update the request length.
     */

    size = scratch - firstEntry;
    req->length += NUMWORDS (size);

    END_REQUEST_HEADER (SetTableEntries, pBuf, req);


    /*
     * Add the table entry data to the end of the X request.
     */

    Data (display, firstEntry, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXDeleteTableEntries (display, lut, start, count)

INPUT Display		*display;
INPUT PEXLookupTable	lut;
INPUT unsigned int	start;
INPUT unsigned int	count;

{
    register pexDeleteTableEntriesReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (DeleteTableEntries, pBuf);

    BEGIN_REQUEST_HEADER (DeleteTableEntries, pBuf, req);

    PEXStoreReqHead (DeleteTableEntries, req);
    req->lut = lut;
    req->start = start;
    req->count = count;

    END_REQUEST_HEADER (DeleteTableEntries, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}



/*
 * This routine repacks the lut entries returned by PEXGetTableEntry,
 * PEXGetTableEntries, and PEXGetPredefinedEntries.  This is mostly to
 * change the colors from fixed size to the PEXColor union format,
 * as well as to do floating point conversion.
*/

static PEXPointer
_PEXRepackLUTEntries (pBuf, numEntries, tableType, fpConvert, fpFormat)

INPUT char	*pBuf;
INPUT int	numEntries;
INPUT int	tableType;
INPUT int	fpConvert;
INPUT int	fpFormat;

{
    PEXPointer	lutBuf;
    int		i;

    switch (tableType)
    {
    case PEXLUTLineBundle:
    {
	pexLineBundleEntry *src;
	PEXLineBundleEntry *dst;

	GetLUTEntryBuffer (numEntries, PEXLineBundleEntry, lutBuf);
	dst = (PEXLineBundleEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexLineBundleEntry, pBuf, src);
	    pBuf += SIZEOF (pexLineBundleEntry);

	    dst->type = src->lineType;
	    dst->interp_method = src->polylineInterp;
	    dst->curve_approx.method = src->curveApprox_method;
	    dst->color.type = src->lineColorType;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->curveApprox_tolerance,
		    dst->curve_approx.tolerance, fpFormat);
		FP_CONVERT_NTOH (src->lineWidth, dst->width, fpFormat);
	    }
	    else
	    {
		dst->curve_approx.tolerance = src->curveApprox_tolerance;
		dst->width = src->lineWidth;
	    }

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTMarkerBundle:
    {
	pexMarkerBundleEntry *src;
	PEXMarkerBundleEntry *dst;

	GetLUTEntryBuffer (numEntries, PEXMarkerBundleEntry, lutBuf);
	dst = (PEXMarkerBundleEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexMarkerBundleEntry, pBuf, src);
	    pBuf += SIZEOF (pexMarkerBundleEntry);

	    dst->type = src->markerType;
	    dst->color.type = src->markerColorType;
	    
	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->markerScale, dst->scale, fpFormat);
	    }
	    else
		dst->scale = src->markerScale;

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTTextBundle:
    {
	pexTextBundleEntry *src;
	PEXTextBundleEntry *dst;
	    
	GetLUTEntryBuffer (numEntries, PEXTextBundleEntry, lutBuf);
	dst = (PEXTextBundleEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexTextBundleEntry, pBuf, src);
	    pBuf += SIZEOF (pexTextBundleEntry);

	    dst->font_index = src->textFontIndex;
	    dst->precision = src->textPrecision;
	    dst->color.type = src->textColorType;
	    
	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->charExpansion,
		    dst->char_expansion, fpFormat);
		FP_CONVERT_NTOH (src->charSpacing,
		    dst->char_spacing, fpFormat);
	    }
	    else
	    {
		dst->char_expansion = src->charExpansion;
		dst->char_spacing = src->charSpacing;
	    }

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTInteriorBundle:
    {
	pexInteriorBundleEntry *src;
	PEXInteriorBundleEntry *dst;

	GetLUTEntryBuffer (numEntries, PEXInteriorBundleEntry, lutBuf);
	dst = (PEXInteriorBundleEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexInteriorBundleEntry, pBuf, src);
	    pBuf += SIZEOF (pexInteriorBundleEntry);

	    dst->style = src->interiorStyle;
	    dst->style_index = src->interiorStyleIndex;
	    dst->reflection_model = src->reflectionModel;
	    dst->interp_method = src->surfaceInterp;
	    dst->bf_style = src->bfInteriorStyle;
	    dst->bf_style_index = src->bfInteriorStyleIndex;
	    dst->bf_reflection_model = src->bfReflectionModel;
	    dst->bf_interp_method = src->bfSurfaceInterp;
	    dst->surface_approx.method = src->surfaceApprox_method;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->surfaceApproxuTolerance,
		    dst->surface_approx.u_tolerance, fpFormat);
		FP_CONVERT_NTOH (src->surfaceApproxvTolerance,
		    dst->surface_approx.v_tolerance, fpFormat);
	    }
	    else
	    {
		dst->surface_approx.u_tolerance = src->surfaceApproxuTolerance;
		dst->surface_approx.v_tolerance = src->surfaceApproxvTolerance;
	    }

	    /* copy surfaceColor */

	    EXTRACT_COLOR_SPEC (pBuf, dst->color, fpConvert, fpFormat);

	    /* copy reflectionAttr */

	    EXTRACT_REFLECTION_ATTR (pBuf, dst->reflection_attr,
		fpConvert, fpFormat);

	    /* copy bfSurfaceColor */

	    EXTRACT_COLOR_SPEC (pBuf, dst->bf_color, fpConvert, fpFormat);

	    /* copy bfReflectionAttr */

	    EXTRACT_REFLECTION_ATTR (pBuf, dst->bf_reflection_attr,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTEdgeBundle:
    {
	pexEdgeBundleEntry *src;
	PEXEdgeBundleEntry *dst;
         
	GetLUTEntryBuffer (numEntries, PEXEdgeBundleEntry, lutBuf);
	dst = (PEXEdgeBundleEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexEdgeBundleEntry, pBuf, src);
	    pBuf += SIZEOF (pexEdgeBundleEntry);

	    dst->edge_flag = src->edges;
	    dst->type = src->edgeType;
	    dst->color.type = src->edgeColorType;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->edgeWidth, dst->width, fpFormat);
	    }
	    else
		dst->width = src->edgeWidth;

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTPattern:
    {
	pexPatternEntry *src;
	PEXPatternEntry *dst;
    
	GetLUTEntryBuffer (numEntries, PEXPatternEntry, lutBuf);
	dst = (PEXPatternEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexPatternEntry, pBuf, src);
	    pBuf += SIZEOF (pexPatternEntry);

	    dst->color_type = src->colorType;
	    dst->col_count = src->numx;
	    dst->row_count = src->numy;

	    dst->colors.indexed = (PEXColorIndexed *) Xmalloc (
		(unsigned) (GetClientColorSize (dst->color_type) *
		dst->row_count * dst->col_count));

	    EXTRACT_LISTOF_COLOR_VAL (dst->row_count * dst->col_count,
		pBuf, dst->color_type, dst->colors, fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTTextFont:
    {
	pexTextFontEntry *src;
	PEXTextFontEntry *dst;

	GetLUTEntryBuffer (numEntries, PEXTextFontEntry, lutBuf);
	dst = (PEXTextFontEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexTextFontEntry, pBuf, src);
	    pBuf += SIZEOF (pexTextFontEntry);

	    dst->count = src->numFonts;

	    dst->fonts = (PEXFont *)
		Xmalloc ((unsigned) (dst->count * sizeof (PEXFont)));
	    
	    EXTRACT_LISTOF_CARD32 (dst->count, pBuf, dst->fonts);
	}
	break;
    }

    case PEXLUTColor:
    {
	PEXColorEntry     *dst;
    
	GetLUTEntryBuffer (numEntries, PEXColorEntry, lutBuf);
	dst = (PEXColorEntry *) lutBuf;

	EXTRACT_LISTOF_COLOR_SPEC (numEntries, pBuf, dst, fpConvert, fpFormat);
	break;
    }

    case PEXLUTView:
    {
	pexViewEntry *src;
	PEXViewEntry *dst;
	char	     *tPtr;
         
	GetLUTEntryBuffer (numEntries, PEXViewEntry, lutBuf);
	dst = (PEXViewEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexViewEntry, pBuf, src);
	    pBuf += SIZEOF (pexViewEntry);

	    dst->clip_flags = src->clipFlags;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->clipLimits_xmin,
		    dst->clip_limits.min.x, fpFormat);
		FP_CONVERT_NTOH (src->clipLimits_ymin,
		    dst->clip_limits.min.y, fpFormat);
		FP_CONVERT_NTOH (src->clipLimits_zmin,
		    dst->clip_limits.min.z, fpFormat);
		FP_CONVERT_NTOH (src->clipLimits_xmax,
		    dst->clip_limits.max.x, fpFormat);
		FP_CONVERT_NTOH (src->clipLimits_ymax,
		    dst->clip_limits.max.y, fpFormat);
		FP_CONVERT_NTOH (src->clipLimits_zmax,
		    dst->clip_limits.max.z, fpFormat);
	    }
	    else
	    {
		dst->clip_limits.min.x = src->clipLimits_xmin;
		dst->clip_limits.min.y = src->clipLimits_ymin;
		dst->clip_limits.min.z = src->clipLimits_zmin;
		dst->clip_limits.max.x = src->clipLimits_xmax;
		dst->clip_limits.max.y = src->clipLimits_ymax;
		dst->clip_limits.max.z = src->clipLimits_zmax;
	    }

	    tPtr = (char *) src->orientation;
	    EXTRACT_LISTOF_FLOAT32 (16, tPtr, dst->orientation,
		fpConvert, fpFormat);

	    tPtr = (char *) src->mapping;
	    EXTRACT_LISTOF_FLOAT32 (16, tPtr, dst->mapping,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTLight:
    {
	pexLightEntry *src;
	PEXLightEntry *dst;
	
	GetLUTEntryBuffer (numEntries, PEXLightEntry, lutBuf);
	dst = (PEXLightEntry *) lutBuf;

	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexLightEntry, pBuf, src);
	    pBuf += SIZEOF (pexLightEntry);

	    dst->type = src->lightType;
	    dst->color.type = src->lightColorType;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->direction_x, dst->direction.x, fpFormat);
		FP_CONVERT_NTOH (src->direction_y, dst->direction.y, fpFormat);
		FP_CONVERT_NTOH (src->direction_z, dst->direction.z, fpFormat);

		FP_CONVERT_NTOH (src->point_x, dst->point.x, fpFormat);
		FP_CONVERT_NTOH (src->point_y, dst->point.y, fpFormat);
		FP_CONVERT_NTOH (src->point_z, dst->point.z, fpFormat);

		FP_CONVERT_NTOH (src->concentration,
		    dst->concentration, fpFormat);
		FP_CONVERT_NTOH (src->spreadAngle,
		    dst->spread_angle, fpFormat);
		FP_CONVERT_NTOH (src->attenuation1,
		    dst->attenuation1, fpFormat);
		FP_CONVERT_NTOH (src->attenuation2,
		    dst->attenuation2, fpFormat);
	    }
	    else
	    {
		dst->direction.x = src->direction_x;
		dst->direction.y = src->direction_y;
		dst->direction.z = src->direction_z;

		dst->point.x = src->point_x;
		dst->point.y = src->point_y;
		dst->point.z = src->point_z;

		dst->concentration = src->concentration;
		dst->spread_angle = src->spreadAngle;
		dst->attenuation1 = src->attenuation1;
		dst->attenuation2 = src->attenuation2;
	    }

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTDepthCue:
    {
	pexDepthCueEntry *src;
	PEXDepthCueEntry *dst;
    
	GetLUTEntryBuffer (numEntries, PEXDepthCueEntry, lutBuf);
	dst = (PEXDepthCueEntry *) lutBuf;
	
	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexDepthCueEntry, pBuf, src);
	    pBuf += SIZEOF (pexDepthCueEntry);

	    dst->mode = src->mode;
	    dst->color.type = src->depthCueColorType;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->frontPlane, dst->front_plane, fpFormat);
		FP_CONVERT_NTOH (src->backPlane, dst->back_plane, fpFormat);

		FP_CONVERT_NTOH (src->frontScaling,
		    dst->front_scaling, fpFormat);
		FP_CONVERT_NTOH (src->backScaling,
		    dst->back_scaling, fpFormat);
	    }
	    else
	    {
		dst->front_plane = src->frontPlane;
		dst->back_plane = src->backPlane;

		dst->front_scaling = src->frontScaling;
		dst->back_scaling = src->backScaling;
	    }

	    EXTRACT_COLOR_VAL (pBuf, dst->color.type, dst->color.value,
		fpConvert, fpFormat);
	}
	break;
    }

    case PEXLUTColorApprox:
    {
	pexColorApproxEntry *src;
	PEXColorApproxEntry *dst;
    
	GetLUTEntryBuffer (numEntries, PEXColorApproxEntry, lutBuf);
	dst = (PEXColorApproxEntry *) lutBuf;
	
	for (i = 0; i < numEntries; i++, dst++)
	{
	    GET_STRUCT_PTR (pexColorApproxEntry, pBuf, src);
	    pBuf += SIZEOF (pexColorApproxEntry);

	    dst->type = src->approxType;
	    dst->model = src->approxModel;
	    dst->max1 = src->max1;
	    dst->max2 = src->max2;
	    dst->max3 = src->max3;
	    dst->dither = src->dither;
	    dst->mult1 = src->mult1;
	    dst->mult2 = src->mult2;
	    dst->mult3 = src->mult3;
	    dst->base_pixel = src->basePixel;

	    if (fpConvert)
	    {
		FP_CONVERT_NTOH (src->weight1, dst->weight1, fpFormat);
		FP_CONVERT_NTOH (src->weight2, dst->weight2, fpFormat);
		FP_CONVERT_NTOH (src->weight3, dst->weight3, fpFormat);
	    }
	    else
	    {
		dst->weight1 = src->weight1;
		dst->weight2 = src->weight2;
		dst->weight3 = src->weight3;
	    }
	}
	break;
    }
    }

    return (lutBuf);
}
