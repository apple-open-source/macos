/* $Xorg: pl_wks.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/

/******************************************************************************
Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts
Copyright 1992 by ShoGraphics, Inc., Mountain View, California

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital or ShowGraphics not be used in advertising or
publicity pertaining to distribution of the software without specific, written
prior permission.  Digital and ShowGraphics make no representations
about the suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

SHOGRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
SHOGRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
*************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"


PEXWorkstation
PEXCreateWorkstation (display, drawable, lineBundle, markerBundle, textBundle,
    interiorBundle, edgeBundle, colorTable, patternTable, textFontTable,
    depthCueTable, lightTable, colorApproxTable, highlightIncl,
    highlightExcl, invisibilityIncl, invisibilityExcl, bufferMode)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXLookupTable	lineBundle;
INPUT PEXLookupTable	markerBundle;
INPUT PEXLookupTable	textBundle;
INPUT PEXLookupTable	interiorBundle;
INPUT PEXLookupTable	edgeBundle;
INPUT PEXLookupTable	colorTable;
INPUT PEXLookupTable	patternTable;
INPUT PEXLookupTable	textFontTable;
INPUT PEXLookupTable	depthCueTable;
INPUT PEXLookupTable	lightTable;
INPUT PEXLookupTable	colorApproxTable;
INPUT PEXNameSet	highlightIncl;
INPUT PEXNameSet	highlightExcl;
INPUT PEXNameSet	invisibilityIncl;
INPUT PEXNameSet	invisibilityExcl;
INPUT int		bufferMode;

{
    register pexCreateWorkstationReq	*req;
    char				*pBuf;
    PEXWorkstation			wks;


    /*
     * Get a Phigs Workstation resource id from X.
     */

    wks = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreateWorkstation, pBuf);

    BEGIN_REQUEST_HEADER (CreateWorkstation, pBuf, req);

    PEXStoreReqHead (CreateWorkstation, req);
    req->drawable = drawable;
    req->wks = wks;
    req->lineBundle = lineBundle;
    req->markerBundle = markerBundle;
    req->textBundle = textBundle;
    req->interiorBundle = interiorBundle;
    req->edgeBundle = edgeBundle;
    req->colorTable = colorTable;
    req->patternTable = patternTable;
    req->textFontTable = textFontTable;
    req->depthCueTable = depthCueTable;
    req->lightTable = lightTable;
    req->colorApproxTable = colorApproxTable;
    req->highlightIncl = highlightIncl;
    req->highlightExcl = highlightExcl;
    req->invisIncl = invisibilityIncl;
    req->invisExcl = invisibilityExcl;
    req->bufferMode = bufferMode;

    END_REQUEST_HEADER (CreateWorkstation, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (wks);
}


void
PEXFreeWorkstation (display, wks)

INPUT Display		*display;
INPUT PEXWorkstation	wks;

{
    register pexFreeWorkstationReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreeWorkstation, pBuf);

    BEGIN_REQUEST_HEADER (FreeWorkstation, pBuf, req);

    PEXStoreReqHead (FreeWorkstation, req);
    req->id = wks;

    END_REQUEST_HEADER (FreeWorkstation, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXWorkstationAttributes *
PEXGetWorkstationAttributes (display, wks, valueMask)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT unsigned long	*valueMask;

{
    register pexGetWorkstationAttributesReq	*req;
    register char				*pBuf, *pBufSave;
    pexGetWorkstationAttributesReply		rep;
    PEXWorkstationAttributes			*ppwi;
    Bool					bitSet;
    unsigned long				count;
    int						i;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetWorkstationAttributes, pBuf);

    BEGIN_REQUEST_HEADER (GetWorkstationAttributes, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetWorkstationAttributes, fpFormat, req);
    req->wks = wks;
    req->itemMask0 = valueMask[0];
    req->itemMask1 = valueMask[1];

    END_REQUEST_HEADER (GetWorkstationAttributes, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (NULL);            /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    ppwi = (PEXWorkstationAttributes *)
	Xmalloc (sizeof (PEXWorkstationAttributes));

    ppwi->defined_views.count = 0;
    ppwi->defined_views.views = NULL;
    ppwi->posted_structures.count = 0;
    ppwi->posted_structures.structures = NULL;

    for (i = 0; i < (PEXPWMaxShift + 1); i++)
    {
	bitSet = valueMask[i >> 5] & (1L << (i & 0x1f));
	if (bitSet != 0)
	{
	    switch (i)
	    {
	    case PEXPWDisplayUpdate:

		EXTRACT_LOV_INT16 (pBuf, ppwi->drawable_update);
		break;

	    case PEXPWVisualState:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->visual_state);
		break;

	    case PEXPWDisplaySurface:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->drawable_surface);
		break;

	    case PEXPWViewUpdate:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->view_update);
		break;

	    case PEXPWDefinedViews:

		EXTRACT_CARD32 (pBuf, count);
		ppwi->defined_views.count = count;

		ppwi->defined_views.views = (PEXTableIndex *) Xmalloc (
		    (unsigned) (count * sizeof (PEXTableIndex)));

		EXTRACT_LISTOF_CARD16 (count, pBuf, ppwi->defined_views.views);
		pBuf += ((count & 1) * SIZEOF (CARD16));
		break;

	    case PEXPWWorkstationUpdate:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->wks_update);
		break;

	    case PEXPWReqNPCSubVolume:

		EXTRACT_NPC_SUBVOLUME (pBuf, ppwi->req_npc_subvolume,
                    fpConvert, fpFormat);
		break;

	    case PEXPWCurNPCSubVolume:

		EXTRACT_NPC_SUBVOLUME (pBuf, ppwi->cur_npc_subvolume,
		    fpConvert, fpFormat);
		break;

	    case PEXPWReqViewport:

		EXTRACT_VIEWPORT (pBuf, ppwi->req_workstation_viewport,
		    fpConvert, fpFormat);
		break;

	    case PEXPWCurViewport:

		EXTRACT_VIEWPORT (pBuf, ppwi->cur_workstation_viewport,
		    fpConvert, fpFormat);
		break;

	    case PEXPWHLHSRUpdate:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->hlhsr_update);
		break;

	    case PEXPWReqHLHSRMode:

		EXTRACT_LOV_INT16 (pBuf, ppwi->req_hlhsr_mode);
		break;

	    case PEXPWCurHLHSRMode:

		EXTRACT_LOV_INT16 (pBuf, ppwi->cur_hlhsr_mode);
		break;

	    case PEXPWDrawable:

		EXTRACT_CARD32 (pBuf, ppwi->drawable);
		break;

	    case PEXPWMarkerBundle:

		EXTRACT_CARD32 (pBuf, ppwi->marker_bundle);
		break;

	    case PEXPWTextBundle:

		EXTRACT_CARD32 (pBuf, ppwi->text_bundle);
		break;

	    case PEXPWLineBundle:

		EXTRACT_CARD32 (pBuf, ppwi->line_bundle);
		break;

	    case PEXPWInteriorBundle:

		EXTRACT_CARD32 (pBuf, ppwi->interior_bundle);
		break;

	    case PEXPWEdgeBundle:

		EXTRACT_CARD32 (pBuf, ppwi->edge_bundle);
		break;

	    case PEXPWColorTable:

		EXTRACT_CARD32 (pBuf, ppwi->color_table);
		break;

	    case PEXPWDepthCueTable:

		EXTRACT_CARD32 (pBuf, ppwi->depth_cue_table);
		break;

	    case PEXPWLightTable:

		EXTRACT_CARD32 (pBuf, ppwi->light_table);
		break;

	    case PEXPWColorApproxTable:

		EXTRACT_CARD32 (pBuf, ppwi->color_approx_table);
		break;

	    case PEXPWPatternTable:

		EXTRACT_CARD32 (pBuf, ppwi->pattern_table);
		break;

	    case PEXPWTextFontTable:

		EXTRACT_CARD32 (pBuf, ppwi->text_font_table);
		break;

	    case PEXPWHighlightIncl:

		EXTRACT_CARD32 (pBuf, ppwi->highlight_incl);
		break;

	    case PEXPWHighlightExcl:

		EXTRACT_CARD32 (pBuf, ppwi->highlight_excl);
		break;

	    case PEXPWInvisibilityIncl:

		EXTRACT_CARD32 (pBuf, ppwi->invisibility_incl);
		break;

	    case PEXPWInvisibilityExcl:

		EXTRACT_CARD32 (pBuf, ppwi->invisibility_excl);
		break;

	    case PEXPWPostedStructures:

		EXTRACT_CARD32 (pBuf, count);
		ppwi->posted_structures.count = count;

		ppwi->posted_structures.structures = (PEXPostedStructure *)
		    Xmalloc ((unsigned) (count * sizeof (PEXPostedStructure)));

		EXTRACT_LISTOF_POSTED_STRUCS (count, pBuf,
		    ppwi->posted_structures.structures, fpConvert, fpFormat);
		break;

	    case PEXPWNumPriorities:

		EXTRACT_CARD32 (pBuf, ppwi->count_priorities);
		break;

	    case PEXPWBufferUpdate:

		EXTRACT_LOV_CARD8 (pBuf, ppwi->buffer_update);
		break;

	    case PEXPWReqBufferMode:

		EXTRACT_LOV_CARD16 (pBuf, ppwi->req_buffer_mode);
		break;

	    case PEXPWCurBufferMode:

		EXTRACT_LOV_CARD16 (pBuf, ppwi->cur_buffer_mode);
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

    return (ppwi);
}


Status
PEXGetWorkstationDynamics (display, d, dynamics)

INPUT Display			*display;
INPUT Drawable			d;
INPUT PEXWorkstationDynamics	*dynamics;

{
    register pexGetWorkstationDynamicsReq	*req;
    char					*pBuf;
    pexGetWorkstationDynamicsReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetWorkstationDynamics, pBuf);

    BEGIN_REQUEST_HEADER (GetWorkstationDynamics, pBuf, req);

    PEXStoreReqHead (GetWorkstationDynamics, req);
    req->drawable = d;

    END_REQUEST_HEADER (GetWorkstationDynamics, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (0);            /* return an error */
    }


    /*
     * Fill in the dynamics.
     */

    dynamics->view_rep = rep.viewRep;
    dynamics->marker_bundle = rep.markerBundle;
    dynamics->text_bundle = rep.textBundle;
    dynamics->line_bundle = rep.lineBundle;
    dynamics->interior_bundle = rep.interiorBundle;
    dynamics->edge_bundle = rep.edgeBundle;
    dynamics->color_table = rep.colorTable;
    dynamics->pattern_table = rep.patternTable;
    dynamics->wks_transform = rep.wksTransform;
    dynamics->highlight_filter = rep.highlightFilter;
    dynamics->invisibility_filter = rep.invisibilityFilter;
    dynamics->hlhsr_mode = rep.HlhsrMode;
    dynamics->structure_modify = rep.structureModify;
    dynamics->post_structure = rep.postStructure;
    dynamics->unpost_structure = rep.unpostStructure;
    dynamics->delete_structure = rep.deleteStructure;
    dynamics->reference_modify = rep.referenceModify;
    dynamics->buffer_modify = rep.bufferModify;
    dynamics->light_table = rep.lightTable;
    dynamics->depth_cue_table = rep.depthCueTable;
    dynamics->color_approx_table = rep.colorApproxTable;


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetWorkstationViewRep (display, wks, index,
    viewUpdateReturn, reqViewReturn, curViewReturn)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT unsigned int	index;
OUTPUT int		*viewUpdateReturn;
OUTPUT PEXViewRep	*reqViewReturn;
OUTPUT PEXViewRep	*curViewReturn;

{
    register pexGetWorkstationViewRepReq	*req;
    register char				*pBuf, *pBufSave;
    pexGetWorkstationViewRepReply		rep;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetWorkstationViewRep, pBuf);

    BEGIN_REQUEST_HEADER (GetWorkstationViewRep, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetWorkstationViewRep, fpFormat, req);
    req->wks = wks;
    req->index = index;

    END_REQUEST_HEADER (GetWorkstationViewRep, pBuf, req);


    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (0);            /* return an error */
    }

    *viewUpdateReturn = rep.viewUpdate;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Get the requested view rep.
     */

    EXTRACT_CARD32 (pBuf, reqViewReturn->index);
    EXTRACT_CARD32 (pBuf, reqViewReturn->view.clip_flags);

    EXTRACT_NPC_SUBVOLUME (pBuf, reqViewReturn->view.clip_limits,
	fpConvert, fpFormat);

    EXTRACT_LISTOF_FLOAT32 (16, pBuf, reqViewReturn->view.orientation,
        fpConvert, fpFormat);
    EXTRACT_LISTOF_FLOAT32 (16, pBuf, reqViewReturn->view.mapping,
        fpConvert, fpFormat);


    /*
     * Get the current view rep.
     */

    EXTRACT_CARD32 (pBuf, curViewReturn->index);
    EXTRACT_CARD32 (pBuf, curViewReturn->view.clip_flags);

    EXTRACT_NPC_SUBVOLUME (pBuf, curViewReturn->view.clip_limits,
	fpConvert, fpFormat);

    EXTRACT_LISTOF_FLOAT32 (16, pBuf, curViewReturn->view.orientation,
        fpConvert, fpFormat);
    EXTRACT_LISTOF_FLOAT32 (16, pBuf, curViewReturn->view.mapping,
        fpConvert, fpFormat);

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXGetWorkstationPostings (display, structure, numWksReturn, wksReturn)

INPUT Display		*display;
INPUT PEXStructure	structure;
OUTPUT unsigned long	*numWksReturn;
OUTPUT PEXWorkstation	**wksReturn;

{
    register pexGetWorkstationPostingsReq	*req;
    char					*pBuf;
    pexGetWorkstationPostingsReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetWorkstationPostings, pBuf);

    BEGIN_REQUEST_HEADER (GetWorkstationPostings, pBuf, req);

    PEXStoreReqHead (GetWorkstationPostings, req);
    req->id = structure;

    END_REQUEST_HEADER (GetWorkstationPostings, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*wksReturn = NULL;
        return (0);            /* return an error */
    }

    *numWksReturn = rep.length;


    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    *wksReturn = (PEXWorkstation *) Xmalloc (
        (unsigned) (*numWksReturn * sizeof (PEXWorkstation)));

    XREAD_LISTOF_CARD32 (display, *numWksReturn, *wksReturn);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


void
PEXSetWorkstationViewPriority (display, wks, index1, index2, priority)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT unsigned int	index1;
INPUT unsigned int	index2;
INPUT int		priority;

{
    register pexSetWorkstationViewPriorityReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationViewPriority, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationViewPriority, pBuf, req);

    PEXStoreReqHead (SetWorkstationViewPriority, req);
    req->wks = wks;
    req->index1 = index1;
    req->index2 = index2;
    req->priority = priority;

    END_REQUEST_HEADER (SetWorkstationViewPriority, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationDisplayUpdateMode (display, wks, displayUpdate)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT int		displayUpdate;

{
    register pexSetWorkstationDisplayUpdateModeReq	*req;
    char						*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationDisplayUpdateMode, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationDisplayUpdateMode, pBuf, req);

    PEXStoreReqHead (SetWorkstationDisplayUpdateMode, req);
    req->wks = wks;
    req->displayUpdate = displayUpdate;

    END_REQUEST_HEADER (SetWorkstationDisplayUpdateMode, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationBufferMode (display, wks, bufferMode)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT int		bufferMode;

{
    register pexSetWorkstationBufferModeReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationBufferMode, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationBufferMode, pBuf, req);

    PEXStoreReqHead (SetWorkstationBufferMode, req);
    req->wks = wks;
    req->bufferMode = bufferMode;

    END_REQUEST_HEADER (SetWorkstationBufferMode, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationViewRep (display, wks, viewIndex, values)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT unsigned int	viewIndex;
INPUT PEXViewEntry	*values;

{
    register pexSetWorkstationViewRepReq	*req;
    char					*pBuf;
    int						fpConvert;
    int						fpFormat;
    char					*ptr;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationViewRep, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationViewRep, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (SetWorkstationViewRep, fpFormat, req);
    req->wks = wks;
    req->index = viewIndex;
    req->clipFlags = values->clip_flags;

    if (fpConvert)
    {
	FP_CONVERT_HTON (values->clip_limits.min.x,
	    req->clipLimits_xmin, fpFormat);
	FP_CONVERT_HTON (values->clip_limits.min.y,
	    req->clipLimits_ymin, fpFormat);
	FP_CONVERT_HTON (values->clip_limits.min.z,
	    req->clipLimits_zmin, fpFormat);
	FP_CONVERT_HTON (values->clip_limits.max.x,
	    req->clipLimits_xmax, fpFormat);
	FP_CONVERT_HTON (values->clip_limits.max.y,
	    req->clipLimits_ymax, fpFormat);
	FP_CONVERT_HTON (values->clip_limits.max.z,
	    req->clipLimits_zmax, fpFormat);
    }
    else
    {
	req->clipLimits_xmin = values->clip_limits.min.x;
	req->clipLimits_ymin = values->clip_limits.min.y;
	req->clipLimits_zmin = values->clip_limits.min.z;
	req->clipLimits_xmax = values->clip_limits.max.x;
	req->clipLimits_ymax = values->clip_limits.max.y;
	req->clipLimits_zmax = values->clip_limits.max.z;
    }

    ptr = (char *) req->view_orientation;
    STORE_LISTOF_FLOAT32 (16, values->orientation, ptr, fpConvert, fpFormat);

    ptr = (char *) req->view_mapping;
    STORE_LISTOF_FLOAT32 (16, values->mapping, ptr, fpConvert, fpFormat);

    END_REQUEST_HEADER (SetWorkstationViewRep, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationWindow (display, wks, npcSubvolume)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT PEXNPCSubVolume	*npcSubvolume;

{
    register pexSetWorkstationWindowReq		*req;
    char					*pBuf;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationWindow, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationWindow, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (SetWorkstationWindow, fpFormat, req);
    req->wks = wks;

    if (fpConvert)
    {
	FP_CONVERT_HTON (npcSubvolume->min.x,
	    req->npcSubvolume_xmin, fpFormat);
	FP_CONVERT_HTON (npcSubvolume->min.y,
	    req->npcSubvolume_ymin, fpFormat);
	FP_CONVERT_HTON (npcSubvolume->min.z,
            req->npcSubvolume_zmin, fpFormat);
	FP_CONVERT_HTON (npcSubvolume->max.x,
	    req->npcSubvolume_xmax, fpFormat);
	FP_CONVERT_HTON (npcSubvolume->max.y,
	    req->npcSubvolume_ymax, fpFormat);
	FP_CONVERT_HTON (npcSubvolume->max.z,
	    req->npcSubvolume_zmax, fpFormat);
    }
    else
    {
	req->npcSubvolume_xmin = npcSubvolume->min.x;
	req->npcSubvolume_ymin = npcSubvolume->min.y;
	req->npcSubvolume_zmin = npcSubvolume->min.z;
	req->npcSubvolume_xmax = npcSubvolume->max.x;
	req->npcSubvolume_ymax = npcSubvolume->max.y;
	req->npcSubvolume_zmax = npcSubvolume->max.z;
    }

    END_REQUEST_HEADER (SetWorkstationWindow, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationViewport (display, wks, viewport)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT PEXViewport	*viewport;

{
    register pexSetWorkstationViewportReq	*req;
    char					*pBuf;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationViewport, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationViewport, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (SetWorkstationViewport, fpFormat, req);
    req->wks = wks;
    req->viewport_xmin = viewport->min.x;
    req->viewport_ymin = viewport->min.y;
    req->viewport_xmax = viewport->max.x;
    req->viewport_ymax = viewport->max.y;
    req->useDrawable = viewport->use_drawable;

    if (fpConvert)
    {
	FP_CONVERT_HTON (viewport->min.z, req->viewport_zmin, fpFormat);
	FP_CONVERT_HTON (viewport->max.z, req->viewport_zmax, fpFormat);
    }
    else
    {
	req->viewport_zmin = viewport->min.z;
	req->viewport_zmax = viewport->max.z;
    }

    END_REQUEST_HEADER (SetWorkstationViewport, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXSetWorkstationHLHSRMode (display, wks, hlhsrMode)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT int		hlhsrMode;

{
    register pexSetWorkstationHLHSRModeReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (SetWorkstationHLHSRMode, pBuf);

    BEGIN_REQUEST_HEADER (SetWorkstationHLHSRMode, pBuf, req);

    PEXStoreReqHead (SetWorkstationHLHSRMode, req);
    req->wks = wks;
    req->mode = (pexEnumTypeIndex) hlhsrMode;

    END_REQUEST_HEADER (SetWorkstationHLHSRMode, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXRedrawAllStructures (display, wks)

INPUT Display		*display;
INPUT PEXWorkstation	wks;

{
    register pexRedrawAllStructuresReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (RedrawAllStructures, pBuf);

    BEGIN_REQUEST_HEADER (RedrawAllStructures, pBuf, req);

    PEXStoreReqHead (RedrawAllStructures, req);
    req->id = wks;

    END_REQUEST_HEADER (RedrawAllStructures, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXUpdateWorkstation (display, wks)

INPUT Display		*display;
INPUT PEXWorkstation	wks;

{
    register pexUpdateWorkstationReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (UpdateWorkstation, pBuf);

    BEGIN_REQUEST_HEADER (UpdateWorkstation, pBuf, req);

    PEXStoreReqHead (UpdateWorkstation, req);
    req->id = wks;

    END_REQUEST_HEADER (UpdateWorkstation, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXExecuteDeferredActions (display, wks)

INPUT Display		*display;
INPUT PEXWorkstation	wks;

{
    register pexExecuteDeferredActionsReq	*req;
    char					*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (ExecuteDeferredActions, pBuf);

    BEGIN_REQUEST_HEADER (ExecuteDeferredActions, pBuf, req);

    PEXStoreReqHead (ExecuteDeferredActions, req);
    req->id = wks;

    END_REQUEST_HEADER (ExecuteDeferredActions, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXMapDCToWC (display, wks, dc_count, dc_points,
    view_index_return, wc_count_return, wc_points_return)

INPUT Display			*display;
INPUT PEXWorkstation		wks;
INPUT unsigned long		dc_count;
INPUT PEXDeviceCoord		*dc_points;
OUTPUT unsigned int		*view_index_return;
OUTPUT unsigned long		*wc_count_return;
OUTPUT PEXCoord			**wc_points_return;

{
    register pexMapDCtoWCReq	*req;
    char			*pBuf;
    pexMapDCtoWCReply		rep;
    int				size;
    int				fpConvert;
    int				fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    size = dc_count * SIZEOF (pexDeviceCoord);
    PEXGetReqExtra (MapDCtoWC, size, pBuf);

    BEGIN_REQUEST_HEADER (MapDCtoWC, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (MapDCtoWC, fpFormat, size, req);
    req->wks = wks;
    req->numCoords = dc_count;

    END_REQUEST_HEADER (MapDCtoWC, pBuf, req);

    STORE_LISTOF_DEVCOORD (dc_count, dc_points, pBuf, fpConvert, fpFormat);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*wc_count_return = 0;
	*wc_points_return = NULL;
        return (0);            /* return an error */
    }

    *view_index_return = rep.viewIndex;
    *wc_count_return = rep.numCoords;


    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    *wc_points_return = (PEXCoord *) Xmalloc (
        (unsigned) (rep.numCoords * sizeof (PEXCoord)));

    XREAD_LISTOF_COORD3D (display, rep.numCoords, (*wc_points_return),
	fpConvert, fpFormat);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


Status
PEXMapWCToDC (display, wks, wc_count, wc_points,
    view_index, dc_count_return, dc_points_return)

INPUT Display			*display;
INPUT PEXWorkstation		wks;
INPUT unsigned long		wc_count;
INPUT PEXCoord			*wc_points;
INPUT unsigned int		view_index;
OUTPUT unsigned long		*dc_count_return;
OUTPUT PEXDeviceCoord		**dc_points_return;

{
    register pexMapWCtoDCReq	*req;
    char			*pBuf;
    pexMapWCtoDCReply		rep;
    int				size;
    int				fpConvert;
    int				fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    size = wc_count * SIZEOF (pexCoord3D);
    PEXGetReqExtra (MapWCtoDC, size, pBuf);

    BEGIN_REQUEST_HEADER (MapWCtoDC, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (MapWCtoDC, fpFormat, size, req);
    req->wks = wks;
    req->index = view_index;
    req->numCoords = wc_count;

    END_REQUEST_HEADER (MapWCtoDC, pBuf, req);

    STORE_LISTOF_COORD3D (wc_count, wc_points, pBuf, fpConvert, fpFormat);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*dc_count_return = 0;
	*dc_points_return = NULL;
        return (0);            /* return an error */
    }

    *dc_count_return = rep.numCoords;


    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    *dc_points_return = (PEXDeviceCoord *) Xmalloc (
        (unsigned) (rep.numCoords * sizeof (PEXDeviceCoord)));

    XREAD_LISTOF_DEVCOORD (display, rep.numCoords, (*dc_points_return),
        fpConvert, fpFormat);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


void
PEXPostStructure (display, wks, structure, priority)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT PEXStructure	structure;
INPUT double		priority;

{
    register pexPostStructureReq	*req;
    char				*pBuf;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (PostStructure, pBuf);

    BEGIN_REQUEST_HEADER (PostStructure, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (PostStructure, fpFormat, req);
    req->wks = wks;
    req->sid = structure;

    if (fpConvert)
    {
	FP_CONVERT_DHTON (priority, req->priority, fpFormat);
    }
    else
	req->priority = priority;

    END_REQUEST_HEADER (PostStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXUnpostStructure (display, wks, structure)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT PEXStructure	structure;

{
    register pexUnpostStructureReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (UnpostStructure, pBuf);

    BEGIN_REQUEST_HEADER (UnpostStructure, pBuf, req);

    PEXStoreReqHead (UnpostStructure, req);
    req->wks = wks;
    req->sid = structure;

    END_REQUEST_HEADER (UnpostStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXUnpostAllStructures (display, wks)

INPUT Display		*display;
INPUT PEXWorkstation	wks;

{
    register pexUnpostAllStructuresReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (UnpostAllStructures, pBuf);

    BEGIN_REQUEST_HEADER (UnpostAllStructures, pBuf, req);

    PEXStoreReqHead (UnpostAllStructures, req);
    req->id = wks;

    END_REQUEST_HEADER (UnpostAllStructures, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXRedrawClipRegion (display, wks, numRectangles, deviceRectangles)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT unsigned long	numRectangles;
INPUT PEXDeviceRect	*deviceRectangles;

{
    register pexRedrawClipRegionReq	*req;
    char				*pBuf;
    int					size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    size = numRectangles * SIZEOF (pexDeviceRect);
    PEXGetReqExtra (RedrawClipRegion, size, pBuf);

    BEGIN_REQUEST_HEADER (RedrawClipRegion, pBuf, req);

    PEXStoreReqExtraHead (RedrawClipRegion, size, req);
    req->wks = wks;
    req->numRects = numRectangles;

    END_REQUEST_HEADER (RedrawClipRegion, pBuf, req);

    STORE_LISTOF_DEVRECT (numRectangles, deviceRectangles, pBuf);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}
