/* $Xorg: ddContext.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.

			All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems
not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level2/ddContext.c,v 3.7 2001/12/14 19:57:19 dawes Exp $ */

#include "miWks.h"
#include "miStruct.h"
#include "miRender.h"
#include "PEXErr.h"
#include "pexUtils.h"
#include "pixmap.h"
#include "windowstr.h"
#include "regionstr.h"
#include "miscstruct.h"
#include "pexos.h"


static void     deleteDynamicDDContext();

/* External variables used */

extern void     miMatMult();
extern RendTableType RenderPrimitiveTable[];
extern RendTableType PickPrimitiveTable[];

/* pcflag is initialized in  ddpexInit() */
extern ddBOOL   pcflag;
extern ddPCAttr defaultPCAttr;
#define MI_GET_DEFAULT_PC(pPC)  \
        if (!pcflag) {          \
                DefaultPC(pPC); \
                pcflag = MI_TRUE;  }

/*++
 |
 |  Function Name:	CreateDDContext
 |
 |  Function Description:
 |	 Creates and initializes a DDContext structure and
 |       intializes the Renderer pointer.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
CreateDDContext(pRend)
    ddRendererPtr   pRend;	/* renderer handle */
{

    extern GCPtr    CreateScratchGC();
    miDDContext    *pddc;
    ddPCAttr       *ppca;
    listofObj      *pMC, *pLS;
    int             i;

    pRend->pDDContext = NULL;

    /*
     * Create the dd attribute context: this requires creating both the
     * static and Dynamic portions of the DDContext.
     */
    pddc = (miDDContext *) xalloc(sizeof(miDDContext));
    if (!pddc)
	return (BadAlloc);
    pddc->Dynamic = (miDynamicDDContext *) xalloc(sizeof(miDynamicDDContext));
    if (!pddc->Dynamic) {
	xfree((char *) pddc);
	return (BadAlloc);
    }
    /* allocate storage for local copy of PC */
    pddc->Dynamic->pPCAttr = (ddPCAttr *) xalloc(sizeof(ddPCAttr));
    if (!pddc->Dynamic->pPCAttr) {
	xfree((char *) pddc->Dynamic);
	xfree((char *) pddc);
	return (BadAlloc);
    }

    /* initialize pointers so that panic-caused freeing is clean */
    pddc->Static.misc.ms_MCV = NULL;
    pddc->Static.attrs = NULL;
    pddc->Dynamic->next = NULL;
    pddc->Dynamic->pPCAttr->modelClipVolume = NULL;
    pddc->Dynamic->pPCAttr->lightState = NULL;
    pddc->Static.misc.pPolylineGC = NULL;
    pddc->Static.misc.pFillAreaGC = NULL;
    pddc->Static.misc.pEdgeGC = NULL;
    pddc->Static.misc.pPolyMarkerGC = NULL;
    pddc->Static.misc.pTextGC = NULL;


    /* don't forget Model Clip and lightState in PC */
    pddc->Dynamic->pPCAttr->modelClipVolume = puCreateList(DD_HALF_SPACE);
    if (!pddc->Dynamic->pPCAttr->modelClipVolume) {
	DeleteDDContext(pddc);
	return (BadAlloc);
    }

    pddc->Dynamic->pPCAttr->lightState = puCreateList(DD_INDEX);
    if (!pddc->Dynamic->pPCAttr->lightState) {
	DeleteDDContext(pddc);
	return (BadAlloc);
    }

    /* don't forget transformed versions of MCVolume in PC */
    pddc->Static.misc.ms_MCV = puCreateList(DD_HALF_SPACE);
    if (!pddc->Static.misc.ms_MCV) {
	DeleteDDContext(pddc);
	return (BadAlloc);
    }

    /*
     * Initialize the newly created ddcontext.
     */
    ppca = pddc->Dynamic->pPCAttr;
    pMC = ppca->modelClipVolume;
    pLS = ppca->lightState;
    if (pRend->pPC != NULL) {
	*ppca = *pRend->pPC->pPCAttr;

	/*
	 * don't forget the model clip half planes and list of light sources,
	 * which are only pointed to
	 */
	if (puCopyList(pRend->pPC->pPCAttr->modelClipVolume, pMC)) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (puCopyList(pRend->pPC->pPCAttr->lightState, pLS)) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
    } else {			/* use default PC values */
	MI_GET_DEFAULT_PC(&defaultPCAttr);
	*ppca = defaultPCAttr;

	/*
	 * don't forget the model clip half planes and list of light sources,
	 * which are only pointed to
	 */
	if (puCopyList(defaultPCAttr.modelClipVolume, pMC)) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (puCopyList(defaultPCAttr.lightState, pLS)) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
    }
    ppca->modelClipVolume = pMC;
    ppca->lightState = pLS;

    /* copy the current name set from the ns resource to the renderer */
    MINS_EMPTY_NAMESET(pddc->Dynamic->currentNames);
    if (ppca->pCurrentNS) {
	miNSHeader     *pns = (miNSHeader *) ppca->pCurrentNS->deviceData;

	MINS_COPY_NAMESET(pns->names, pddc->Dynamic->currentNames);
    }
    pddc->Dynamic->filter_flags = 0;
    pddc->Dynamic->do_prims = 1;

    /*
     * Initialize level 1 rendering procedure jump table
     */
    memcpy( (char *) pddc->Static.RenderProcs,
	  (char *) RenderPrimitiveTable,
	  sizeof(RendTableType) * RENDER_TABLE_LENGTH);

    /*
     * Allocate storage and initialize the DD context rendering attributes.
     */
    if (!(pddc->Static.attrs =
	  (miDDContextRendAttrs *) xalloc(sizeof(miDDContextRendAttrs)))) {
	DeleteDDContext(pddc);
	return (BadAlloc);
    }

    /*
     * Intialize the scratch data pointer areas Setting the maxlists or
     * maxData fields to 0 insures that headers are allocated the first time
     * the lists are used.
     */
    pddc->Static.misc.listIndex = 0;
    for (i = 0; i < MI_MAXTEMPDATALISTS; i++)
	pddc->Static.misc.list4D[i].maxLists = 0;
    pddc->Static.misc.list2D.maxLists = 0;

    for (i = 0; i < MI_MAXTEMPFACETLISTS; i++)
	pddc->Static.misc.facets[i].maxData = 0;

    /*
     * get a GC  use the default values for now a scratch gc should be OK,
     * it's the same as a regular gc except it doesn't create the tile or
     * stipple I don't think we need to express interest in any change to the
     * GC, since the ddpex code which uses the gc is the only one who should
     * be changing it. But, we may need to express interest in window events
     * or something if we can.
     * 
     * a GC is created for each primitive type. THis should reduce the number of
     * calls to change GC as otherwise this call would be required for each
     * primitive. Note, that the down-side of this approach is that many
     * hardware platforms only support a single graphics context, and thus
     * will have to be re-loaded at every validate GC anyways.... There is no
     * drawable if doing a search (ISS), so don't create the GCs. They aren't
     * needed.
     */

    if (pRend->pDrawable) {
	pddc->Static.misc.flags |= (POLYLINEGCFLAG | FILLAREAGCFLAG |
				    EDGEGCFLAG | MARKERGCFLAG | TEXTGCFLAG |
				    NOLINEDASHFLAG);

	if (!(pddc->Static.misc.pPolylineGC =
	      CreateScratchGC(pRend->pDrawable->pScreen, pRend->pDrawable->depth))) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (!(pddc->Static.misc.pFillAreaGC =
	      CreateScratchGC(pRend->pDrawable->pScreen, pRend->pDrawable->depth))) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (!(pddc->Static.misc.pEdgeGC =
	    CreateScratchGC(pRend->pDrawable->pScreen, pRend->pDrawable->depth))) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (!(pddc->Static.misc.pPolyMarkerGC =
	      CreateScratchGC(pRend->pDrawable->pScreen, pRend->pDrawable->depth))) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
	if (!(pddc->Static.misc.pTextGC =
	      CreateScratchGC(pRend->pDrawable->pScreen, pRend->pDrawable->depth))) {
	    DeleteDDContext(pddc);
	    return (BadAlloc);
	}
    }
    /* init pick and search structs */
    pddc->Static.pick.type = 0;
    pddc->Static.pick.status = PEXNoPick;

    MINS_EMPTY_NAMESET(pddc->Static.pick.inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.pick.exclusion);

    pddc->Static.search.status = PEXNotFound;

    MINS_EMPTY_NAMESET(pddc->Static.search.norm_inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.norm_exclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.invert_inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.invert_exclusion);

    /* Indicate all xforms in static are invalid  */
     pddc->Static.misc.flags |= ( INVTRCCTODCXFRMFLAG | INVTRWCTOCCXFRMFLAG 
     | INVTRMCTOCCXFRMFLAG | INVTRMCTOWCXFRMFLAG | INVVIEWXFRMFLAG );
      
    /* Mark as invalid any transform dependant fields in ddContext
     */
    pddc->Static.misc.flags |= (MCVOLUMEFLAG | CC_DCUEVERSION);

    /* If successfull, initialize Renderer pointer and return */
    pRend->pDDContext = (ddPointer) pddc;
    return (Success);
}

/*++
 |
 |  Function Name:	DeleteDDContext
 |
 |  Function Description:
 |	 Deletes all of the storage used by the ddPEX attribute context.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
DeleteDDContext(pDDContext)
/* in */
    miDDContext    *pDDContext;	/* ddPEX attribute structure */
/* out */
{
    miDynamicDDContext *pdddc, *pdddc2;
    int             i;

#ifdef DDTEST
    ErrorF(" DeleteDDContext\n");
#endif

    if (!pDDContext) return(Success);	/* just in case */

    /* Free the ddcontext attribute store */
    if (pDDContext->Static.attrs) {
	xfree((char *) (pDDContext->Static.attrs));
	pDDContext->Static.attrs = 0;
    }

    /*
     * free the scratch rendering data buffers.
     */
    for (i = 0; i < MI_MAXTEMPDATALISTS; i++) {
	MI_FREELISTHEADER(&(pDDContext->Static.misc.list4D[i]));
    }
    MI_FREELISTHEADER(&pDDContext->Static.misc.list2D);

    for (i = 0; i < MI_MAXTEMPFACETLISTS; i++)
	if (pDDContext->Static.misc.facets[i].maxData) {
	    xfree((char *)(pDDContext->Static.misc.facets[i].facets.pNoFacet));
	    pDDContext->Static.misc.facets[i].facets.pNoFacet = 0;
	}

    if (pDDContext->Static.misc.ms_MCV) {
	puDeleteList(pDDContext->Static.misc.ms_MCV);
	pDDContext->Static.misc.ms_MCV = 0;
    }

    /*
     * Free the graphics contexts.
     */
    if (pDDContext->Static.misc.pPolylineGC)
	FreeScratchGC(pDDContext->Static.misc.pPolylineGC);

    if (pDDContext->Static.misc.pFillAreaGC)
	FreeScratchGC(pDDContext->Static.misc.pFillAreaGC);

    if (pDDContext->Static.misc.pEdgeGC)
	FreeScratchGC(pDDContext->Static.misc.pEdgeGC);

    if (pDDContext->Static.misc.pPolyMarkerGC)
	FreeScratchGC(pDDContext->Static.misc.pPolyMarkerGC);

    if (pDDContext->Static.misc.pTextGC)
	FreeScratchGC(pDDContext->Static.misc.pTextGC);

    /* free the Dynamic part(s) */
    pdddc = pDDContext->Dynamic;
    while (pdddc) {
	pdddc2 = pdddc->next;
	deleteDynamicDDContext(pdddc);
	pdddc = pdddc2;
    }

    /* zero pointers to force illumination of any server bugs */
    pDDContext->Static.misc.ms_MCV = NULL;
    pDDContext->Static.attrs = NULL;
    pDDContext->Static.misc.pPolylineGC = NULL;
    pDDContext->Static.misc.pFillAreaGC = NULL;
    pDDContext->Static.misc.pEdgeGC = NULL;
    pDDContext->Static.misc.pPolyMarkerGC = NULL;
    pDDContext->Static.misc.pTextGC = NULL;

    pDDContext->Dynamic = NULL;

    /* Lastly, free the ddcontext pointer itself.... */
    xfree((char *) (pDDContext));

    return (Success);
}				/* DeleteDDContext */

/*++
 |
 |  Function Name:	deleteDynamicDDContext
 |
 |  Function Description:
 |	 Deletes all of the storage used by the DynamicDDContext
 |       attribute context.
 |
 |  Note(s):
 |
 --*/

static
void
deleteDynamicDDContext(pdddc)
/* in */
    miDynamicDDContext *pdddc;
{
    /* delete the dynamic part of the DDContext */

    if (!pdddc) return;

    /* delete the pc attributes */
    if (pdddc->pPCAttr) {
	/* model clip volume */
	if (pdddc->pPCAttr->modelClipVolume) {
	    puDeleteList(pdddc->pPCAttr->modelClipVolume);
	    pdddc->pPCAttr->modelClipVolume = NULL; 
	}
	/* list of light source indices */
	if (pdddc->pPCAttr->lightState) {
	    puDeleteList(pdddc->pPCAttr->lightState);
	    pdddc->pPCAttr->lightState = NULL;
	}
	/* pc attr */
	xfree((char *) pdddc->pPCAttr);
	pdddc->pPCAttr = NULL;
    }
    xfree((char *) pdddc);
}				/* deleteDynamicDDContext */

/*++
 |
 |  Function Name:	PushddContext
 |
 |  Function Description:
 |	Pushes an instance of the ddContext on the stack, and creates
 |	a new "current" copy.
 |
 |	The ddContext is divided into two parts: static and dynamic.
 |	The static attributes of the ddContext are either invarient
 |	across CSS hierarchy levels, or must be recomputed when moving
 |	up a level in the hierarchy. Dybnamic attributes, on the other
 |	hand, are stored in the stack when decending down the CSS, and
 |	thus can be restored by a simple "pop" operation when climbing
 |	back up through the CSS hierarchy.
 |
 |	This routine, therefore, saves the current dynamic ddContext
 |	attributes on the stack, and initializes a new dynamic ddContext.
 |
 |  Note(s):
 |      Since some of the dynamic ddContext elements contain and/or
 |	are pointers to objects, the info cannot be copied directly,
 |	but new objects must be made to be pointed to and their contents copied.
 |      So, the sequence that this procedure goes through is this:
 |	create a new dd context data structure
 |	copy old context to the new context, but remember
 |		that pointers to objects will be replaced.
 |	create a new PCAttr structure and copy old to new
 |	update the current path and transform matrices
 |	push the new context onto the stack
 |
 --*/

ddpex3rtn
PushddContext(pRend)
/* in */
    ddRendererPtr   pRend;	/* renderer handle */
/* out */
{
    miDDContext    *pDDContext = (miDDContext *) (pRend->pDDContext);
    miDynamicDDContext *oldDContext = pDDContext->Dynamic;
    miDynamicDDContext *newDContext;

#ifdef DDTEST
    ErrorF(" BeginStructure %d\n", sId);
#endif

    /* First, create a new dynamic dd context  */
    if (!(newDContext = (miDynamicDDContext *) xalloc(sizeof(miDynamicDDContext)))) {
	return (BadAlloc);
    }

    /*
     * copy the contents of the old Dynamic ddContext to the new one this
     * also copies the current name set
     */
    *newDContext = *oldDContext;

    /* now, create a new PC for the new dd context */
    if (!(newDContext->pPCAttr = (ddPCAttr *) xalloc(sizeof(ddPCAttr)))) {
	xfree((char *) newDContext);
	return (BadAlloc);
    }
    /* Copy static portion of PC attrs */
    *newDContext->pPCAttr = *oldDContext->pPCAttr;

    /* now create new stuff for pointers in new PCAttr to point to */
    /* model clip volume */
    newDContext->pPCAttr->modelClipVolume = puCreateList(DD_HALF_SPACE);
    if (!newDContext->pPCAttr->modelClipVolume) {
	deleteDynamicDDContext(newDContext);
	return (BadAlloc);
    }
    if (puCopyList(oldDContext->pPCAttr->modelClipVolume,
		   newDContext->pPCAttr->modelClipVolume)) {
	deleteDynamicDDContext(newDContext);
	return (BadAlloc);
    }
    /* light source indices */
    newDContext->pPCAttr->lightState = puCreateList(DD_INDEX);
    if (!newDContext->pPCAttr->lightState) {
	deleteDynamicDDContext(newDContext);
	return (BadAlloc);
    }
    if (puCopyList(oldDContext->pPCAttr->lightState,
		   newDContext->pPCAttr->lightState)) {
	deleteDynamicDDContext(newDContext);
	return (BadAlloc);
    }
    /** Concatenate the local and global transforms into the new
     ** global transform, then identity out the local one **/
    miMatMult(newDContext->pPCAttr->globalMat,
	      oldDContext->pPCAttr->localMat,
	      oldDContext->pPCAttr->globalMat);

    memcpy( (char *) newDContext->pPCAttr->localMat, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    /** Push the new context onto the renderer DDContext **/
    /* newContext points to the old context */
    newDContext->next = pDDContext->Dynamic;
    /* renderer points to the new context */
    pDDContext->Dynamic = newDContext;

    return (Success);
}				/* PushddContext */

/*++
 |
 |  Function Name:	PopddContext
 |
 |  Function Description:
 |	Pops an instance of a dynamic ddContext structure off the stack.
 |	(See description above.)
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
PopddContext(pRend)
/* in */
    ddRendererPtr   pRend;	/* renderer handle */
/* out */
{
    miDDContext    *pddc = (miDDContext *) pRend->pDDContext;
    miDynamicDDContext *oldDDDContext;

#ifdef DDTEST
    ErrorF(" EndStructure\n");
#endif

    /** Pop off top of attr context stack **/
    oldDDDContext = pddc->Dynamic;
    pddc->Dynamic = oldDDDContext->next;

    /** Free up storage used by oldContext **/
    deleteDynamicDDContext(oldDDDContext);

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG |
				INVTRMCTOCCXFRMFLAG | INVTRCCTODCXFRMFLAG |
				INVVIEWXFRMFLAG);

    /* Mark as invalid any transform dependant fields in ddContext */
    pddc->Static.misc.flags |= (MCVOLUMEFLAG | CC_DCUEVERSION);

    return (Success);

}				/* PopddContext */

/*++
 |
 |  Function Name:	ValidateDDContextAttrs
 |
 |  Function Description:
 |	 Updates the rendering attributes to match the
 |	 attributes associated with the current PC.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs)
    ddRendererPtr   pRend;	/* renderer handle */
    miDDContext    *pddc;	/* ddPEX attribute structure */
    ddBitmask       tables, namesets, attrs;
{
    ddpex3rtn       miConvertColor();
    miLineBundleEntry *linebundle = 0;
    miTextBundleEntry *textbundle = 0;
    miMarkerBundleEntry *markerbundle = 0;
    miInteriorBundleEntry *intbundle = 0;
    miEdgeBundleEntry *edgebundle = 0;
    miViewEntry    *view = 0;
    ddUSHORT        status = 0;
    char            colors;

    colors = (tables & PEXDynColourTable) || (tables & PEXDynColourTableContents);

    /*
     * Marker Attributes
     */
    if ((tables & PEXDynMarkerBundle) ||
	(tables & PEXDynMarkerBundleContents) ||
	colors) {
	if (~(pddc->Dynamic->pPCAttr->asfs
	   & (PEXMarkerTypeAsf | PEXMarkerScaleAsf | PEXMarkerColourAsf))) {

	    if ((InquireLUTEntryAddress(PEXMarkerBundleLUT,
					pRend->lut[PEXMarkerBundleLUT],
					pddc->Dynamic->pPCAttr->markerIndex,
					&status, (ddPointer *) (&markerbundle)))
		== PEXLookupTableError)
		return (PEXLookupTableError);
	}
	/* always try to set the color */
	if (!MI_DDC_IS_HIGHLIGHT(pddc)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &markerbundle->real_entry.markerColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->markerColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->markerColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->markerColour);
	    }
	}
	/* only set these if it's the bundle that changed */
	if ((tables & PEXDynMarkerBundle) ||
	    (tables & PEXDynMarkerBundleContents)) {

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerTypeAsf) == PEXBundled)
		pddc->Static.attrs->markerType = markerbundle->real_entry.markerType;
	    else
		pddc->Static.attrs->markerType = pddc->Dynamic->pPCAttr->markerType;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXMarkerScaleAsf) == PEXBundled)
		pddc->Static.attrs->markerScale =
		    markerbundle->real_entry.markerScale;
	    else
		pddc->Static.attrs->markerScale =
		    pddc->Dynamic->pPCAttr->markerScale;

	}
	pddc->Static.misc.flags |= MARKERGCFLAG;
    }

    /*
     * Text Attributes
     */
    if ((tables & PEXDynTextBundle) ||
	(tables & PEXDynTextBundleContents) ||
	colors) {
	if (~(pddc->Dynamic->pPCAttr->asfs
	      & (PEXTextFontIndexAsf | PEXTextPrecAsf | PEXCharExpansionAsf |
		 PEXCharSpacingAsf | PEXTextColourAsf))) {

	    if ((InquireLUTEntryAddress(PEXTextBundleLUT,
					pRend->lut[PEXTextBundleLUT],
					pddc->Dynamic->pPCAttr->textIndex,
				      &status, (ddPointer *) (&textbundle)))
		== PEXLookupTableError)
		return (PEXLookupTableError);
	}
	/* First, bundled attributes */
	/* always try to set the color */
	if (!MI_DDC_IS_HIGHLIGHT(pddc)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXTextColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &textbundle->real_entry.textColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->textColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->textColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->textColour);
	    }
	}
	/* only set these if it's the bundle that changed */
	if ((tables & PEXDynTextBundle) ||
	    (tables & PEXDynTextBundleContents)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXTextFontIndexAsf) == PEXBundled)
		pddc->Static.attrs->textFont = textbundle->real_entry.textFontIndex;
	    else
		pddc->Static.attrs->textFont = pddc->Dynamic->pPCAttr->textFont;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXTextPrecAsf) == PEXBundled)
		pddc->Static.attrs->textPrecision =
		    textbundle->real_entry.textPrecision;
	    else
		pddc->Static.attrs->textPrecision =
		    pddc->Dynamic->pPCAttr->textPrecision;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXCharExpansionAsf) == PEXBundled)
		pddc->Static.attrs->charExpansion =
		    textbundle->real_entry.charExpansion;
	    else
		pddc->Static.attrs->charExpansion =
		    pddc->Dynamic->pPCAttr->charExpansion;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXCharSpacingAsf) == PEXBundled)
		pddc->Static.attrs->charSpacing = textbundle->real_entry.charSpacing;
	    else
		pddc->Static.attrs->charSpacing = pddc->Dynamic->pPCAttr->charSpacing;
	}
    }

    /*
     * Next, unbundled attributes  always do these, but there may be some way
     * to put some smarts in about this
     */
    pddc->Static.attrs->charHeight = pddc->Dynamic->pPCAttr->charHeight;
    pddc->Static.attrs->charUp = pddc->Dynamic->pPCAttr->charUp;
    pddc->Static.attrs->textPath = pddc->Dynamic->pPCAttr->textPath;
    pddc->Static.attrs->textAlignment = pddc->Dynamic->pPCAttr->textAlignment;
    pddc->Static.attrs->atextHeight = pddc->Dynamic->pPCAttr->atextHeight;
    pddc->Static.attrs->atextUp = pddc->Dynamic->pPCAttr->atextUp;
    pddc->Static.attrs->atextPath = pddc->Dynamic->pPCAttr->atextPath;
    pddc->Static.attrs->atextAlignment = pddc->Dynamic->pPCAttr->atextAlignment;
    pddc->Static.attrs->atextStyle = pddc->Dynamic->pPCAttr->atextStyle;

    pddc->Static.misc.flags |= TEXTGCFLAG;

    /*
     * Line Attributes
     */
    if ((tables & PEXDynLineBundle) ||
	(tables & PEXDynLineBundleContents) ||
	colors) {
	if (~(pddc->Dynamic->pPCAttr->asfs
	      & (PEXLineTypeAsf | PEXLineWidthAsf | PEXLineColourAsf |
		 PEXCurveApproxAsf | PEXPolylineInterpAsf))) {
	    if ((InquireLUTEntryAddress(PEXLineBundleLUT,
					pRend->lut[PEXLineBundleLUT],
					pddc->Dynamic->pPCAttr->lineIndex,
				      &status, (ddPointer *) (&linebundle)))
		== PEXLookupTableError)
		return (PEXLookupTableError);
	}
	/* Update DDC rendering attributes if bundled */
	/* always try to set the color */
	if (!MI_DDC_IS_HIGHLIGHT(pddc)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &linebundle->real_entry.lineColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->lineColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->lineColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->lineColour);
	    }
	}
	/* only set these if it's the bundle that changed */
	if ((tables & PEXDynLineBundle) ||
	    (tables & PEXDynLineBundleContents)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineTypeAsf) == PEXBundled)
		pddc->Static.attrs->lineType = linebundle->real_entry.lineType;
	    else
		pddc->Static.attrs->lineType = pddc->Dynamic->pPCAttr->lineType;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXLineWidthAsf) == PEXBundled)
		pddc->Static.attrs->lineWidth = linebundle->real_entry.lineWidth;
	    else
		pddc->Static.attrs->lineWidth = pddc->Dynamic->pPCAttr->lineWidth;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXCurveApproxAsf) == PEXBundled)
		pddc->Static.attrs->curveApprox = linebundle->real_entry.curveApprox;
	    else
		pddc->Static.attrs->curveApprox = pddc->Dynamic->pPCAttr->curveApprox;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXPolylineInterpAsf) == PEXBundled)
		pddc->Static.attrs->lineInterp = linebundle->real_entry.polylineInterp;
	    else
		pddc->Static.attrs->lineInterp = pddc->Dynamic->pPCAttr->lineInterp;
	}
	pddc->Static.misc.flags |= POLYLINEGCFLAG;
    }

    /*
     * Surface Attributes
     */

    if ((tables & PEXDynInteriorBundle) ||
	(tables & PEXDynInteriorBundleContents) ||
	colors) {
	if (~(pddc->Dynamic->pPCAttr->asfs
	      & (PEXInteriorStyleAsf | PEXInteriorStyleIndexAsf |
		 PEXSurfaceColourAsf | PEXSurfaceInterpAsf |
		 PEXReflectionModelAsf | PEXReflectionAttrAsf |
		 PEXBfInteriorStyleAsf | PEXBfInteriorStyleIndexAsf |
		 PEXBfSurfaceColourAsf | PEXBfSurfaceInterpAsf |
		 PEXBfReflectionModelAsf | PEXBfReflectionAttrAsf |
		 PEXSurfaceApproxAsf))) {
	    if ((InquireLUTEntryAddress(PEXInteriorBundleLUT,
					pRend->lut[PEXInteriorBundleLUT],
					pddc->Dynamic->pPCAttr->intIndex,
					&status, (ddPointer *) (&intbundle)))
		== PEXLookupTableError)
		return (PEXLookupTableError);
	}
	/* Update DDC rendering attributes if bundled */
	/* always try to set the color */
	if (!MI_DDC_IS_HIGHLIGHT(pddc)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &intbundle->real_entry.surfaceColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->surfaceColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->surfaceColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->surfaceColour);
	    }
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXBfSurfaceColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &intbundle->real_entry.bfSurfaceColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->bfSurfColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->bfSurfColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->bfSurfColour);
	    }

	}
	/* only set these if it's the bundle that changed */
	if ((tables & PEXDynInteriorBundle) ||
	    (tables & PEXDynInteriorBundleContents)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceInterpAsf) 
								== PEXBundled)
		pddc->Static.attrs->surfInterp = 
					intbundle->real_entry.surfaceInterp;
	    else
		pddc->Static.attrs->surfInterp = 
					pddc->Dynamic->pPCAttr->surfInterp;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXReflectionModelAsf) 
								== PEXBundled)
		pddc->Static.attrs->reflModel = 
					intbundle->real_entry.reflectionModel;
	    else
		pddc->Static.attrs->reflModel = 
					pddc->Dynamic->pPCAttr->reflModel;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXReflectionAttrAsf) 
								== PEXBundled){
		pddc->Static.attrs->reflAttr = 
					intbundle->real_entry.reflectionAttr;
  		miConvertColor(pRend,
			&intbundle->real_entry.reflectionAttr.specularColour,
  			pddc->Dynamic->pPCAttr->rdrColourModel,
  			&pddc->Static.attrs->reflAttr.specularColour);
  	    } else {
		pddc->Static.attrs->reflAttr = pddc->Dynamic->pPCAttr->reflAttr;
  		miConvertColor(pRend,
  			&pddc->Dynamic->pPCAttr->reflAttr.specularColour,
  			pddc->Dynamic->pPCAttr->rdrColourModel,
  			&pddc->Static.attrs->reflAttr.specularColour);
  	    }


	    if ((pddc->Dynamic->pPCAttr->asfs & PEXInteriorStyleAsf) 
								== PEXBundled)
		pddc->Static.attrs->intStyle = 
					intbundle->real_entry.interiorStyle;
	    else
		pddc->Static.attrs->intStyle = pddc->Dynamic->pPCAttr->intStyle;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXBfSurfaceInterpAsf) 
								== PEXBundled)
		pddc->Static.attrs->bfSurfInterp =
		    			intbundle->real_entry.bfSurfaceInterp;
	    else
		pddc->Static.attrs->bfSurfInterp =
		    pddc->Dynamic->pPCAttr->bfSurfInterp;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXBfReflectionModelAsf) 
								== PEXBundled)
		pddc->Static.attrs->bfReflModel =
		    			intbundle->real_entry.bfReflectionModel;
	    else
		pddc->Static.attrs->bfReflModel = 
					pddc->Dynamic->pPCAttr->bfReflModel;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXBfReflectionAttrAsf) 
								== PEXBundled){
		pddc->Static.attrs->bfReflAttr =
		    			intbundle->real_entry.bfReflectionAttr;
  		miConvertColor(pRend,
 			&intbundle->real_entry.bfReflectionAttr.specularColour,
  			pddc->Dynamic->pPCAttr->rdrColourModel,
  			&pddc->Static.attrs->bfReflAttr.specularColour);
  	    } else {
		pddc->Static.attrs->bfReflAttr = 
					pddc->Dynamic->pPCAttr->bfReflAttr;
  		miConvertColor(pRend,
  			&pddc->Dynamic->pPCAttr->bfReflAttr.specularColour,
  			pddc->Dynamic->pPCAttr->rdrColourModel,
  			&pddc->Static.attrs->bfReflAttr.specularColour);
  	    }


	    if ((pddc->Dynamic->pPCAttr->asfs & PEXBfInteriorStyleAsf) 
								== PEXBundled)
		pddc->Static.attrs->bfIntStyle = 
					intbundle->real_entry.bfInteriorStyle;
	    else
		pddc->Static.attrs->bfIntStyle = 
					pddc->Dynamic->pPCAttr->bfIntStyle;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceApproxAsf) 
								== PEXBundled)
		pddc->Static.attrs->surfApprox = 
					intbundle->real_entry.surfaceApprox;
	    else
		pddc->Static.attrs->surfApprox = 
					pddc->Dynamic->pPCAttr->surfApprox;
	}
	pddc->Static.misc.flags |= FILLAREAGCFLAG;
    }

    /*
     * Surface edge Attributes
     */
    if ((tables & PEXDynEdgeBundle) ||
	(tables & PEXDynEdgeBundleContents) ||
	colors) {
	if (~(pddc->Dynamic->pPCAttr->asfs
	      & (PEXSurfaceEdgeTypeAsf | PEXSurfaceEdgeWidthAsf |
		 PEXSurfaceEdgeColourAsf | PEXSurfaceEdgesAsf))) {
	    if ((InquireLUTEntryAddress(PEXEdgeBundleLUT,
					pRend->lut[PEXEdgeBundleLUT],
					pddc->Dynamic->pPCAttr->edgeIndex,
				      &status, (ddPointer *) (&edgebundle)))
		== PEXLookupTableError)
		return (PEXLookupTableError);
	}
	/* always try to set the color */
	if (!MI_DDC_IS_HIGHLIGHT(pddc)) {

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgeColourAsf) == PEXBundled) {
		miConvertColor(pRend,
			       &edgebundle->real_entry.edgeColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->edgeColour);
	    } else {
		miConvertColor(pRend,
			       &pddc->Dynamic->pPCAttr->edgeColour,
			       pddc->Dynamic->pPCAttr->rdrColourModel,
			       &pddc->Static.attrs->edgeColour);
	    }
	}
	/* only set these if it's the bundle that changed */
	if ((tables & PEXDynEdgeBundle) ||
	    (tables & PEXDynEdgeBundleContents)) {
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgesAsf) == PEXBundled)
		pddc->Static.attrs->edges = edgebundle->real_entry.edges;
	    else
		pddc->Static.attrs->edges = pddc->Dynamic->pPCAttr->edges;
	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgeTypeAsf) == PEXBundled)
		pddc->Static.attrs->edgeType = edgebundle->real_entry.edgeType;
	    else
		pddc->Static.attrs->edgeType = pddc->Dynamic->pPCAttr->edgeType;

	    if ((pddc->Dynamic->pPCAttr->asfs & PEXSurfaceEdgeWidthAsf) == PEXBundled)
		pddc->Static.attrs->edgeWidth = edgebundle->real_entry.edgeWidth;
	    else
		pddc->Static.attrs->edgeWidth = pddc->Dynamic->pPCAttr->edgeWidth;
	}
	pddc->Static.misc.flags |= EDGEGCFLAG;
    }

    /*
     * View table
     */
    if ((attrs & PEXDynNpcSubvolume) || (attrs & PEXDynViewport)) {
	extern ddpex3rtn miBldViewport_xform();

	miBldViewport_xform(pRend, pRend->pDrawable, 
			    pddc->Static.misc.viewport_xform, pddc );
    }
    if ((tables & PEXDynViewTable) || (tables & PEXDynViewTableContents)) {
	extern ddpex3rtn miBldCC_xform();

	miBldCC_xform(pRend, pddc);
    }

    /* the echo colour change always take places */
    pddc->Static.attrs->echoColour = pRend->echoColour;

    /* If the echo mode changes, we have to change all these GCs */
    if( attrs & PEXDynEchoMode )
      {
	pddc->Static.attrs->echoMode = pRend->echoMode;
        pddc->Static.misc.flags |= POLYLINEGCFLAG;
        pddc->Static.misc.flags |= MARKERGCFLAG;
        pddc->Static.misc.flags |= FILLAREAGCFLAG;
        pddc->Static.misc.flags |= EDGEGCFLAG;
        pddc->Static.misc.flags |= TEXTGCFLAG;
      }

    /*
     * Set the Clip List in each GC if there are any
     * all GCs are defined at the same time so check if any one exists
     */
    if ((attrs & PEXDynClipList) && pddc->Static.misc.pPolylineGC) 
     {

      extern int      SetClipRects();
      extern void     ValidateGC();
      xRectangle      *xrects, *p;
      ddDeviceRect    *ddrects;
      ddLONG          numrects;
      XID	      gcval;
      int             i;

      numrects = pRend->clipList->numObj;
      if (numrects) {
        ddrects = (ddDeviceRect *) pRend->clipList->pList;
        xrects = (xRectangle*) xalloc(numrects * sizeof(xRectangle));
        if (!xrects) return BadAlloc;
        /* Need to convert to XRectangle format */
        for (i = 0, p = xrects; i < numrects; i++, p++, ddrects++) {
          p->x = ddrects->xmin;
          p->y = pRend->pDrawable->height - ddrects->ymax;
          p->width = ddrects->xmax - ddrects->xmin + 1;
          p->height = ddrects->ymax - ddrects->ymin + 1;
          }

        SetClipRects(pddc->Static.misc.pPolylineGC, 0, 0,
                     (int)numrects, xrects, Unsorted);
        SetClipRects(pddc->Static.misc.pFillAreaGC, 0, 0,
                     (int)numrects, xrects, Unsorted);
        SetClipRects(pddc->Static.misc.pEdgeGC, 0, 0,
                     (int)numrects, xrects, Unsorted);
        SetClipRects(pddc->Static.misc.pPolyMarkerGC, 0, 0,
                     (int)numrects, xrects, Unsorted);
        SetClipRects(pddc->Static.misc.pTextGC, 0, 0,
                     (int)numrects, xrects, Unsorted);
        xfree((char*)xrects);
      }
      else {
	gcval = None;
	ChangeGC(pddc->Static.misc.pPolylineGC, GCClipMask, &gcval);
	ChangeGC(pddc->Static.misc.pFillAreaGC, GCClipMask, &gcval);
	ChangeGC(pddc->Static.misc.pEdgeGC, GCClipMask, &gcval);
	ChangeGC(pddc->Static.misc.pPolyMarkerGC, GCClipMask, &gcval);
	ChangeGC(pddc->Static.misc.pTextGC, GCClipMask, &gcval);
      }
      ValidateGC(pRend->pDrawable, pddc->Static.misc.pPolylineGC);
      ValidateGC(pRend->pDrawable, pddc->Static.misc.pFillAreaGC);
      ValidateGC(pRend->pDrawable, pddc->Static.misc.pEdgeGC);
      ValidateGC(pRend->pDrawable, pddc->Static.misc.pPolyMarkerGC);
      ValidateGC(pRend->pDrawable, pddc->Static.misc.pTextGC);
    }

    return (Success);
}
