/* $Xorg: pl_pc.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */

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

static void _PEXGeneratePCList();


PEXPipelineContext
PEXCreatePipelineContext (display, valueMask, values)

INPUT Display		*display;
INPUT unsigned long	*valueMask;
INPUT PEXPCAttributes	*values;

{
    register pexCreatePipelineContextReq	*req;
    char					*pBuf;
    PEXPipelineContext				pc;
    int						size = 0;
    char					*pList;
    int						fpConvert;
    int						fpFormat;


    /*
     * Get a pipeline context resource id from X.
     */

    pc = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreatePipelineContext, pBuf);

    BEGIN_REQUEST_HEADER (CreatePipelineContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (CreatePipelineContext, fpFormat, req);
    req->pc = pc;
    req->itemMask0 = valueMask[0];
    req->itemMask1 = valueMask[1];
    req->itemMask2 = valueMask[2];

    if (valueMask[0] != 0 || valueMask[1] != 0 || valueMask[2] != 0)
    {
	_PEXGeneratePCList (display, fpConvert, fpFormat,
	    valueMask, values, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (CreatePipelineContext, pBuf, req);


    /*
     * Send the list of values.
     */

    if (size > 0)
	Data (display, pList, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pc);
}


void
PEXFreePipelineContext (display, pc)

INPUT Display			*display;
INPUT PEXPipelineContext	pc;

{
    register pexFreePipelineContextReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreePipelineContext, pBuf);

    BEGIN_REQUEST_HEADER (FreePipelineContext, pBuf, req);

    PEXStoreReqHead (FreePipelineContext, req);
    req->id = pc;

    END_REQUEST_HEADER (FreePipelineContext, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopyPipelineContext (display, valueMask, srcPc, destPc)

INPUT Display			*display;
INPUT unsigned long		*valueMask;
INPUT PEXPipelineContext	srcPc;
INPUT PEXPipelineContext	destPc;

{
    register pexCopyPipelineContextReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopyPipelineContext, pBuf);

    BEGIN_REQUEST_HEADER (CopyPipelineContext, pBuf, req);

    PEXStoreReqHead (CopyPipelineContext, req);
    req->src = srcPc;
    req->dst = destPc;
    req->itemMask0 = valueMask[0];
    req->itemMask1 = valueMask[1];
    req->itemMask2 = valueMask[2];

    END_REQUEST_HEADER (CopyPipelineContext, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXPCAttributes *
PEXGetPipelineContext (display, pc, valueMask)

INPUT Display			*display;
INPUT PEXPipelineContext	pc;
INPUT unsigned long		*valueMask;

{
    register pexGetPipelineContextReq	*req;
    char				*pBuf, *pBufSave;
    pexGetPipelineContextReply		rep;
    PEXPCAttributes			*pAttr;
    Bool				bitSet;
    unsigned int			size;
    CARD32				count;
    int					n;
    INT16				pscType;
    INT16				paramSize;
    int					fpConvert;
    int					fpFormat;



    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetPipelineContext, pBuf);

    BEGIN_REQUEST_HEADER (GetPipelineContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetPipelineContext, fpFormat, req);
    req->pc = pc;
    req->itemMask0 = valueMask[0];
    req->itemMask1 = valueMask[1];
    req->itemMask2 = valueMask[2];

    END_REQUEST_HEADER (GetPipelineContext, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (NULL);         /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    pAttr = (PEXPCAttributes *) Xmalloc (sizeof (PEXPCAttributes));

    pAttr->model_clip_volume.count = 0;
    pAttr->model_clip_volume.half_spaces = NULL;
    pAttr->light_state.count = 0;
    pAttr->light_state.indices = NULL;
    pAttr->para_surf_char.type = 0;


    /*
     * Fill in the PC attributes.
     */

    for (n = 0; n < (PEXPCMaxShift + 1); n++)
    {
	bitSet = valueMask[n >> 5] & (1L << (n & 0x1f));

	if (bitSet != 0)
        {
            switch (n)
	    {
            case PEXPCMarkerType:

		EXTRACT_LOV_INT16 (pBuf, pAttr->marker_type);
		break;

            case PEXPCMarkerScale:

		EXTRACT_FLOAT32 (pBuf, pAttr->marker_scale,
		    fpConvert, fpFormat);
		break;

            case PEXPCMarkerColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->marker_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCMarkerBundleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->marker_bundle_index);
		break;

            case PEXPCTextFont:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->text_font);
		break;

            case PEXPCTextPrecision:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->text_precision);
		break;

            case PEXPCCharExpansion:

	  	EXTRACT_FLOAT32 (pBuf, pAttr->char_expansion,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharSpacing:

		EXTRACT_FLOAT32 (pBuf, pAttr->char_spacing,
		    fpConvert, fpFormat);
		break;

            case PEXPCTextColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->text_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharHeight:

		EXTRACT_FLOAT32 (pBuf, pAttr->char_height,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharUpVector:

		EXTRACT_VECTOR2D (pBuf, pAttr->char_up_vector,
		    fpConvert, fpFormat);
		break;

            case PEXPCTextPath:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->text_path);
		break;

            case PEXPCTextAlignment:

		EXTRACT_TEXTALIGN (pBuf, pAttr->text_alignment);
		break;

            case PEXPCATextHeight:

		EXTRACT_FLOAT32 (pBuf, pAttr->atext_height,
		    fpConvert, fpFormat);
		break;

            case PEXPCATextUpVector:

		EXTRACT_VECTOR2D (pBuf, pAttr->atext_up_vector,
		    fpConvert, fpFormat);
		break;

            case PEXPCATextPath:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->atext_path);
		break;

            case PEXPCATextAlignment:

		EXTRACT_TEXTALIGN (pBuf, pAttr->atext_alignment);
		break;

            case PEXPCATextStyle:

		EXTRACT_LOV_INT16 (pBuf, pAttr->atext_style);
		break;

            case PEXPCTextBundleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->text_bundle_index);
		break;

            case PEXPCLineType:

		EXTRACT_LOV_INT16 (pBuf, pAttr->line_type);
		break;

            case PEXPCLineWidth:

		EXTRACT_FLOAT32 (pBuf, pAttr->line_width,
		    fpConvert, fpFormat);
		break;

            case PEXPCLineColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->line_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCCurveApprox:

		EXTRACT_LOV_INT16 (pBuf, pAttr->curve_approx.method);
		EXTRACT_FLOAT32 (pBuf, pAttr->curve_approx.tolerance,
		    fpConvert, fpFormat);
		break;

            case PEXPCPolylineInterp:

		EXTRACT_LOV_INT16 (pBuf, pAttr->polyline_interp);
		break;

            case PEXPCLineBundleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->line_bundle_index);
		break;

            case PEXPCInteriorStyle:

		EXTRACT_LOV_INT16 (pBuf, pAttr->interior_style);
		break;

            case PEXPCInteriorStyleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->interior_style_index);
		break;

            case PEXPCSurfaceColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->surface_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCReflectionAttr:

		EXTRACT_REFLECTION_ATTR (pBuf, pAttr->reflection_attr,
		    fpConvert, fpFormat);
		break;

            case PEXPCReflectionModel:

		EXTRACT_LOV_INT16 (pBuf, pAttr->reflection_model);
		break;

            case PEXPCSurfaceInterp:

		EXTRACT_LOV_INT16 (pBuf, pAttr->surface_interp);
		break;

            case PEXPCBFInteriorStyle:

		EXTRACT_LOV_INT16 (pBuf, pAttr->bf_interior_style);
		break;

            case PEXPCBFInteriorStyleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->bf_interior_style_index);
		break;

            case PEXPCBFSurfaceColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->bf_surface_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCBFReflectionAttr:

		EXTRACT_REFLECTION_ATTR (pBuf, pAttr->bf_reflection_attr,
		    fpConvert, fpFormat);
		break;

            case PEXPCBFReflectionModel:

		EXTRACT_LOV_INT16 (pBuf, pAttr->bf_reflection_model);
		break;

            case PEXPCBFSurfaceInterp:

		EXTRACT_LOV_INT16 (pBuf, pAttr->bf_surface_interp);
		break;

            case PEXPCSurfaceApprox:

		EXTRACT_LOV_INT16 (pBuf, pAttr->surface_approx.method);
		EXTRACT_FLOAT32 (pBuf, pAttr->surface_approx.u_tolerance,
		    fpConvert, fpFormat);
		EXTRACT_FLOAT32 (pBuf, pAttr->surface_approx.v_tolerance,
		    fpConvert, fpFormat);
		break;

            case PEXPCCullingMode:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->culling_mode);
		break;

            case PEXPCDistinguishFlag:

		EXTRACT_LOV_CARD8 (pBuf, pAttr->distinguish);
		break;

            case PEXPCPatternSize:

		EXTRACT_COORD2D (pBuf, pAttr->pattern_size,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefPoint:

		EXTRACT_COORD3D (pBuf, pAttr->pattern_ref_point,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefVec1:

		EXTRACT_VECTOR3D (pBuf, pAttr->pattern_ref_vec1,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefVec2:

		EXTRACT_VECTOR3D (pBuf, pAttr->pattern_ref_vec2,
		    fpConvert, fpFormat);
		break;

            case PEXPCInteriorBundleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->interior_bundle_index);
		break;

            case PEXPCSurfaceEdgeFlag:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->surface_edges);
		break;

            case PEXPCSurfaceEdgeType:

		EXTRACT_LOV_INT16 (pBuf, pAttr->surface_edge_type);
		break;

            case PEXPCSurfaceEdgeWidth:

		EXTRACT_FLOAT32 (pBuf, pAttr->surface_edge_width,
		    fpConvert, fpFormat);
		break;

            case PEXPCSurfaceEdgeColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->surface_edge_color,
		    fpConvert, fpFormat);
		break;

            case PEXPCEdgeBundleIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->edge_bundle_index);
		break;

            case PEXPCLocalTransform:

		EXTRACT_LISTOF_FLOAT32 (16, pBuf, pAttr->local_transform,
		    fpConvert, fpFormat);
		break;

            case PEXPCGlobalTransform:

		EXTRACT_LISTOF_FLOAT32 (16, pBuf, pAttr->global_transform,
		    fpConvert, fpFormat);
		break;

            case PEXPCModelClip:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->model_clip);
		break;

            case PEXPCModelClipVolume:

		EXTRACT_CARD32 (pBuf, count);
		pAttr->model_clip_volume.count = count;

		size = count * sizeof (PEXHalfSpace);
		pAttr->model_clip_volume.half_spaces =
		    (PEXHalfSpace *) Xmalloc (size);

		EXTRACT_LISTOF_HALFSPACE3D (count, pBuf,
		    pAttr->model_clip_volume.half_spaces,
		    fpConvert, fpFormat);
		break;

            case PEXPCViewIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->view_index);
		break;

            case PEXPCLightState:

		EXTRACT_CARD32 (pBuf, count);
		pAttr->light_state.count = count;

		size = count * sizeof (PEXTableIndex);
		pAttr->light_state.indices =
		    (PEXTableIndex *) Xmalloc (size);

		EXTRACT_LISTOF_CARD16 (count, pBuf,
		    pAttr->light_state.indices);
		if (count & 1)
		    pBuf += SIZEOF (CARD16);
		break;

            case PEXPCDepthCueIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->depth_cue_index);
		break;

            case PEXPCPickID:

		EXTRACT_CARD32 (pBuf, pAttr->pick_id);
		break;

            case PEXPCHLHSRIdentifier:

		EXTRACT_CARD32 (pBuf, pAttr->hlhsr_id);
		break;

            case PEXPCNameSet:

		EXTRACT_CARD32 (pBuf, pAttr->name_set);
		break;

            case PEXPCASFValues:

		EXTRACT_CARD32 (pBuf, pAttr->asf_enables);
		EXTRACT_CARD32 (pBuf, pAttr->asf_values);
		break;

	    case PEXPCColorApproxIndex:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->color_approx_index);
		break;

	    case PEXPCRenderingColorModel:

		EXTRACT_LOV_INT16 (pBuf, pAttr->rendering_color_model);
		break;

	    case PEXPCParaSurfCharacteristics:

		EXTRACT_INT16 (pBuf, pscType);
		EXTRACT_INT16 (pBuf, paramSize);

		pAttr->para_surf_char.type = pscType;

		if (pscType == PEXPSCIsoCurves)
		{
		    EXTRACT_PSC_ISOCURVES (pBuf,
			pAttr->para_surf_char.psc.iso_curves);
		}
		else if (pscType == PEXPSCMCLevelCurves ||
		    pscType == PEXPSCWCLevelCurves)
		{
		    count = (paramSize - SIZEOF (pexPSC_LevelCurves)) /
			SIZEOF (float);

		    EXTRACT_PSC_LEVELCURVES (pBuf,
			pAttr->para_surf_char.psc.level_curves,
		        fpConvert, fpFormat);

		    pAttr->para_surf_char.psc.level_curves.parameters = 
		       (float *) Xmalloc ((unsigned) (count * sizeof (float)));

		    EXTRACT_LISTOF_FLOAT32 (count, pBuf,
			pAttr->para_surf_char.psc.level_curves.parameters,
			fpConvert, fpFormat);
		}
		break;
	    }
	}
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pAttr);
}


void
PEXChangePipelineContext (display, pc, valueMask, pcAttributes)

INPUT Display			*display;
INPUT PEXPipelineContext	pc;
INPUT unsigned long		*valueMask;
INPUT PEXPCAttributes		*pcAttributes;

{
    register pexChangePipelineContextReq	*req;
    char					*pBuf;
    int						size = 0;
    char					*pList;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (ChangePipelineContext, pBuf);

    BEGIN_REQUEST_HEADER (ChangePipelineContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (ChangePipelineContext, fpFormat, req);
    req->pc = pc;
    req->itemMask0 = valueMask[0];
    req->itemMask1 = valueMask[1];
    req->itemMask2 = valueMask[2];

    if (valueMask[0] != 0 || valueMask[1] != 0 || valueMask[2] != 0)
    {
	_PEXGeneratePCList (display, fpConvert, fpFormat,
	    valueMask, pcAttributes, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (ChangePipelineContext, pBuf, req);


    /*
     * Send the list of values.
     */

    if (size > 0)
	Data (display, pList, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}



/*
 * Routine to write a packed list of pc attributes into the transport buf.
 */

static void
_PEXGeneratePCList (display, fpConvert, fpFormat,
    valueMask, values, sizeRet, listRet)

INPUT Display		*display;
INPUT int		fpConvert;
INPUT int		fpFormat;
INPUT unsigned long    	*valueMask;
INPUT PEXPCAttributes	*values;
OUTPUT int		*sizeRet;
OUTPUT char		**listRet;

{
    register char	*pBuf;
    unsigned long       size;
    CARD32		count;
    Bool		bitSet;
    int			pscType, n;


    /*
     * size is the maximum size we might need to store the PC list.  Just
     * use 2*sizeof(PEXPCAttributes) to account for padding between shorts.
     */

    size = 2 * sizeof (PEXPCAttributes);

    if (valueMask[1] &
	1L << (PEXPCModelClipVolume - PEXPCBFInteriorStyleIndex))
	size += values->model_clip_volume.count * SIZEOF (pexHalfSpace);

    if (valueMask[1] & 1L << (PEXPCLightState - PEXPCBFInteriorStyleIndex))
	size += PADDED_BYTES (values->light_state.count *
	    SIZEOF (pexTableIndex));

    *listRet = pBuf = (char *) _XAllocScratch (display, size);

    for (n = 0; n < (PEXPCMaxShift + 1); n++)
    {
	bitSet = valueMask[n >> 5] & (1L << (n & 0x1f));
	if (bitSet != 0)
        {
            switch (n)
	    {
            case PEXPCMarkerType:

		STORE_CARD32 (values->marker_type, pBuf);
		break;

            case PEXPCMarkerScale:

		STORE_FLOAT32 (values->marker_scale, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCMarkerColor:

		STORE_COLOR_SPEC (values->marker_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCMarkerBundleIndex:

		STORE_CARD32 (values->marker_bundle_index, pBuf);
		break;

            case PEXPCTextFont:

		STORE_CARD32 (values->text_font, pBuf);
		break;

            case PEXPCTextPrecision:

		STORE_CARD32 (values->text_precision, pBuf);
		break;

            case PEXPCCharExpansion:

		STORE_FLOAT32 (values->char_expansion, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharSpacing:

		STORE_FLOAT32 (values->char_spacing, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCTextColor:

		STORE_COLOR_SPEC (values->text_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharHeight:

		STORE_FLOAT32 (values->char_height, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCCharUpVector:

		STORE_VECTOR2D (values->char_up_vector, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCTextPath:

		STORE_CARD32 (values->text_path, pBuf);
		break;

            case PEXPCTextAlignment:

		STORE_TEXTALIGN (values->text_alignment, pBuf);
		break;

            case PEXPCATextHeight:

		STORE_FLOAT32 (values->atext_height, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCATextUpVector:

		STORE_VECTOR2D (values->atext_up_vector, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCATextPath:

		STORE_CARD32 (values->atext_path, pBuf);
		break;

            case PEXPCATextAlignment:

		STORE_TEXTALIGN (values->atext_alignment, pBuf);
		break;

            case PEXPCATextStyle:

		STORE_CARD32 (values->atext_style, pBuf);
		break;

            case PEXPCTextBundleIndex:

		STORE_CARD32 (values->text_bundle_index, pBuf);
		break;

            case PEXPCLineType:

		STORE_CARD32 (values->line_type, pBuf);
		break;

            case PEXPCLineWidth:

		STORE_FLOAT32 (values->line_width, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCLineColor:

		STORE_COLOR_SPEC (values->line_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCCurveApprox:

		STORE_CARD32 (values->curve_approx.method, pBuf);
		STORE_FLOAT32 (values->curve_approx.tolerance, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCPolylineInterp:

		STORE_CARD32 (values->polyline_interp, pBuf);
		break;

            case PEXPCLineBundleIndex:

		STORE_CARD32 (values->line_bundle_index, pBuf);
		break;

            case PEXPCInteriorStyle:

		STORE_CARD32 (values->interior_style, pBuf);
		break;

            case PEXPCInteriorStyleIndex:

		STORE_CARD32 (values->interior_style_index, pBuf);
		break;

            case PEXPCSurfaceColor:

		STORE_COLOR_SPEC (values->surface_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCReflectionAttr:

		STORE_REFLECTION_ATTR (values->reflection_attr, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCReflectionModel:

		STORE_CARD32 (values->reflection_model, pBuf);
		break;

            case PEXPCSurfaceInterp:

		STORE_CARD32 (values->surface_interp, pBuf);
		break;

            case PEXPCBFInteriorStyle:

		STORE_CARD32 (values->bf_interior_style, pBuf);
		break;

            case PEXPCBFInteriorStyleIndex:

		STORE_CARD32 (values->bf_interior_style_index, pBuf);
		break;

            case PEXPCBFSurfaceColor:

		STORE_COLOR_SPEC (values->bf_surface_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCBFReflectionAttr:

		STORE_REFLECTION_ATTR (values->bf_reflection_attr, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCBFReflectionModel:

		STORE_CARD32 (values->bf_reflection_model, pBuf);
		break;

            case PEXPCBFSurfaceInterp:

		STORE_CARD32 (values->bf_surface_interp, pBuf);
		break;

            case PEXPCSurfaceApprox:

		STORE_CARD32 (values->surface_approx.method, pBuf);
		STORE_FLOAT32 (values->surface_approx.u_tolerance, pBuf,
		    fpConvert, fpFormat);
		STORE_FLOAT32 (values->surface_approx.v_tolerance, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCCullingMode:

		STORE_CARD32 (values->culling_mode, pBuf);
		break;

            case PEXPCDistinguishFlag:

		STORE_CARD32 (values->distinguish, pBuf);
		break;

            case PEXPCPatternSize:

		STORE_COORD2D (values->pattern_size, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefPoint:

		STORE_COORD3D (values->pattern_ref_point, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefVec1:

		STORE_VECTOR3D (values->pattern_ref_vec1, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCPatternRefVec2:

		STORE_VECTOR3D (values->pattern_ref_vec2, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCInteriorBundleIndex:

		STORE_CARD32 (values->interior_bundle_index, pBuf);
		break;

            case PEXPCSurfaceEdgeFlag:

		STORE_CARD32 (values->surface_edges, pBuf);
		break;

            case PEXPCSurfaceEdgeType:

		STORE_CARD32 (values->surface_edge_type, pBuf);
		break;

            case PEXPCSurfaceEdgeWidth:

		STORE_FLOAT32 (values->surface_edge_width, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCSurfaceEdgeColor:

		STORE_COLOR_SPEC (values->surface_edge_color, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCEdgeBundleIndex:

		STORE_CARD32 (values->edge_bundle_index, pBuf);
		break;

            case PEXPCLocalTransform:

		STORE_LISTOF_FLOAT32 (16, values->local_transform, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCGlobalTransform:

		STORE_LISTOF_FLOAT32 (16, values->global_transform, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCModelClip:

		STORE_CARD32 (values->model_clip, pBuf);
		break;

            case PEXPCModelClipVolume:

		count = values->model_clip_volume.count;
		STORE_CARD32 (count, pBuf);
		STORE_LISTOF_HALFSPACE3D (count,
		    values->model_clip_volume.half_spaces, pBuf,
		    fpConvert, fpFormat);
		break;

            case PEXPCViewIndex:

		STORE_CARD32 (values->view_index, pBuf);
		break;

            case PEXPCLightState:

		count = values->light_state.count;
		STORE_CARD32 (count, pBuf);
		STORE_LISTOF_CARD16 (count, values->light_state.indices, pBuf);
		if (count & 1)
		    pBuf += SIZEOF (CARD16);
		break;

            case PEXPCDepthCueIndex:

		STORE_CARD32 (values->depth_cue_index, pBuf);
		break;

            case PEXPCPickID:

		STORE_CARD32 (values->pick_id, pBuf);
		break;

            case PEXPCHLHSRIdentifier:

		STORE_CARD32 (values->hlhsr_id, pBuf);
		break;

            case PEXPCNameSet:

		STORE_CARD32 (values->name_set, pBuf);
		break;

            case PEXPCASFValues:

		STORE_CARD32 (values->asf_enables, pBuf);
		STORE_CARD32 (values->asf_values, pBuf);
		break;

	    case PEXPCColorApproxIndex:

		STORE_CARD32 (values->color_approx_index, pBuf);
		break;

	    case PEXPCRenderingColorModel:

		STORE_CARD32 (values->rendering_color_model, pBuf);
		break;

	    case PEXPCParaSurfCharacteristics:

		pscType = values->para_surf_char.type;
		if (pscType == PEXPSCIsoCurves)
		{
		    size = SIZEOF (pexPSC_IsoparametricCurves);
	    
		    STORE_INT16 (PEXPSCIsoCurves, pBuf);
		    STORE_INT16 (size, pBuf);
		    STORE_PSC_ISOCURVES (
			values->para_surf_char.psc.iso_curves, pBuf);
		}
		else if (pscType == PEXPSCMCLevelCurves ||
		    pscType == PEXPSCWCLevelCurves)
		{
		    size = SIZEOF (pexPSC_LevelCurves) + (SIZEOF (float) *
			 values->para_surf_char.psc.level_curves.count);
	    
		    STORE_INT16 (pscType, pBuf);
		    STORE_INT16 (size, pBuf);
		    STORE_PSC_LEVELCURVES (
			values->para_surf_char.psc.level_curves, pBuf,
			fpConvert, fpFormat);
		    STORE_LISTOF_FLOAT32 (
			values->para_surf_char.psc.level_curves.count,
			values->para_surf_char.psc.level_curves.parameters,
			pBuf, fpConvert, fpFormat);
		}      
		break;
	    }
	}
    }

    *sizeRet = pBuf - *listRet;
}
