/* $Xorg: pl_rdr.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */

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

static void _PEXGenerateRendererList();


PEXRenderer
PEXCreateRenderer (display, drawable, valueMask, values)

INPUT Display			*display;
INPUT Drawable			drawable;
INPUT unsigned long		valueMask;
INPUT PEXRendererAttributes 	*values;

{
    register pexCreateRendererReq	*req;
    char				*pBuf;
    PEXRenderer				rdr;
    int					size = 0;
    char				*pList;
    int					fpConvert;
    int					fpFormat;


    /*
     * Get a renderer resource id from X.
     */

    rdr = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.  For the value mask,
     * turn off the Current Path and Renderer State bits, since these
     * attributes are not modifiable.
     */

    valueMask &= ~(PEXRACurrentPath | PEXRARendererState);

    PEXGetReq (CreateRenderer, pBuf);

    BEGIN_REQUEST_HEADER (CreateRenderer, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (CreateRenderer, fpFormat, req);
    req->drawable = drawable;
    req->rdr = rdr;
    req->itemMask = valueMask;

    if (valueMask != 0)
    {
	_PEXGenerateRendererList (display, fpConvert, fpFormat,
	    valueMask, values, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (CreateRenderer, pBuf, req);


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

    return (rdr);
}


void
PEXFreeRenderer (display, renderer)

INPUT Display		*display;
INPUT PEXRenderer	renderer;

{
    register pexFreeRendererReq		*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreeRenderer, pBuf);

    BEGIN_REQUEST_HEADER (FreeRenderer, pBuf, req);

    PEXStoreReqHead (FreeRenderer, req);
    req->id = renderer;

    END_REQUEST_HEADER (FreeRenderer, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXRendererAttributes *
PEXGetRendererAttributes (display, renderer, valueMask)

INPUT Display			*display;
INPUT PEXRenderer		renderer;
INPUT unsigned long		valueMask;

{
    register pexGetRendererAttributesReq	*req;
    register PEXRendererAttributes		*pAttr;
    register char				*pBuf, *pBufSave;
    pexGetRendererAttributesReply		rep;
    unsigned long				f;
    int						i;
    unsigned					count;
    int						fpConvert;
    int						fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetRendererAttributes, pBuf);

    BEGIN_REQUEST_HEADER (GetRendererAttributes, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetRendererAttributes, fpFormat, req);
    req->rdr = renderer;
    req->itemMask = valueMask;

    END_REQUEST_HEADER (GetRendererAttributes, pBuf, req);

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

    pAttr = (PEXRendererAttributes *)
	Xmalloc (sizeof (PEXRendererAttributes));

    pAttr->current_path.count = 0;
    pAttr->current_path.elements = NULL;
    pAttr->clip_list.count = 0;
    pAttr->clip_list.rectangles = NULL;
    pAttr->pick_start_path.count = 0;
    pAttr->pick_start_path.elements = NULL;

    for (i = 0; i < (PEXRAMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
            switch (f)
	    {
	    case PEXRACurrentPath:

		EXTRACT_CARD32 (pBuf, count);
		pAttr->current_path.count = count;

		pAttr->current_path.elements = (PEXElementRef *)
		    Xmalloc (count * sizeof (PEXElementRef));

		EXTRACT_LISTOF_ELEMREF (count,
		    pBuf, pAttr->current_path.elements);
		break;

	    case PEXRAPipelineContext:

		EXTRACT_CARD32 (pBuf, pAttr->pipeline_context);
		break;

	    case PEXRAMarkerBundle:

		EXTRACT_CARD32 (pBuf, pAttr->marker_bundle);
		break;

	    case PEXRATextBundle:

		EXTRACT_CARD32 (pBuf, pAttr->text_bundle);
		break;

	    case PEXRALineBundle:

		EXTRACT_CARD32 (pBuf, pAttr->line_bundle);
		break;

	    case PEXRAInteriorBundle:

		EXTRACT_CARD32 (pBuf, pAttr->interior_bundle);
		break;

	    case PEXRAEdgeBundle:

		EXTRACT_CARD32 (pBuf, pAttr->edge_bundle);
		break;

	    case PEXRAViewTable:

		EXTRACT_CARD32 (pBuf, pAttr->view_table);
		break;

	    case PEXRAColorTable:

		EXTRACT_CARD32 (pBuf, pAttr->color_table);
		break;

	    case PEXRADepthCueTable:

		EXTRACT_CARD32 (pBuf, pAttr->depth_cue_table);
		break;

	    case PEXRALightTable:

		EXTRACT_CARD32 (pBuf, pAttr->light_table);
		break;

	    case PEXRAColorApproxTable:

		EXTRACT_CARD32 (pBuf, pAttr->color_approx_table);
		break;

	    case PEXRAPatternTable:

		EXTRACT_CARD32 (pBuf, pAttr->pattern_table);
		break;

	    case PEXRATextFontTable:

		EXTRACT_CARD32 (pBuf, pAttr->text_font_table);
		break;

	    case PEXRAHighlightIncl:

		EXTRACT_CARD32 (pBuf, pAttr->highlight_incl);
		break;

	    case PEXRAHighlightExcl:

		EXTRACT_CARD32 (pBuf, pAttr->highlight_excl);
		break;

	    case PEXRAInvisibilityIncl:

		EXTRACT_CARD32 (pBuf, pAttr->invisibility_incl);
		break;

	    case PEXRAInvisibilityExcl:

		EXTRACT_CARD32 (pBuf, pAttr->invisibility_excl);
		break;

	    case PEXRARendererState:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->renderer_state);
		break;

	    case PEXRAHLHSRMode:

		EXTRACT_LOV_INT16 (pBuf, pAttr->hlhsr_mode);
		break;

	    case PEXRANPCSubVolume:

		EXTRACT_NPC_SUBVOLUME (pBuf, pAttr->npc_subvolume,
		    fpConvert, fpFormat);
		break;
		
	    case PEXRAViewport:

		EXTRACT_VIEWPORT (pBuf, pAttr->viewport, fpConvert, fpFormat);
		break;

	    case PEXRAClipList:

		EXTRACT_CARD32 (pBuf, count);
		pAttr->clip_list.count = count;

		pAttr->clip_list.rectangles = (PEXDeviceRect *)
		    Xmalloc (count * sizeof (PEXDeviceRect));

		EXTRACT_LISTOF_DEVRECT (count,
		    pBuf, pAttr->clip_list.rectangles);
		break;

	    case PEXRAPickIncl:

		EXTRACT_CARD32 (pBuf, pAttr->pick_incl);
		break;

	    case PEXRAPickExcl:

		EXTRACT_CARD32 (pBuf, pAttr->pick_excl);
		break;

	    case PEXRAPickStartPath:

		EXTRACT_CARD32 (pBuf, count);
		pAttr->pick_start_path.count = count;

		pAttr->pick_start_path.elements = (PEXElementRef *)
		    Xmalloc (count * sizeof (PEXElementRef));

		EXTRACT_LISTOF_ELEMREF (count,
		    pBuf, pAttr->pick_start_path.elements);
		break;

	    case PEXRABackgroundColor:

		EXTRACT_COLOR_SPEC (pBuf, pAttr->background_color,
		    fpConvert, fpFormat);
		break;

	    case PEXRAClearImage:

		EXTRACT_LOV_CARD8 (pBuf, pAttr->clear_image);
		break;

	    case PEXRAClearZ:

		EXTRACT_LOV_CARD8 (pBuf, pAttr->clear_z);
		break;

	    case PEXRAEchoMode:

		EXTRACT_LOV_CARD16 (pBuf, pAttr->echo_mode);
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


Status
PEXGetRendererDynamics (display, renderer, tablesReturn,
    namesetsReturn, attributesReturn)

INPUT Display			*display;
INPUT PEXRenderer		renderer;
OUTPUT unsigned long		*tablesReturn;
OUTPUT unsigned long		*namesetsReturn;
OUTPUT unsigned long		*attributesReturn;

{
    register pexGetRendererDynamicsReq	*req;
    char				*pBuf;
    pexGetRendererDynamicsReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetRendererDynamics, pBuf);

    BEGIN_REQUEST_HEADER (GetRendererDynamics, pBuf, req);

    PEXStoreReqHead (GetRendererDynamics, req);
    req->id = renderer;

    END_REQUEST_HEADER (GetRendererDynamics, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*tablesReturn = *namesetsReturn = *attributesReturn = 0;
        return (0);	            /* return an error */
    }

    *tablesReturn = rep.tables;
    *namesetsReturn = rep.namesets;
    *attributesReturn = rep.attributes;


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


void
PEXChangeRenderer (display, renderer, valueMask, values)

INPUT Display			*display;
INPUT PEXRenderer		renderer;
INPUT unsigned long		valueMask;
INPUT PEXRendererAttributes 	*values;

{
    register pexChangeRendererReq	*req;
    char				*pBuf;
    int					size = 0;
    char				*pList;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.  For the item mask,
     * turn off the Current Path and Renderer State attributes, since
     * these are not modifiable.
     */

    valueMask &= ~(PEXRACurrentPath | PEXRARendererState);

    PEXGetReq (ChangeRenderer, pBuf);

    BEGIN_REQUEST_HEADER (ChangeRenderer, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (ChangeRenderer, fpFormat, req);
    req->rdr = renderer;
    req->itemMask = valueMask;

    if (valueMask != 0)
    {
	_PEXGenerateRendererList (display, fpConvert, fpFormat,
	    valueMask, values, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (ChangeRenderer, pBuf, req);


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


void
PEXBeginRendering (display, drawable, renderer)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXRenderer	renderer;

{
    register pexBeginRenderingReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (BeginRendering, pBuf);

    BEGIN_REQUEST_HEADER (BeginRendering, pBuf, req);

    PEXStoreReqHead (BeginRendering, req);
    req->rdr = renderer;
    req->drawable = drawable;

    END_REQUEST_HEADER (BeginRendering, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXEndRendering (display, renderer, flush)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
INPUT int		flush;

{
    register pexEndRenderingReq		*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (EndRendering, pBuf);

    BEGIN_REQUEST_HEADER (EndRendering, pBuf, req);

    PEXStoreReqHead (EndRendering, req);
    req->rdr = renderer;
    req->flushFlag = flush;

    END_REQUEST_HEADER (EndRendering, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXBeginStructure (display, renderer, id)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
INPUT long		id;

{
    register pexBeginStructureReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (BeginStructure, pBuf);

    BEGIN_REQUEST_HEADER (BeginStructure, pBuf, req);

    PEXStoreReqHead (BeginStructure, req);
    req->rdr = renderer;
    req->sid = id;

    END_REQUEST_HEADER (BeginStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXEndStructure (display, renderer)

INPUT Display		*display;
INPUT PEXRenderer	renderer;

{
    register pexEndStructureReq		*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (EndStructure, pBuf);

    BEGIN_REQUEST_HEADER (EndStructure, pBuf, req);

    PEXStoreReqHead (EndStructure, req);
    req->id = renderer;

    END_REQUEST_HEADER (EndStructure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXRenderNetwork (display, drawable, renderer, structure)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXRenderer	renderer;
INPUT PEXStructure	structure;

{
    register pexRenderNetworkReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (RenderNetwork, pBuf);

    BEGIN_REQUEST_HEADER (RenderNetwork, pBuf, req);

    PEXStoreReqHead (RenderNetwork, req);
    req->rdr = renderer;
    req->drawable = drawable;
    req->sid = structure;

    END_REQUEST_HEADER (RenderNetwork, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXRenderElements (display, renderer, sid, whence1, offset1, whence2, offset2)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
INPUT PEXStructure	sid;
INPUT int		whence1;
INPUT long		offset1;
INPUT int		whence2;
INPUT long		offset2;

{
    register pexRenderElementsReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (RenderElements, pBuf);

    BEGIN_REQUEST_HEADER (RenderElements, pBuf, req);

    PEXStoreReqHead (RenderElements, req);
    req->rdr = renderer;
    req->sid = sid;
    req->position1_whence = whence1;
    req->position1_offset = offset1;
    req->position2_whence = whence2;
    req->position2_offset = offset2;

    END_REQUEST_HEADER (RenderElements, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXAccumulateState (display, renderer, numElements, elements)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
INPUT unsigned long	numElements;
INPUT PEXElementRef	*elements;

{
    register pexAccumulateStateReq	*req;
    char				*pBuf;
    unsigned int			size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    size = numElements * SIZEOF (pexElementRef);
    PEXGetReqExtra (AccumulateState, size, pBuf);

    BEGIN_REQUEST_HEADER (AccumulateState, pBuf, req);

    PEXStoreReqExtraHead (AccumulateState, size, req);
    req->rdr = renderer;
    req->numElRefs = numElements;

    END_REQUEST_HEADER (AccumulateState, pBuf, req);

    STORE_LISTOF_ELEMREF (numElements, elements, pBuf);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}



/*
 * Routine to write a packed list of renderer attributes to the
 * transport buffer.
 */


static void
_PEXGenerateRendererList (display, fpConvert, fpFormat,
    valueMask, values, sizeRet, listRet)

INPUT Display             	*display;
INPUT int			fpConvert;
INPUT int			fpFormat;
INPUT unsigned long       	valueMask;
INPUT PEXRendererAttributes 	*values;
OUTPUT int			*sizeRet;
OUTPUT char			**listRet;

{
    register char	*pBuf;
    int			size;
    int			i, n;
    unsigned long	f;


    /*
     * Count the number of attributes being set, then allocate a
     * scratch buffer used to pack the attributes.  It's not worth
     * computing the exact amount of memory needed, so assume
     * worse case.
     */

    CountOnes (valueMask, n);
    size =  n * SIZEOF (CARD32) + 
	SIZEOF (pexNpcSubvolume) +
	SIZEOF (pexViewport) +
	SIZEOF (pexColorSpecifier);

    if (valueMask & PEXRAClipList)
    {
	size += (values->clip_list.count * SIZEOF (pexDeviceRect));
    }

    if (valueMask & PEXRAPickStartPath)
    {
	size += (values->pick_start_path.count * SIZEOF (pexElementRef));
    }

    pBuf = *listRet = (char *) _XAllocScratch (display, size);


    /*
     * Pack the attributes.
     */

    for (i = 0; i < (PEXRAMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
            switch (f)
	    {
	    case PEXRACurrentPath:

		/*
		 * Current path doesn't make sense in a new or changed
		 * renderer, so ignore it.
		 */
		break;

	    case PEXRAPipelineContext:

		STORE_CARD32 (values->pipeline_context, pBuf);
		break;

	    case PEXRAMarkerBundle:

		STORE_CARD32 (values->marker_bundle, pBuf);
		break;

	    case PEXRATextBundle:

		STORE_CARD32 (values->text_bundle, pBuf);
		break;

	    case PEXRALineBundle:

		STORE_CARD32 (values->line_bundle, pBuf);
		break;

	    case PEXRAInteriorBundle:

		STORE_CARD32 (values->interior_bundle, pBuf);
		break;

	    case PEXRAEdgeBundle:

		STORE_CARD32 (values->edge_bundle, pBuf);
		break;

	    case PEXRAViewTable:

		STORE_CARD32 (values->view_table, pBuf);
		break;

	    case PEXRAColorTable:

		STORE_CARD32 (values->color_table, pBuf);
		break;

	    case PEXRADepthCueTable:

		STORE_CARD32 (values->depth_cue_table, pBuf);
		break;

	    case PEXRALightTable:

		STORE_CARD32 (values->light_table, pBuf);
		break;

	    case PEXRAColorApproxTable:

		STORE_CARD32 (values->color_approx_table, pBuf);
		break;

	    case PEXRAPatternTable:

		STORE_CARD32 (values->pattern_table, pBuf);
		break;

	    case PEXRATextFontTable:

		STORE_CARD32 (values->text_font_table, pBuf);
		break;

	    case PEXRAHighlightIncl:

		STORE_CARD32 (values->highlight_incl, pBuf);
		break;

	    case PEXRAHighlightExcl:

		STORE_CARD32 (values->highlight_excl, pBuf);
		break;

	    case PEXRAInvisibilityIncl:

		STORE_CARD32 (values->invisibility_incl, pBuf);
		break;

	    case PEXRAInvisibilityExcl:

		STORE_CARD32 (values->invisibility_excl, pBuf);
		break;

	    case PEXRARendererState:

		/*
		 * Renderer state doesn't make sense in a new or changed
		 * renderer, so ignore it.
		 */
		break;

	    case PEXRAHLHSRMode:

		STORE_CARD32 (values->hlhsr_mode, pBuf);
		break;

	    case PEXRANPCSubVolume:

		STORE_NPC_SUBVOLUME (values->npc_subvolume, pBuf,
		    fpConvert, fpFormat);
		break;

	    case PEXRAViewport:

		STORE_VIEWPORT (values->viewport, pBuf, fpConvert, fpFormat);
		break;

	    case PEXRAClipList:

		STORE_CARD32 (values->clip_list.count, pBuf);

		STORE_LISTOF_DEVRECT (values->clip_list.count,
		    values->clip_list.rectangles, pBuf);
		break;

	    case PEXRAPickIncl:

		STORE_CARD32 (values->pick_incl, pBuf);
		break;

	    case PEXRAPickExcl:

		STORE_CARD32 (values->pick_excl, pBuf);
		break;

	    case PEXRAPickStartPath:

		STORE_CARD32 (values->pick_start_path.count, pBuf);

		STORE_LISTOF_ELEMREF (values->pick_start_path.count,
		    values->pick_start_path.elements, pBuf);
		break;

	    case PEXRABackgroundColor:

		STORE_COLOR_SPEC (values->background_color, pBuf,
		    fpConvert, fpFormat);
		break;

	    case PEXRAClearImage:

		STORE_CARD32 (values->clear_image, pBuf);
		break;

	    case PEXRAClearZ:

		STORE_CARD32 (values->clear_z, pBuf);
		break;

	    case PEXRAEchoMode:

		STORE_CARD32 (values->echo_mode, pBuf);
		break;
	    }
	}
    }

    *sizeRet = pBuf - *listRet;
}
