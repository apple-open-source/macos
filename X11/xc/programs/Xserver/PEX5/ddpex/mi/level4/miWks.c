/* $Xorg: miWks.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/miWks.c,v 1.10 2001/12/14 19:57:36 dawes Exp $ */


#include "miWks.h"
#include "miInfo.h"
#include "miLUT.h"
#include "pexUtils.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexExtract.h"
#include "Xprotostr.h"
#include "gcstruct.h"
#include "resource.h"
#include "pexos.h"


#ifdef MULTIBUFFER
#define _MULTIBUF_SERVER_
#include <X11/extensions/multibuf.h>
#endif

/*  Level 4 Workstation Support */
/* PHIGS Workstation Procedures */

extern ddpex4rtn    miDealWithStructDynamics();
extern ddpex4rtn    miDealWithDynamics();
extern void	    miMatInverse();
extern void	    miTransformPoint();
extern void	    path_update_struct_refs();
extern ddpex3rtn    miBldViewport_xform();

ddpex4rtn       mi_add_ord_view();
ddOrdView      *mi_find_ord_view();

extern miEnumType miHlhsrModeET[MI_MAXDRAWABLES][SI_HLHSR_NUM];
extern miEnumType miDisplayUpdateModeET[MI_MAXDRAWABLES][SI_UPDATE_NUM];

static void     deletewks();

/* init_pick_flag is initialized in ddpexInit()
 * there are MIWKS_NUM_PICK_DEVICES pick devices  (defined in miWks.h)
 * pick_devices is the initial state for each of these that
 * is used to initialize the pick devices in the wks
 * pick_devices have to be initialized dynamically because
 * there is a union in them
 */
ddBOOL          init_pick_flag;
void            initialize_pick_devices();
miPickDevice    pick_devices[MIWKS_NUM_PICK_DEVICES];

static ddNpcSubvolume NPCInit =
{{0.0, 0.0, 0.0},
{1.0, 1.0, 1.0}
};

static ddViewport viewportInit =
{{0, 0, 0.0},
{1, 1, 1.0},
MI_TRUE
};

/* ugly globals for this module */
static ddpex4rtn err4;
static ddpex43rtn err43;
static ddpex3rtn err3;
static int      err;

#define WKS_WINDOW_MASK 1
#define WKS_VIEWPORT_MASK 2

/* here are the dynamics that our wks can support */

/* only one so far */
ddBYTE          mi_dynamics[MI_MAXDRAWABLES][MAX_DYNAMIC] = {
	PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG,
	PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG, PEXIRG,
	PEXIRG, PEXIRG, PEXIRG
};

/* this macro checks to see if a change to the workstation affects
 * the current display
 */
#define WKSIMMEDIATE(pws, attr) \
		( (pws->displaySurface == PEXEmpty) || \
		((pws)->dynamics[(int)attr] == PEXIMM) )

/* adjust this for the supported dynamics & update modes in your implementation
 * this should be true if the change to the attribute is visualizable now
 */
#define VISUALIZE_NOW( pws, dynamic)	\
	(((pws)->dynamics[(int)(dynamic)] == PEXIMM) ||		\
		((pws)->displayUpdate == PEXVisualizeEach))

SetDoubleDrawable (pwks)
miWksPtr	pwks;
{
     /* This routine sets the drawable pointer. It does this with or
        without the MultiBuffer Extension
     */
 
     if (pwks->hasDoubleBuffer) 
     {
 	/* If you have the Double Buffer do whats right for the update mode */
 	switch (pwks->displayUpdate) {
 	case PEXVisualizeEach:
 	case PEXVisualizeWhenever:
 	case PEXVisualizeNone:
 	    pwks->pCurDrawable = pwks->doubleDrawables[pwks->curDoubleBuffer];
 	    pwks->usingDoubleBuffer = MI_TRUE;
 	    break;
 	case PEXSimulateSome:
 	case PEXVisualizeEasy:
 	    pwks->pCurDrawable = pwks->pRend->pDrawable;
 	    pwks->usingDoubleBuffer = MI_FALSE;
 	    break;
 	}
     }
     else 
     {
        /* No Double Buffer so use the renderers drawable */
 	pwks->usingDoubleBuffer = MI_FALSE;
 	pwks->pCurDrawable = pwks->pRend->pDrawable;
     }
}
 
ChangeDoubleBuffers (pwks)
miWksPtr	pwks;
{
 
     /*
      * This routine does the actual work to create or delete the Double
      * Buffers.  It is a no-op if there is no MultiBuffer Extension or the
      * drawable isn't a window, except it still has to SetDoubleBuffers to
      * insure the right drawable gets used
      */
 
#ifdef MULTIBUFFER
 
     if (pwks->pRend->pDrawable->type == DRAWABLE_WINDOW)
     if (pwks->curBufferMode == PEXDoubleBuffered && !pwks->hasDoubleBuffer)
     {
 	/* create the Double Buffers */
 	int	i;
 	int	client;
 	int	result;
 	XID	ids[2];
 	extern DrawablePtr  GetBufferPointer ();
 
 	client = CLIENT_ID(pwks->pRend->pDrawable->id);
 	for (i = 0; i < 2; i++)
 	    ids[i] = FakeClientID (client);
 	result = CreateImageBuffers ((WindowPtr)(pwks->pRend->pDrawable),
			    2, ids,
 			    MultibufferUpdateActionBackground,
 			    MultibufferUpdateHintFrequent);
 	if (result != Success)
 	{
 	    pwks->pCurDrawable = pwks->pRend->pDrawable;
 	    return;
 	}
 	for (i = 0; i < 2; i++)
 	    pwks->doubleDrawables[i] = GetBufferPointer (pwks->pRend->pDrawable, i);
 	pwks->curDoubleBuffer = 1;
 	pwks->hasDoubleBuffer = MI_TRUE;
     }
     else if (pwks->curBufferMode == PEXSingleBuffered && pwks->hasDoubleBuffer)
     {
 	/* Destroy the Double Buffers */
 	DestroyImageBuffers ((WindowPtr)(pwks->pRend->pDrawable));
 	pwks->hasDoubleBuffer = MI_FALSE;
     }
#endif
 
     /* With or Without MultiBuffer Call this to set the Drawable Pointer */
     SetDoubleDrawable (pwks);
}
 
SwapDoubleBuffers (pwks)
miWksPtr	pwks;
{
     int	    i;
 
     /* This one swaps the buffers, a no-op if no MultiBuffer Extension */
 
#ifdef MULTIBUFFER
     if (pwks->usingDoubleBuffer)
     {
 	DisplayImageBuffers (&pwks->pCurDrawable->id, 1);
 	pwks->curDoubleBuffer ^= 1;
 	pwks->pCurDrawable = pwks->doubleDrawables[pwks->curDoubleBuffer];
     }
#endif
}
 


/*++
 |
 |  Function Name:	CreatePhigsWks
 |
 |  Function Description:
 |	 Handles the PEXCreatePhigsWKS request.
 |
 |  Note(s):
 |
 --*/

/* set dst = src and if it's not NULL, update the ref list */
#define MIWKS_SETLUT( SRCLUT, DSTLUT, WKS )				\
	if ( (DSTLUT) = (SRCLUT) ) {					\
	    if(UpdateLUTRefs(	(DSTLUT), (diResourceHandle)WKS,	\
				WORKSTATION_RESOURCE, ADD ) ) {		\
		deletewks((miWksPtr)(WKS->deviceData), WKS);		\
		return (BadAlloc);					\
    	} }

#define MIWKS_SETNS( SRCNS, DSTNS, WKS )			\
	if ( (DSTNS) = (SRCNS) ) {				\
	    if (UpdateNSRefs(	(DSTNS), (diResourceHandle)WKS,	\
				WORKSTATION_RESOURCE, ADD )) {	\
		deletewks((miWksPtr)(WKS->deviceData), WKS);	\
		return (BadAlloc);				\
	} }

ddpex4rtn
CreatePhigsWks(pInitInfo, pWKS)
/* in */
	ddWksInit      *pInitInfo;	/* workstation info */
	diWKSHandle     pWKS;	/* workstation handle */
/* out */
{
	register miWksPtr pwks;
	register ddRendererPtr prend;
	ddOrdStruct    *pos;
	diLUTHandle     view;
	register int    i;
	int             type;

#ifdef DDTEST
	ErrorF("\nCreatePhigsWks\n");
#endif
	pWKS->deviceData = NULL;

	if (!pInitInfo->pDrawable)
		return (BadDrawable);
	MI_WHICHDRAW(pInitInfo->pDrawable, type);

	pwks = (miWksPtr) xalloc(sizeof(miWksStr));
	if (pwks == NULL)
		return (BadAlloc);
	pWKS->deviceData = (ddPointer) pwks;

	pwks->pRend = NULL;
	pwks->refCount = 0;
	pwks->freeFlag = 0;
	pwks->reqViewTable = 0;

	pwks->pwksList = puCreateList(DD_WKS);
	if (!pwks->pwksList) {
	    xfree(pwks);
	    pWKS->deviceData = NULL;
	    return (BadAlloc);
	}

	/* see miWks.h for explanation of how view priority list works 
	 * there are dummy first and last entries */
	pwks->views.defined_views = 0;
	pwks->views.highest = &(pwks->views.entries[0]);
	pwks->views.lowest = &(pwks->views.entries[1]);
	pwks->views.entries[0].defined = MI_FALSE;
	pwks->views.entries[0].first_view = 0;
	pwks->views.entries[0].last_view = 0;
	pwks->views.entries[0].higher = NULL;
	pwks->views.entries[0].lower = &(pwks->views.entries[2]);
	pwks->views.entries[1].defined = MI_FALSE;
	pwks->views.entries[1].first_view = 0;
	pwks->views.entries[1].last_view = 0;
	pwks->views.entries[1].higher = &(pwks->views.entries[2]);
	pwks->views.entries[1].lower = NULL;
	/* set first entry */
	pwks->views.entries[2].defined = MI_FALSE;
	pwks->views.entries[2].first_view = 0;
	pwks->views.entries[2].last_view = 65534;
	pwks->views.entries[2].higher = &(pwks->views.entries[0]);
	pwks->views.entries[2].lower = &(pwks->views.entries[1]);
	/* set free entries */
	pwks->views.free = &(pwks->views.entries[3]);
	for (i = 3; i < MIWKS_MAX_ORD_VIEWS - 1; i++) {
		pwks->views.entries[i].defined = MI_FALSE;
		pwks->views.entries[i].first_view = 0;
		pwks->views.entries[i].last_view = 0;
		pwks->views.entries[i].higher = &(pwks->views.entries[i - 1]);
		pwks->views.entries[i].lower = &(pwks->views.entries[i + 1]);
	}
	/* redefine the higher value for the first free entry */
	pwks->views.entries[3].higher = NULL;
	/* now define the last free entry */
	i = MIWKS_MAX_ORD_VIEWS - 1;
	pwks->views.entries[i].defined = MI_FALSE;
	pwks->views.entries[i].first_view = 0;
	pwks->views.entries[i].last_view = 0;
	pwks->views.entries[i].higher = &(pwks->views.entries[i - 1]);
	pwks->views.entries[i].lower = NULL;
	/* predefined views are set in the priority list later */

	pwks->postedStructs.numStructs = 0;
	pwks->postedStructs.postruct = NULL;

	prend = (ddRendererPtr) xalloc(sizeof(ddRendererStr));
	if (prend == NULL) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	pwks->pRend = prend;

	/* initialize the renderer */
	/* initialize the default pipeline context, if necessary,
	   and set this into the renderer */
	prend->pPC = NULL;

	/* initialize to 0, just in case we hit an alloc error and
	    call deletewks before finishing creation
	 */
	for (i = MI_FIRSTTABLETYPE; i <= PEXMaxTableType; i++)
	    prend->lut[i] = 0;

	for (i=0; i< (int) DD_MAX_FILTERS; i++)
	    prend->lut[i] = 0;

	for (i=0; i< MIWKS_NUM_PICK_DEVICES; i++)
	    pwks->devices[i].path = 0;

	/* expect a valid drawable */
	MI_SETDRAWEXAMPLE(pInitInfo->pDrawable, &(prend->drawExample));

	prend->rendId = pWKS->id;
	prend->pDrawable = pInitInfo->pDrawable;
	prend->drawableId = pInitInfo->drawableId;
	prend->curPath = puCreateList(DD_ELEMENT_REF);
	prend->clipList = 0;
	if (!prend->curPath) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	prend->clipList = puCreateList(DD_DEVICE_RECT);
	if (!prend->clipList) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	prend->state = PEXIdle;

	/* add later: make sure that the drawable is OK for the luts */
	MIWKS_SETLUT(pInitInfo->pMarkerLUT, prend->lut[PEXMarkerBundleLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pTextLUT, prend->lut[PEXTextBundleLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pLineLUT, prend->lut[PEXLineBundleLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pIntLUT, prend->lut[PEXInteriorBundleLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pEdgeLUT, prend->lut[PEXEdgeBundleLUT], pWKS);
	/* view table  done later */
	MIWKS_SETLUT(pInitInfo->pColourLUT, prend->lut[PEXColourLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pDepthCueLUT, prend->lut[PEXDepthCueLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pLightLUT, prend->lut[PEXLightLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pColourAppLUT, prend->lut[PEXColourApproxLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pPatternLUT, prend->lut[PEXPatternLUT], pWKS);
	MIWKS_SETLUT(pInitInfo->pFontLUT, prend->lut[PEXTextFontLUT], pWKS);

	MIWKS_SETNS(pInitInfo->pHighInclSet, prend->ns[(int) DD_HIGH_INCL_NS], pWKS);
	MIWKS_SETNS(pInitInfo->pHighExclSet, prend->ns[(int) DD_HIGH_EXCL_NS], pWKS);
	MIWKS_SETNS(pInitInfo->pInvisInclSet, prend->ns[(int) DD_INVIS_INCL_NS], pWKS);
	MIWKS_SETNS(pInitInfo->pInvisExclSet, prend->ns[(int) DD_INVIS_EXCL_NS], pWKS);
        /* These are for Renderer Picking, not used for Wks */
        MIWKS_SETNS(0, prend->ns[(int) DD_PICK_INCL_NS], pWKS);
        MIWKS_SETNS(0, prend->ns[(int) DD_PICK_EXCL_NS], pWKS);


	prend->hlhsrMode = PEXHlhsrOff;
	prend->npcSubvolume = NPCInit;

	prend->viewport.useDrawable = MI_TRUE;
	prend->viewport.minval.x = 0.0;
	prend->viewport.minval.y = 0.0;
	prend->viewport.minval.z = 0.0;
	prend->viewport.maxval.x = prend->pDrawable->width;
	prend->viewport.maxval.y = prend->pDrawable->height;
	prend->viewport.maxval.z = 1.0;
	/* don't really use renderer dynamics */
	prend->tablesMask = 0;
	prend->namesetsMask = 0;
	prend->attrsMask = 0;
	prend->tablesChanges = 0;
	prend->namesetsChanges = 0;
	prend->attrsChanges = 0;
	prend->immediateMode = FALSE;

	prend->pDDContext = NULL;

	/* new flags added for 5.1 need to be initialized */
	prend->backgroundColour.colourType = PEXIndexedColour;
	prend->backgroundColour.colour.indexed.index = 0;
	prend->clearI = FALSE;
	prend->clearZ = TRUE;
	prend->echoMode = PEXNoEcho;
	prend->echoColour.colourType = PEXIndexedColour;
	prend->echoColour.colour.indexed.index = 0;


	pwks->displayUpdate = PEXVisualizeEach;
	pwks->visualState = PEXCorrect;
	pwks->displaySurface = PEXEmpty;
	pwks->viewUpdate = PEXNotPending;
	pwks->deltaviewMask = 0;
	pwks->wksUpdate = PEXNotPending;
	pwks->wksMask = 0;

	pwks->hlhsrUpdate = PEXNotPending;

	pwks->bufferUpdate = PEXNotPending;
	pwks->curBufferMode = pwks->reqBufferMode = pInitInfo->bufferMode;
        pwks->hasDoubleBuffer = MI_FALSE;

	pwks->reqNpcSubvolume = NPCInit;

	pwks->reqviewport.useDrawable = MI_TRUE;
	pwks->reqviewport.minval.x = 0.0;
	pwks->reqviewport.minval.y = 0.0;
	pwks->reqviewport.minval.z = 0.0;
	pwks->reqviewport.maxval.x = prend->pDrawable->width;
	pwks->reqviewport.maxval.y = prend->pDrawable->height;
	pwks->reqviewport.maxval.z = 1.0;

	/*
	 * pos: ordered structure list for posted structures. the first pos
	 * is a dummy
	 */
	pos = (ddOrdStruct *) xalloc(sizeof(ddOrdStruct));
	if (pos == NULL) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	pos->pstruct = 0;
	pos->priority = 0;
	pos->next = NULL;
	pwks->postedStructs.postruct = pos;
	pwks->postedStructs.numStructs = 0;

	/*
	 * create the view tables as PEX lookup tables use the wks id as the
	 * lut id. don't put the workstation on the view table's reference
	 * list, we don't want the LUT procedure to generate any picture
	 * redraws this also prevents any nasty side effects when the
	 * resource is deleted
	 */

	/* requested view */
	/* first create the resource structure that has the table id and type */
	if (!(view = (diLUTHandle) xalloc(sizeof(ddLUTResource)))) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	view->id = pWKS->id;
	view->lutType = PEXViewLUT;

	if (err43 = CreateLUT(pInitInfo->pDrawable, view)) {
		deletewks(pwks, pWKS);
		return (err43);
	}
	pwks->reqViewTable = view;

	/* current view */
	/* first create the resource structure that has the table id and type */
	if (!(view = (diLUTHandle) xalloc(sizeof(ddLUTResource)))) {
		deletewks(pwks, pWKS);
		return (BadAlloc);
	}
	view->id = pWKS->id;
	view->lutType = PEXViewLUT;

	if (err43 = CreateLUT(pInitInfo->pDrawable, view)) {
		deletewks(pwks, pWKS);
		return (err43);
	}
	prend->lut[PEXViewLUT] = view;

	/*
	 * add an entry for each predefined view note that this assumes that
	 * view 0 is defined to the PEX default values as required for PHIGS
	 * wks
	 */
	if ((MILUT_HEADER(view))->tableInfo.numPredefined) {
		for (i = (MILUT_HEADER(view))->tableInfo.predefinedMin;
		  i <= (MILUT_HEADER(view))->tableInfo.predefinedMax; i++) {
			err3 = mi_add_ord_view(&pwks->views, (ddUSHORT)i);
		}
	}
	/* WksDynamics */
	for (i = 0; i < (int) MAX_DYNAMIC; i++)
		pwks->dynamics[i] = mi_dynamics[type][i];

	/* call level III procedure to create dd parts of renderer */
	if (err3 = InitRenderer(prend)) {
		deletewks(pwks, pWKS);
		return (err3);
	}

	/* Pick device */
	if (!init_pick_flag) {
		initialize_pick_devices();
		init_pick_flag = MI_TRUE;
	}
	for (i = 0; i < MIWKS_NUM_PICK_DEVICES; i++) {
		pwks->devices[i] = pick_devices[i];
		if (!(pwks->devices[i].path = puCreateList(DD_PICK_PATH))) {
			deletewks(pwks, pWKS);
			return (BadAlloc);
		}
	}


        /* do stuff here to set up hlhsr mode */

        /* do stuff here to set up buffer mode */
        ChangeDoubleBuffers (pwks);

	return (Success);
}				/* CreatePhigsWks */

static void
deletewks(pwks, pWKS)
	miWksPtr        pwks;
	diWKSHandle     pWKS;
{
    register ddRendererPtr prend;
    register int    i;
    ddOrdStruct    *pos, *posn;

    if (pwks == NULL) return;
    prend = pwks->pRend;
    if (prend) {
	register int    i;
	for (i = MI_FIRSTTABLETYPE; i <= PEXMaxTableType; i++) {
	    if (prend->lut[i]) {
		if (i != PEXViewLUT)
		    err43 = UpdateLUTRefs(prend->lut[i], (diResourceHandle) pWKS,
					      WORKSTATION_RESOURCE, REMOVE);
		else

		    /*
		     * FreeLUT frees the resource handle structure and the 
		     * dd struct if there are no outstanding refs to the LUT
		     */
		    FreeLUT(prend->lut[i], pWKS->id);
		prend->lut[i] = 0;
	    }
	}
	for (i = 0; i < (int) DD_MAX_FILTERS; i++) {
	    if (prend->ns[i])
		err43 = UpdateNSRefs(prend->ns[i], (diResourceHandle) pWKS,
					      WORKSTATION_RESOURCE, REMOVE);
	}

	if (prend->curPath) {
	    puDeleteList(prend->curPath);
	    prend->curPath = 0; }
	if (prend->clipList) {
	    puDeleteList(prend->clipList);
	    prend->clipList = 0; }

	if (prend->pDDContext) {
	    DeleteDDContext(prend->pDDContext);
	    prend->pDDContext = 0; }

	if (prend->pPC) { 
	    xfree(prend->pPC);	/* allocated in one chunk */
	    prend->pPC = 0;}

	xfree(prend);
	pwks->pRend = 0;
    }

    if (pwks->reqViewTable) {
	FreeLUT(pwks->reqViewTable, pWKS->id);
	pwks->reqViewTable; }
    if (pwks->pwksList) {
	puDeleteList(pwks->pwksList);
	pwks->reqViewTable; }

    /* unpost all the structures */
    if (pwks->postedStructs.postruct) {
	pos = pwks->postedStructs.postruct->next;  /* first element is a dummy */
	while (pos) {
	    /* take the wks out of the structs wks lists */
	    err4 = UpdateStructRefs(	pos->pstruct, (diResourceHandle) pWKS,
					WORKSTATION_RESOURCE, REMOVE);
	    posn = pos;
	    pos = posn->next;
	    xfree(posn);
	}
	pwks->postedStructs.numStructs = 0;
	pwks->postedStructs.postruct->next =  0;
	xfree(pwks->postedStructs.postruct);
	pwks->postedStructs.postruct = 0;
    }

    /* pick devices */
    for (i = 0; i < MIWKS_NUM_PICK_DEVICES; i++) {

	/*
	 * paths are empty on initialization, but remember that structure 
	 * reference count are updated when the path changes
	 */
	path_update_struct_refs(pwks->devices[i].path, (diResourceHandle) NULL, 
				PICK_RESOURCE, REMOVE);

	if (pwks->devices[i].path) {
	    puDeleteList(pwks->devices[i].path);
	    pwks->devices[i].path = 0; }
	if (pwks->devices[i].inclusion) {
	    err43 = UpdateNSRefs(pwks->devices[i].inclusion, 
				(diResourceHandle) NULL, PICK_RESOURCE, REMOVE);
	    pwks->devices[i].inclusion = 0; }
	if (pwks->devices[i].exclusion) {
	    err43 = UpdateNSRefs(pwks->devices[i].exclusion,
				 (diResourceHandle) NULL, PICK_RESOURCE, REMOVE);
	    pwks->devices[i].exclusion = 0; }
    }

    xfree(pwks);
    pWKS->deviceData = NULL;
    return;
}				/* deletewks */

#define CHECK_DELETE( pddwks, handle )						\
	if ((((pddwks)->freeFlag) == MI_TRUE) && (((pddwks)->refCount) <= 0 ))	\
	{	deletewks( pddwks, handle );					\
		xfree((char *)(handle));					\
	}

/*++
 |
 |  Function Name:	FreePhigsWks
 |
 |  Function Description:
 |	 Handles the PEXFreePhigsWKS request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
FreePhigsWks(pWKS, WKSid)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddResourceId    WKSid;	/* phigs workstation resource id */
/* out */
{
	register miWksPtr pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nFreePhigsWks\n");
#endif

	pWKS->id = PEXAlreadyFreed;
	pwks->freeFlag = MI_TRUE;
	CHECK_DELETE(pwks, pWKS);

	return (Success);
}				/* FreePhigsWks */

/*++
 |
 |  Function Name:	InquireWksInfo
 |
 |  Function Description:
 |	 Handles the PEXGetWKSInfo request.
 |
 |  Note(s):
 |
 --*/

/* depends on 'mask' being defined */
#define	WKS_CHECK_BITMASK( bitIndex )		\
	if (mask[((bitIndex)/32)] & (((unsigned long)1) << ((bitIndex) % 32)))

/* depends on 'mask' and needbytes being there */
#define COUNTBYTES( type, bytes ) 		\
	WKS_CHECK_BITMASK( type )		\
		needbytes += (bytes)

static XID  ulNULL = 0;

#define PLUTID( plut ) \
	(plut)==NULL ? &ulNULL : &(plut)->id

#define PNSID( pns ) \
	(pns)==NULL ? &ulNULL : &(pns)->id

ddpex4rtn
InquireWksInfo(pWKS, mask, pNumValues, pBuffer)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddBitmask       mask[2];/* item mask */
/* out */
	ddULONG        *pNumValues;	/* number of items returned */
	ddBufferPtr     pBuffer;/* workstation information */
{
	register miWksPtr pwks = (miWksPtr) pWKS->deviceData;
	ddULONG         needbytes;
	int             sshort, sulong, sbyte, sushort, sfloat, spexNpcSubvolume, spexViewport;
	ddPointer       pbyte;

#ifdef DDTEST
	ErrorF("\nInquireWksInfo\n");
#endif

	*pNumValues = 0;

	/* calculate the number of bytes needed to return the info */
	needbytes = 0;

	COUNTBYTES(PEXPWDisplayUpdate, 4);
	COUNTBYTES(PEXPWVisualState, 4);
	COUNTBYTES(PEXPWDisplaySurface, 4);
	COUNTBYTES(PEXPWViewUpdate, 4);
	COUNTBYTES(PEXPWDefinedViews, (4 + 4 * pwks->views.defined_views));
	COUNTBYTES(PEXPWWksUpdate, 4);
	COUNTBYTES(PEXPWReqNpcSubvolume, 24);
	COUNTBYTES(PEXPWCurNpcSubvolume, 24);
	COUNTBYTES(PEXPWReqWksViewport, 20);
	COUNTBYTES(PEXPWCurWksViewport, 20);
	COUNTBYTES(PEXPWHlhsrUpdate, 4);
	COUNTBYTES(PEXPWReqHlhsrMode, 4);
	COUNTBYTES(PEXPWCurHlhsrMode, 4);
	COUNTBYTES(PEXPWDrawable, 4);
	COUNTBYTES(PEXPWMarkerBundle, 4);
	COUNTBYTES(PEXPWTextBundle, 4);
	COUNTBYTES(PEXPWLineBundle, 4);
	COUNTBYTES(PEXPWInteriorBundle, 4);
	COUNTBYTES(PEXPWEdgeBundle, 4);
	COUNTBYTES(PEXPWColourTable, 4);
	COUNTBYTES(PEXPWDepthCueTable, 4);
	COUNTBYTES(PEXPWLightTable, 4);
	COUNTBYTES(PEXPWColourApproxTable, 4);
	COUNTBYTES(PEXPWPatternTable, 4);
	COUNTBYTES(PEXPWTextFontTable, 4);
	COUNTBYTES(PEXPWHighlightIncl, 4);
	COUNTBYTES(PEXPWHighlightExcl, 4);
	COUNTBYTES(PEXPWInvisibilityIncl, 4);
	COUNTBYTES(PEXPWInvisibilityExcl, 4);
	COUNTBYTES(PEXPWPostedStructures, (4 + 8 * pwks->postedStructs.numStructs));
	COUNTBYTES(PEXPWNumPriorities, 4);
        COUNTBYTES(PEXPWBufferUpdate, 4); 
	COUNTBYTES(PEXPWCurBufferMode, 4);
	COUNTBYTES(PEXPWReqBufferMode, 4);

	PU_CHECK_BUFFER_SIZE(pBuffer, needbytes);

	/* let's remember a few sizes */
	sshort = sizeof(ddSHORT);
	ASSURE(sshort == 2);
	sbyte = sizeof(ddBYTE);
	ASSURE(sbyte == 1);
	sulong = sizeof(ddULONG);
	ASSURE(sulong == 4);
	sushort = sizeof(ddUSHORT);
	ASSURE(sushort == 2);
	sfloat = sizeof(ddFLOAT);
	ASSURE(sfloat == 4);
	spexNpcSubvolume = sizeof(ddNpcSubvolume);
	ASSURE(spexNpcSubvolume == 24);
	spexViewport = sizeof(ddViewport);
	ASSURE(spexViewport == 20);

	pbyte = pBuffer->pBuf;

	/* return the info in the format encoded for the reply */

	/*  
	  Took out the micopy usage cause it was a real brain dead way 
	  to do this. Note that this stuff NEVER checks the buffer size, 
	  maybe I'll add this someday  - JSH
	 */
	WKS_CHECK_BITMASK(PEXPWDisplayUpdate) {
	      PACK_CARD32(pwks->displayUpdate, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWVisualState) {
	      PACK_CARD32(pwks->visualState, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWDisplaySurface) {
	      PACK_CARD32(pwks->displaySurface, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWViewUpdate) {
	      PACK_CARD32(pwks->viewUpdate, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWDefinedViews) {
		/* returns in order, highest priority view first */
		ddOrdView      *indexer;

		(*pNumValues) += pwks->views.defined_views;
		PACK_CARD32(pwks->views.defined_views, pbyte);
		indexer = pwks->views.highest;
		do {
			if (indexer->defined) {
			      PACK_CARD32(indexer->first_view, pbyte);
			}
			indexer = indexer->lower;
		} while (indexer != NULL);
	}
	WKS_CHECK_BITMASK(PEXPWWksUpdate) {
	      PACK_CARD32(pwks->wksUpdate, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWReqNpcSubvolume) {
	      PACK_STRUCT(ddNpcSubvolume, &(pwks->reqNpcSubvolume), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWCurNpcSubvolume) {
	      PACK_STRUCT(ddNpcSubvolume, &(pwks->pRend->npcSubvolume), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWReqWksViewport) {
	      PACK_STRUCT(ddViewport, &(pwks->reqviewport), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWCurWksViewport) {
	      PACK_STRUCT(ddViewport, &(pwks->pRend->viewport), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWHlhsrUpdate) {
	      PACK_CARD32(pwks->hlhsrUpdate, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWReqHlhsrMode) {
	      PACK_CARD32(pwks->reqhlhsrMode, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWCurHlhsrMode) {
	      PACK_CARD32(pwks->pRend->hlhsrMode, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWDrawable) {
	      PACK_CARD32(pwks->pRend->drawableId, pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWMarkerBundle) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXMarkerBundleLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWTextBundle) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXTextBundleLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWLineBundle) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXLineBundleLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWInteriorBundle) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXInteriorBundleLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWEdgeBundle) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXEdgeBundleLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWColourTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXColourLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWDepthCueTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXDepthCueLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWLightTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXLightLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWColourApproxTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXColourApproxLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWPatternTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXPatternLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWTextFontTable) {
	      PACK_CARD32(*(PLUTID(pwks->pRend->lut[PEXTextFontLUT])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWHighlightIncl) {
	      PACK_CARD32(*(PNSID(pwks->pRend->ns[(int) DD_HIGH_INCL_NS])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWHighlightExcl) {
	      PACK_CARD32(*(PNSID(pwks->pRend->ns[(int) DD_HIGH_EXCL_NS])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWInvisibilityIncl) {
	      PACK_CARD32(*(PNSID(pwks->pRend->ns[(int) DD_INVIS_INCL_NS])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWInvisibilityExcl) {
	      PACK_CARD32(*(PNSID(pwks->pRend->ns[(int) DD_INVIS_EXCL_NS])), pbyte);
	      (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWPostedStructures) {
		register ddOrdStruct *pos;

		PACK_CARD32(pwks->postedStructs.numStructs, pbyte);
		pos = pwks->postedStructs.postruct;
		while (pos->next) {
			PACK_CARD32(pos->next->pstruct->id, pbyte);
			PACK_FLOAT(pos->next->priority, pbyte);
			pos = pos->next;
		}
		(*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWNumPriorities) {

		/*
		 * return 0 for now - should implement num_priorities as part
		 * of the wks
		 */
		PACK_CARD32( 0, pbyte);
		(*pNumValues)++;
	}

        WKS_CHECK_BITMASK( PEXPWBufferUpdate )
        {
	    PACK_CARD32( pwks->bufferUpdate, pbyte);
	    (*pNumValues)++;
        }

	WKS_CHECK_BITMASK(PEXPWReqBufferMode) {
	    PACK_CARD32( pwks->reqBufferMode, pbyte);
	    (*pNumValues)++;
	}
	WKS_CHECK_BITMASK(PEXPWCurBufferMode) {
	    PACK_CARD32( pwks->curBufferMode, pbyte);
	    (*pNumValues)++;
	}
	pBuffer->dataSize = needbytes;
	ASSURE(needbytes == (pbyte - pBuffer->pBuf));

	return (Success);
}				/* InquireWksInfo */

/*++
 |
 |  Function Name:	InquireWksDynamics
 |
 |  Function Description:
 |	 Handles the PEXGetDynamics request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireWksDynamics(pDrawable, pValues)
/* in */
	DrawablePtr     pDrawable;	/* drawable */
/* out */
	ddWksDynamics  *pValues;/* dynamics information */
{
	ddBYTE         *pdtable;
	int             type;

#ifdef DDTEST
	ErrorF("\nInquireWksDynamics\n");
#endif

	MI_WHICHDRAW(pDrawable, type);

	pdtable = mi_dynamics[type];
	pValues->viewRep = pdtable[(int) VIEW_REP_DYNAMIC];
	pValues->markerBundle = pdtable[(int) MARKER_BUNDLE_DYNAMIC];
	pValues->textBundle = pdtable[(int) TEXT_BUNDLE_DYNAMIC];
	pValues->lineBundle = pdtable[(int) LINE_BUNDLE_DYNAMIC];
	pValues->interiorBundle = pdtable[(int) INTERIOR_BUNDLE_DYNAMIC];
	pValues->edgeBundle = pdtable[(int) EDGE_BUNDLE_DYNAMIC];
	pValues->colourTable = pdtable[(int) COLOUR_TABLE_DYNAMIC];
	pValues->patternTable = pdtable[(int) PATTERN_TABLE_DYNAMIC];
	pValues->wksTransform = pdtable[(int) WKS_TRANSFORM_DYNAMIC];
	pValues->highlightFilter = pdtable[(int) HIGH_FILTER_DYNAMIC];
	pValues->invisFilter = pdtable[(int) INVIS_FILTER_DYNAMIC];
	pValues->hlhsrMode = pdtable[(int) HLHSR_MODE_DYNAMIC];
	pValues->strModify = pdtable[(int) STR_MODIFY_DYNAMIC];
	pValues->postStr = pdtable[(int) POST_STR_DYNAMIC];
	pValues->unpostStr = pdtable[(int) UNPOST_STR_DYNAMIC];
	pValues->deleteStr = pdtable[(int) DELETE_STR_DYNAMIC];
	pValues->refModify = pdtable[(int) REF_MODIFY_DYNAMIC];
	pValues->bufferModify = pdtable[(int) BUFFER_MODIFY_DYNAMIC];
	pValues->lightTable = pdtable[(int) BUFFER_MODIFY_DYNAMIC];
	pValues->depthCueTable = pdtable[(int) BUFFER_MODIFY_DYNAMIC];
	pValues->colourApproxTable = pdtable[(int) BUFFER_MODIFY_DYNAMIC];

	return (Success);
}				/* InquireWksDynamics */

/*++
 |
 |  Function Name:	InquireViewRep
 |
 |  Function Description:
 |	 Handles the PEXGetViewRep request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireViewRep(pWKS, index, pUpdate, pRequested, pCurrent)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddTableIndex    index;	/* view table index */
/* out */
	ddUSHORT       *pUpdate;/* (pending/notpending) */
	ddViewRep      *pRequested;	/* requested view */
	ddViewRep      *pCurrent;	/* current view */
{
	register miWksPtr pwks = (miWksPtr) pWKS->deviceData;
	ddUSHORT        status;
	ddBuffer        buffer;

#ifdef DDTEST
	ErrorF("\nInquireViewRep\n");
#endif

	*pUpdate = pwks->viewUpdate;
	buffer.bufSize = 0;
	buffer.dataSize = 0;
	buffer.pBuf = NULL;
	buffer.pHead = NULL;
	if (err43 = InquireLUTEntry(pwks->reqViewTable, index, PEXSetValue, &status, &buffer))
		return (err43);
	pRequested->index = index;
	mibcopy(buffer.pBuf, &(pRequested->view), sizeof(ddViewEntry));
	if (err43 = InquireLUTEntry(pwks->pRend->lut[PEXViewLUT], index, PEXSetValue, &status, &buffer))
		return (err43);
	pCurrent->index = index;
	mibcopy(buffer.pBuf, &(pCurrent->view), sizeof(ddViewEntry));
	xfree(buffer.pHead);
	return (Success);
}				/* InquireViewRep */

/*++
 |
 |  Function Name:	RedrawStructures
 |
 |  Function Description:
 |	 Handles the PEXRedrawAllStructures request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
RedrawStructures(pWKS)
/* in */
    diWKSHandle     pWKS;	/* workstation handle */
/* out */
{
    register miWksPtr pwks = (miWksPtr) pWKS->deviceData;
    register DrawablePtr pDraw = pwks->pRend->pDrawable;


#ifdef DDTEST
    ErrorF("\nRedrawStructures\n");
#endif

    if (    (pDraw == NULL)
	 || (pwks->pRend->drawableId == PEXAlreadyFreed))
	return (BadDrawable);

    if (pDraw->class == InputOnly)	/* from dix dispatch.c */
	return (BadMatch);
    else if (!pwks->usingDoubleBuffer) {/* pixmap: fill the rectangle ???? */

	GCPtr           pGC;
	extern GCPtr    CreateScratchGC();
	extern int	ChangeGC();
	extern void	ValidateGC();
	xRectangle      xrect;
	ddUSHORT	status;
	miColourEntry	*pLUT;
	unsigned long	gcmask, colourindex;

	/*
	 * get background colour and convert it to an X pixel use
	 * default entry (0) in colourApprox LUT for the conversion
	 */
	InquireLUTEntryAddress(	PEXColourLUT, pwks->pRend->lut[PEXColourLUT], 0,
				&status, &pLUT);
	miColourtoIndex(pwks->pRend, 0, &pLUT->entry, &colourindex);

	pGC = CreateScratchGC(pDraw->pScreen, pDraw->depth);
	gcmask = GCForeground;
	ChangeGC(pGC, gcmask, &colourindex);
	ValidateGC(pDraw, pGC);
	xrect.x = 0;
	xrect.y = 0;	
	xrect.width = pDraw->width;
	xrect.height = pDraw->height;
	(*pGC->ops->PolyFillRect) (pDraw, pGC, 1, &xrect);
	FreeScratchGC(pGC);
	pwks->displaySurface = PEXEmpty;
    }

    /*
     * If any attributes are set to Pending, make the requested values
     * current and reset to NotPending - for the following
     */

    if (pwks->viewUpdate == PEXPending) {
	register ddUSHORT i, maxviews;
	ddUSHORT        status;
	ddBuffer        buffer;

	buffer.bufSize = sizeof(ddViewEntry);
	buffer.dataSize = 0;
	buffer.pHead = buffer.pBuf = (ddPointer) xalloc(sizeof(ddViewEntry));

	if (!buffer.pBuf) return (BadAlloc);	/* we're out of memory! */

	/*
	 * loop to check each entry to see if it is pending assume
	 * they are not consecutive entries and set them one at a
	 * time
	 */
	maxviews = MIWKS_MAX_VIEWS;
	for (i = 0; i < maxviews; i++) {
	    if (pwks->deltaviewMask & (1L << i)) {

		if (err43 = InquireLUTEntry(	pwks->reqViewTable, i, 
						PEXSetValue, &status, &buffer))
		    return (err43);		/* we're out of memory! */

		if (err43 = SetLUTEntries(  pwks->pRend->lut[PEXViewLUT],
					    i, 1, buffer.pBuf))
		    return (err43);		/* we're out of memory! */

		/*
		 * make sure it is in the list of defined
		 * views
		 */
		err43 = mi_add_ord_view(&pwks->views, i);
		if (err43) return (err43);	/* we're out of memory! */
	    }
	}
	pwks->deltaviewMask = 0;
	xfree(buffer.pBuf);
	pwks->viewUpdate = PEXNotPending;
    }

    if (pwks->wksUpdate == PEXPending) {
	if (pwks->wksMask & WKS_WINDOW_MASK) {
	    pwks->pRend->npcSubvolume = pwks->reqNpcSubvolume;
	    pwks->pRend->attrsChanges |= PEXDynNpcSubvolume;
	}

	if (pwks->wksMask & WKS_VIEWPORT_MASK) {
	    pwks->pRend->viewport = pwks->reqviewport;
	    pwks->pRend->attrsChanges |= PEXDynViewport;
	}

	pwks->wksMask = 0;
	pwks->wksUpdate = PEXNotPending;
    }

    if (pwks->hlhsrUpdate == PEXPending) {
	pwks->pRend->hlhsrMode = pwks->reqhlhsrMode;
	pwks->hlhsrUpdate = PEXNotPending;
	pwks->pRend->attrsChanges |= PEXDynHlhsrMode;
	/* do stuff here to effect change in hlhsr mode */
    }

    if (pwks->bufferUpdate == PEXPending) {
	pwks->curBufferMode = pwks->reqBufferMode;
	pwks->bufferUpdate = PEXNotPending;
	/* do stuff here to effect change in buffer mode */
        ChangeDoubleBuffers (pwks);
    }
    err = miTraverse(pWKS);

    SwapDoubleBuffers (pwks);

    /* Set visual_state to Correct */
    pwks->visualState = PEXCorrect;
    /* display Surface is set in miTraverse */

    return (err);
}				/* RedrawStructures */


/*++
 |
 |  Function Name:	UpdateWks
 |
 |  Function Description:
 |	 Handles the PEXUpdateWorkstation request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
UpdateWks(pWKS)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
/* out */
{
	/* If visual_state is Deferred or Simulated, call RedrawAll */
	if (((miWksPtr) pWKS->deviceData)->visualState != PEXCorrect) {
		return (RedrawStructures(pWKS));
	}
	return (Success);
}				/* UpdateWks */

/*++
 |
 |  Function Name:	RedrawClipRegion
 |
 |  Function Description:
 |	 Handles the PEXRedrawClipRegion request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
RedrawClipRegion(pWKS, numRects, pRects)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddULONG         numRects;	/* number of rectangles in list */
	ddDeviceRect   *pRects;	/* list of rectangles */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nRedrawClipRegion\n");
#endif

	/*
	 * set the clip list in the renderer first, empty the clip list
	 */
	pwks->pRend->clipList->numObj = 0;
	if (puAddToList((ddPointer) pRects, numRects, pwks->pRend->clipList)
	    == MI_ALLOCERR)
		return (BadAlloc);

	pwks->pRend->attrsChanges |= PEXDynClipList;

	/* now redraw picture without updating any state */
	miTraverse(pWKS);	/* ignore errors */


 	/*
 	 * The clip list must be emptied so that subsequent workstation
 	 * updates redraw the entire display surface.
 	 */

 	pwks->pRend->clipList->numObj = 0;

	return (Success);
}				/* RedrawClipRegion */

/*++
 |
 |  Function Name:	ExecuteDeferred
 |
 |  Function Description:
 |	 Handles the PEXExecutedDeferredActions request.
 |
 |  Note(s):
	There is no definition in PHIGS or PEX of what this
	is supposed to do.  The PHIGS people we have talked
	to agree with this.  Therefore, we have implemented
	it as a no-op.
 |
 --*/

ddpex4rtn
ExecuteDeferred(pWKS)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
/* out */
{

#ifdef DDTEST
	ErrorF("\nExecuteDeferred\n");
#endif

	return (Success);
}				/* ExecuteDeferred */

/*++
 |
 |  Function Name:	SetViewPriority
 |
 |  Function Description:
 |	 Handles the PEXSetViewPriority request.
 |
 |  Note(s):
	move index1 to priority relative to index2
 |
 --*/

ddpex4rtn
SetViewPriority(pWKS, index1, index2, priority)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddTableIndex    index1;	/* view index */
	ddTableIndex    index2;	/* view index */
	ddUSHORT        priority;	/* (higher/lower) */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	ddOrdView      *entry1, *entry2;

#ifdef DDTEST
	ErrorF("\nSetViewPriority\n");
#endif

	if ((priority != PEXHigher) && (priority != PEXLower))
		return (BadValue);

	entry1 = mi_find_ord_view(&pwks->views, index1);
	entry2 = mi_find_ord_view(&pwks->views, index2);

	if ((entry1 == NULL) || (entry2 == NULL))
		return (BadValue);	/* a view is not defined */

	if (index1 == index2)
		return (Success);

	if (priority == PEXLower) {
		/* don't do anything if index1 is already next lowest */
		if (entry2->lower != entry1) {
			/* take entry1 out of its position */
			entry1->higher->lower = entry1->lower;
			entry1->lower->higher = entry1->higher;
			/* put entry1 before entry2->lower */
			entry1->lower = entry2->lower;
			entry2->lower->higher = entry1;
			/* put entry1 after entry2 */
			entry2->lower = entry1;
			entry1->higher = entry2;
		}
	} else {		/* PEXHigher */
		/* don't do anything if index1 is already next highest */
		if (entry2->higher != entry1) {
			/* take entry1 out of its position */
			entry1->higher->lower = entry1->lower;
			entry1->lower->higher = entry1->higher;
			/* put entry1 after entry2->higher */
			entry1->higher = entry2->higher;
			entry2->higher->lower = entry1;
			/* put entry1 before entry2 */
			entry2->higher = entry1;
			entry1->lower = entry2;
		}
	}

	return (Success);
}				/* SetViewPriority */

/*++
 |
 |  Function Name:	SetDisplayUpdateMode
 |
 |  Function Description:
 |	 Handles the PEXSetDisplayUpdateMode request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetDisplayUpdateMode(pWKS, mode)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddSHORT         mode;	/* display update mode */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	register int    type, i;

#ifdef DDTEST
	ErrorF("\nSetDisplayUpdateMode\n");
#endif

	if ((pwks->pRend->pDrawable == NULL) || (pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

	/* todo: put drawtype in wks header */

	MI_WHICHDRAW(pwks->pRend->pDrawable, type);

	/*
	 * make sure this mode is supported by the drawable in this
	 * implementation
	 */
	for (i = 0; i < SI_UPDATE_NUM; i++) {
		if (mode == miDisplayUpdateModeET[type][i].index) {
			pwks->displayUpdate = mode;
                        SetDoubleDrawable (pwks);
			if (mode == PEXVisualizeEach)

				/*
				 * you may want to do this if mode =
				 * PEXWhenever, too
				 */
			{
				/* regen the picture if it isn't correct */
				if (pwks->visualState != PEXCorrect) {
					if (err4 = RedrawStructures(pWKS))
						return (err4);
					pwks->visualState = PEXCorrect;
				}
			}
			return (Success);
		}
	}

	return (BadValue);
}				/* SetDisplayUpdateMode */

/* get_view gets the view transforms from the view table and
 * it returns the composit transform and the clipping info
 * it calculates the composite view transform if the vomFlag is true
 */
static int
get_view(view_table, view_index, clipflag, clips, vom, vomFlag)
	diLUTHandle     view_table;
	ddTableIndex    view_index;
	ddUSHORT       *clipflag;
	ddNpcSubvolume *clips;
	ddFLOAT         vom[4][4];
	ddBYTE          vomFlag;
{
	ddBuffer        buffer;
	ddUSHORT        status;
	ddFLOAT         orient[4][4];
	ddFLOAT         map[4][4];

	/* get view rep */
	buffer.pHead = buffer.pBuf = NULL;
	buffer.dataSize = buffer.bufSize = 0;
	err = InquireLUTEntry(view_table, view_index, PEXSetValue, &status, &buffer);
	if (err != Success)
		return (err);

	mibcopy(&(((ddViewEntry *) buffer.pBuf)->clipLimits),
	      clips, sizeof(ddNpcSubvolume));
	*clipflag = ((ddViewEntry *) buffer.pBuf)->clipFlags;

	if (vomFlag) {
		mibcopy((((ddViewEntry *) buffer.pBuf)->orientation),
		      orient, 16 * sizeof(ddFLOAT));
		mibcopy((((ddViewEntry *) buffer.pBuf)->mapping),
		      map, 16 * sizeof(ddFLOAT));
		miMatMult(vom, orient, map);
	}
	xfree(buffer.pHead);
	return (Success);
}

/* no check for z when using drawable */
#define PT_IN_LIMIT(prend, lim, pt) 				\
     ( (pt)->x >= (lim)->minval.x && (pt)->x <= (lim)->maxval.x \
    && (pt)->y >= (lim)->minval.y && (pt)->y <= (lim)->maxval.y \
    && (pt)->z >= (lim)->minval.z && (pt)->z <= (lim)->maxval.z )

/*++
 |
 |  Function Name:	MapDcWc
 |
 |  Function Description:
 |	 Handles the PEXMapDCtoWC request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
MapDcWc(pWKS, numPoints, pDCpoints, pRetPoints, pWCpoints, pView)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddULONG         numPoints;	/* number of coords */
	ddDeviceCoord  *pDCpoints;	/* list of device coords */
/* out */
	ddULONG        *pRetPoints;	/* number of coords returned */
	ddCoord3D      *pWCpoints;	/* list of world coords */
	ddUSHORT       *pView;	/* view index */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	ddFLOAT         npc_dc_xform[4][4];
	ddFLOAT         npc_to_wc_xform[4][4];
	ddUSHORT        clipFlags;
	ddNpcSubvolume  clipLimits;
	ddDeviceCoord  *pDCpoint;
	ddCoord3D      *pWCpoint;
	ddCoord4D       pt4d1, pt4d2;
	register int    i;
	ddTableIndex    view = 0;
	int             ptcount = 0, tmpcount = 0;
	ddOrdView      *poview;
	ddBOOL          last_view;

#ifdef DDTEST
	ErrorF("\nMapDcWc\n");
#endif

	*pView = 0;
	*pRetPoints = 0;

	/*
	 * transforms:  wc_to_npc_xform = [view orient][view map]
	 * npc_to_wc_xform = inverse(wc_to_npc_xform) 
	 * npc_to_dc_xform = [Sx 0  0   0]  
	 *           S=Scale [0  Sy 0   0]  
	 *       T=Translate [0  0  Sz  0] 
	 *                   [Tx Ty Tz  1]
	 * dc_to_npc_xform = inverse(npc_to_dc_xform) 
	 * wc_point = [npc_to_wc_xform][npc_point] 
	 * npc_point = * [dc_to_npc_xform][dc_point] 
	 * (dc_point is a 4d column vector of the
	 * given dc with w=1.0)
	 */
	miBldViewport_xform(pwks->pRend, pwks->pRend->pDrawable,
				npc_dc_xform, NULL);
	miMatInverse(npc_dc_xform);

	/*
	 * go through each defined view and find the one which has the most
	 * points in it and the highest priority
	 */
	for (last_view = 0, poview = pwks->views.lowest; !last_view; poview = poview->higher) {
		if (poview->defined) {
			err = get_view(pwks->pRend->lut[PEXViewLUT], poview->first_view, &clipFlags,
				    &clipLimits, npc_to_wc_xform, MI_FALSE);
			if (err != Success)
				return (err);

			for (i = 0, pWCpoint = pWCpoints, pDCpoint = pDCpoints; i < numPoints; i++, pDCpoint++) {
				pt4d1.x = pDCpoint->x;
				pt4d1.y = pDCpoint->y;
				pt4d1.z = pDCpoint->z;
				pt4d1.w = 1.0;
				miTransformPoint(&pt4d1, npc_dc_xform, &pt4d2);
				if (PT_IN_LIMIT(pwks->pRend, &clipLimits, &pt4d2))
					tmpcount++;
			}

			/*
			 * use this view if it has more points.  if it has
			 * same number then this view has higher priority
			 */
			if (tmpcount >= ptcount) {
				view = poview->first_view;
				ptcount = tmpcount;
			}
		}
		last_view = (poview == pwks->views.highest);
	}

	err = get_view(pwks->pRend->lut[PEXViewLUT], view, &clipFlags, &clipLimits,
		       npc_to_wc_xform, MI_TRUE);
	if (err != Success)
		return (err);
	miMatInverse(npc_to_wc_xform);

	for (i = 0, pWCpoint = pWCpoints, pDCpoint = pDCpoints; i < numPoints; i++, pDCpoint++) {
		pt4d1.x = pDCpoint->x;
		pt4d1.y = pDCpoint->y;
		pt4d1.z = pDCpoint->z;
		pt4d1.w = 1.0;
		miTransformPoint(&pt4d1, npc_dc_xform, &pt4d2);
		if (PT_IN_LIMIT(pwks->pRend, &clipLimits, &pt4d2)) {
			miTransformPoint(&pt4d2, npc_to_wc_xform, &pt4d1);
			pWCpoint->x = pt4d1.x;
			pWCpoint->y = pt4d1.y;
			pWCpoint->z = pt4d1.z;
			pWCpoint++;
			(*pRetPoints)++;
		}
	}

	*pView = view;
	return (Success);
}				/* MapDcWc */

/*++
 |
 |  Function Name:	MapWcDc
 |
 |  Function Description:
 |	 Handles the PEXMapWCtoDC request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
MapWcDc(pWKS, numPoints, pWCpoints, view, pRetPoints, pDCpoints)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddULONG         numPoints;	/* number of coords */
	ddCoord3D      *pWCpoints;	/* list of world coords */
	ddTableIndex    view;	/* view index */
/* out */
	ddULONG        *pRetPoints;	/* number of coords returned */
	ddDeviceCoord  *pDCpoints;	/* list of device coords */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	ddFLOAT         wc_to_npc_xform[4][4];
	ddFLOAT         npc_to_dc_xform[4][4];
	ddUSHORT        clipFlags;
	ddNpcSubvolume  clipLimits;
	ddDeviceCoord  *pDCpoint;
	ddCoord3D      *pWCpoint;
	ddCoord4D       pt4d1, pt4d2;
	register int    i;

#ifdef DDTEST
	ErrorF("\nMapWcDc\n");
#endif

	*pRetPoints = 0;

	/*
	 * transforms:  wc_to_npc_xform = [view orient][view map] 
	 * npc_point = [wc_to_npc_xform][wc_point] 
	 *  (wc_point is a 4d column vector of the
	 * given wc with w=1.0) 
	 * npc_to_dc_xform = [Sx 0  0   0]  
	 *           S=Scale [0  Sy 0   0]  
	 *       T=Translate [0  0  Sz  0] 
	 *                   [Tx Ty Tz  1] 
	 * dc_point = [npc_to_dc_xform][npc_point]
	 */
	miBldViewport_xform(pwks->pRend, pwks->pRend->pDrawable,
				npc_to_dc_xform, NULL);

	err = get_view(pwks->pRend->lut[PEXViewLUT], view, &clipFlags, &clipLimits, wc_to_npc_xform, MI_TRUE);
	if (err != Success)
		return (err);

	for (i = 0, pWCpoint = pWCpoints, pDCpoint = pDCpoints; i < numPoints; i++, pWCpoint++) {
		pt4d1.x = pWCpoint->x;
		pt4d1.y = pWCpoint->y;
		pt4d1.z = pWCpoint->z;
		pt4d1.w = 1.0;
		miTransformPoint(&pt4d1, wc_to_npc_xform, &pt4d2);
		if (PT_IN_LIMIT(pwks->pRend, &clipLimits, &pt4d2)) {
			miTransformPoint(&pt4d2, npc_to_dc_xform, &pt4d1);
			pDCpoint->x = pt4d1.x;
			pDCpoint->y = pt4d1.y;
			pDCpoint->z = pt4d1.z;
			pDCpoint++;
			(*pRetPoints)++;
		}
	}

	return (Success);
}				/* MapWcDc */

/*++
 |
 |  Function Name:	SetViewRep
 |
 |  Function Description:
 |	 Handles the PEXSetViewRep request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetViewRep(pWKS, pView)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddViewRep      *pView;	/* view rep */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nSetViewRep\n");
#endif

	/* set the requested tables */
	if (err43 = SetLUTEntries(pwks->reqViewTable, pView->index, 1, (ddPointer) & (pView->view)))
		return (err43);

	if (VISUALIZE_NOW(pwks, VIEW_REP_DYNAMIC)) {
		/* set the current tables */
		if (err43 = SetLUTEntries(pwks->pRend->lut[PEXViewLUT], pView->index, 1,
					  (ddPointer) & (pView->view)))
			return (err43);

		/* set view as defined in view priority list */
		err43 = mi_add_ord_view(&pwks->views, pView->index);
		if (err43)
			return (err43);

		pwks->viewUpdate = PEXNotPending;
		PU_EMPTY_LIST(pwks->pwksList);
		puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

		miDealWithDynamics(WKS_TRANSFORM_DYNAMIC, pwks->pwksList);
	} else {

		/*
		 * set mask to show which entry is pending NOTE: mask is only
		 * big enough for tables <= 32 entries our table only has 6
		 * entries right now
		 */
		pwks->deltaviewMask |= (1L << pView->index);

		pwks->viewUpdate = PEXPending;
		pwks->visualState = PEXDeferred;
	}

	return (Success);
}				/* SetViewRep */

/*++
 |
 |  Function Name:	SetWksWindow
 |
 |  Function Description:
 |	 Handles the PEXSetWKSWindow request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetWksWindow(pWKS, pNpcSubvolume)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddNpcSubvolume *pNpcSubvolume;	/* window volume */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nSetWksWindow\n");
#endif

	pwks->reqNpcSubvolume = *pNpcSubvolume;

	if (VISUALIZE_NOW(pwks, WKS_TRANSFORM_DYNAMIC)) {
		pwks->pRend->npcSubvolume = *pNpcSubvolume;
		pwks->pRend->attrsChanges |= PEXDynNpcSubvolume;

		PU_EMPTY_LIST(pwks->pwksList);
		puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

		miDealWithDynamics(WKS_TRANSFORM_DYNAMIC, pwks->pwksList);
	} else {
		/* don't update display */
		pwks->wksUpdate = PEXPending;
		pwks->visualState = PEXDeferred;
		pwks->wksMask |= WKS_WINDOW_MASK;
	}
	return (Success);
}				/* SetWksWindow */

/*++
 |
 |  Function Name:	SetWksViewport
 |
 |  Function Description:
 |	 Handles the PEXSetWKSViewport request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetWksViewport(pWKS, pViewport)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddViewport     *pViewport;	/* viewport */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nSetWksViewport\n");
#endif

	if ((pwks->pRend->pDrawable == NULL) || (pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

	if (pViewport->useDrawable) {
		pwks->reqviewport.minval.x = 0.0;
		pwks->reqviewport.minval.y = 0.0;
		pwks->reqviewport.minval.z = 0.0;
		pwks->reqviewport.maxval.x = pwks->pRend->pDrawable->width;
		pwks->reqviewport.maxval.y = pwks->pRend->pDrawable->height;
		pwks->reqviewport.maxval.z = 1.0;
	} else
		pwks->reqviewport = *pViewport;

	if (VISUALIZE_NOW(pwks, WKS_TRANSFORM_DYNAMIC)) {
		pwks->pRend->viewport = *pViewport;
		pwks->pRend->attrsChanges |= PEXDynViewport;

		PU_EMPTY_LIST(pwks->pwksList);
		puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

		miDealWithDynamics(WKS_TRANSFORM_DYNAMIC, pwks->pwksList);
	} else {
		/* don't update display */
		pwks->wksUpdate = PEXPending;
		pwks->visualState = PEXDeferred;
		pwks->wksMask |= WKS_VIEWPORT_MASK;
	}

	return (Success);
}				/* SetWksViewport */

/*++
 |
 |  Function Name:	SetHlhsrMode
 |
 |  Function Description:
 |	 Handles the PEXSetHLHSRMode request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetHlhsrMode(pWKS, mode)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddEnumTypeIndex mode;	/* hlhsr mode */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	register int    i, type;

#ifdef DDTEST
	ErrorF("\nSetHlhsrMode\n");
#endif

	if ((pwks->pRend->pDrawable == NULL) || (pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

	MI_WHICHDRAW(pwks->pRend->pDrawable, type);

	/*
	 * make sure this mode is supported by the drawable in this
	 * implementation
	 */
	for (i = 0; i < SI_HLHSR_NUM; i++) {
		if (mode == miHlhsrModeET[type][i].index) {
			pwks->reqhlhsrMode = mode;
			if (WKSIMMEDIATE(pwks, HLHSR_MODE_DYNAMIC))
			{
				pwks->pRend->hlhsrMode = mode;
				pwks->pRend->attrsChanges |= PEXDynHlhsrMode;
			}
			/* hlhsrUpdate doesn't change */
			/* do stuff here to effect change in hlhsr mode */
			else {
				/* don't update display */
				pwks->hlhsrUpdate = PEXPending;
				pwks->visualState = PEXDeferred;
			}
			return (Success);
		}
	}

	return (BadValue);
}				/* SetHlhsrMode */

/*++
 |
 |  Function Name:	SetBufferMode
 |
 |  Function Description:
 |	 Handles the PEXSetBufferMode request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetBufferMode(pWKS, mode)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	ddUSHORT        mode;	/* buffer mode */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	register int    type;

#ifdef DDTEST
	ErrorF("\nSetBufferMode\n");
#endif

	if ((pwks->pRend->pDrawable == NULL) || (pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

	MI_WHICHDRAW(pwks->pRend->pDrawable, type);

	/*
	 * make sure this mode is supported by the drawable in this
	 * implementation
	 */
	if ((mode == PEXSingleBuffered) || (mode == PEXDoubleBuffered)) {
		pwks->reqBufferMode = mode;
		if (WKSIMMEDIATE(pwks, BUFFER_MODIFY_DYNAMIC))
                {
			pwks->curBufferMode = mode;
		/* bufferUpdate doesn't change */
		/* do stuff here to effect change in buffer mode */
                        ChangeDoubleBuffers (pwks);
                }
		else {
			/* don't update display */
			pwks->bufferUpdate = PEXPending;
			pwks->visualState = PEXDeferred;
		}
		return (Success);
	} else
		return (BadValue);
}				/* SetBufferMode */

static int
miAddStructToOrdList(resource, preflist, prity)
	diStructHandle  resource;
	listofOrdStruct *preflist;
	ddFLOAT         prity;
{
	listofOrdStruct *plist = preflist;
	register ddOrdStruct *postruct, *pbefore, *pnew, *pnext;

#ifdef DDTEST
	ErrorF("\tmiAddStructToOrdList \n");
#endif
	err = MI_SUCCESS;

	/*
	 * check to see if it's in the list already and find out where it
	 * goes in the list if it's in the list, set its priority to the new
	 * priority
	 */
	pnew = NULL;		/* set this to point to the struct already in
				 * the list */
	pbefore = NULL;		/* set this to point to where the struct
				 * belongs inthe list */
	postruct = preflist->postruct;	/* the first element is a dummy */

	/*
	 * loop until the end of the list OR the resource is found in the
	 * list and where it belongs in the list is known
	 */
	while (postruct->next && (!pnew || !pbefore)) {
		pnext = postruct->next;
		if ((prity < pnext->priority) && !pbefore)
			pbefore = postruct;
		if (pnext->pstruct == resource) {
			pnew = pnext;	/* remember the old one */
			postruct->next = pnext->next;	/* take it out of the
							 * list */
			err = MI_EXISTERR;
		} else
			postruct = pnext;
	}
	if (!pbefore)
		pbefore = postruct;	/* the last element in the list */

	/* now add it to the list */
	if (!pnew) {
		if ((pnew = (ddOrdStruct *) xalloc(sizeof(ddOrdStruct))) == NULL)
			return (MI_ALLOCERR);
		else
			plist->numStructs++;	/* increment only if it's new
						 * to the list */
	}
	pnew->pstruct = resource;
	pnew->priority = prity;

	pnew->next = pbefore->next;
	pbefore->next = pnew;

	return (err);
}				/* miAddStructToOrdList */

static void
miRemoveStructFromOrdList(resource, preflist)
	diStructHandle  resource;
	listofOrdStruct *preflist;
{
	register ddOrdStruct *postruct, *pnew;

#ifdef DDTEST
	ErrorF("\tmiRemoveStructFromOrdList \n");
#endif

	pnew = NULL;		/* set this to point to the struct already in
				 * the list */
	postruct = preflist->postruct;	/* the first element is a dummy */

	/*
	 * loop until the end of the list OR the resource is found in the
	 * list
	 */
	while (postruct->next && !pnew) {
		if (postruct->next->pstruct == resource)
			pnew = postruct->next;
		else
			postruct = postruct->next;
	}
	if (pnew) {
		postruct->next = pnew->next;
		xfree(pnew);
		preflist->numStructs--;
	}
	return;
}				/* miRemoveStructFromOrdList */

ddBOOL
miGetStructurePriority(pWKS, pStruct, ppriority)
	diWKSHandle     pWKS;	/* workstation handle */
	diStructHandle  pStruct;/* structure handle */
	ddFLOAT        *ppriority;	/* structure priority */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	listofOrdStruct *plist = &pwks->postedStructs;
	register ddOrdStruct *postruct, *pnext;

	postruct = plist->postruct;	/* the first element is a dummy */
	*ppriority = 0;

	while (postruct->next) {
		pnext = postruct->next;
		if (pnext->pstruct == pStruct) {
			*ppriority = pnext->priority;
			return (MI_TRUE);
		} else
			postruct = pnext;
	}
	return (MI_FALSE);
}

/*++
 |
 |  Function Name:	PostStructure
 |
 |  Function Description:
 |	 Handles the PEXPostStructure request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
PostStructure(pWKS, pStruct, priority)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	diStructHandle  pStruct;/* structure handle */
	ddFLOAT         priority;	/* structure priority */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nPostStructure %d\n", pStruct->id);
#endif

	if ((err = miAddStructToOrdList(pStruct, &(pwks->postedStructs), 
			                           priority)) == MI_ALLOCERR)
		return (BadAlloc);

	if (err == MI_SUCCESS) {/* the structure was added */
		/* add wks to structures list */
		if (err4 = UpdateStructRefs(pStruct, (diResourceHandle) pWKS,
					    WORKSTATION_RESOURCE, ADD))
			return (err4);
	}
	/* else the structure was already posted it now has a new priority */

	/* do stuff to see if picture needs to be changed */
	PU_EMPTY_LIST(pwks->pwksList);
	puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

	miDealWithDynamics(POST_STR_DYNAMIC, pwks->pwksList);

	return (Success);
}				/* PostStructure */

/*++
 |
 |  Function Name:	UnpostStructure
 |
 |  Function Description:
 |	 Handles the PEXUnpostStructure request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
UnpostStructure(pWKS, pStruct)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
	diStructHandle  pStruct;/* structure handle */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;

#ifdef DDTEST
	ErrorF("\nUnpostStructure\n");
#endif

	/* take the wks out of the structs wks lists */
	if (err4 = UpdateStructRefs(pStruct, (diResourceHandle) pWKS,
				    WORKSTATION_RESOURCE, REMOVE))
		return (err4);	    /* unlikely */

	if (!pwks) return (Success);	/* workstation already freed */

	/* now, remove the structure from the wks posted list */
	miRemoveStructFromOrdList(pStruct, &(pwks->postedStructs));

	/* do stuff to see if picture needs to be changed */
	PU_EMPTY_LIST(pwks->pwksList);
	puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

	miDealWithDynamics(UNPOST_STR_DYNAMIC, pwks->pwksList);

	return (Success);
}				/* UnpostStructure */

/*++
 |
 |  Function Name:	UnpostAllStructures
 |
 |  Function Description:
 |	 Handles the PEXUnpostAllStructures request.
 |
 |  Note(s):
	don't use UnpostStructure because this follows the list of posted structures
	and removes each one as it goes, instead of having UnpostStructure remove
	them.
 |
 --*/

ddpex4rtn
UnpostAllStructures(pWKS)
/* in */
	diWKSHandle     pWKS;	/* workstation handle */
/* out */
{
	miWksPtr        pwks = (miWksPtr) pWKS->deviceData;
	ddOrdStruct    *pos, *posn;

#ifdef DDTEST
	ErrorF("\nUnpostAllStructures\n");
#endif

	if (!pwks) return (Success);

	/* don't forget that the first pos is a dummy */
	pos = pwks->postedStructs.postruct;
	posn = pos->next;
	pos->next = NULL;
	while (posn) {

		/*
		 * take the wks out of the posted structs and childrens wks
		 * lists
		 */
		err4 = UpdateStructRefs(posn->pstruct, (diResourceHandle) pWKS,
					WORKSTATION_RESOURCE, REMOVE);

		pos = posn;
		posn = pos->next;
		xfree(pos);
	}
	pwks->postedStructs.numStructs = 0;

	/* do stuff to see if picture needs to be changed */
	PU_EMPTY_LIST(pwks->pwksList);
	puAddToList((ddPointer) & pWKS, (ddULONG) 1, pwks->pwksList);

	miDealWithDynamics(UNPOST_STR_DYNAMIC, pwks->pwksList);
	return (Success);
}				/* UnpostAllStructures */

/*++
 |
 |  Function Name:	InquireWksPostings
 |
 |  Function Description:
 |	 Handles the PEXGetWKSPostings request.
 |
 |  Note(s):
 |
 --*/

extern ddpex4rtn get_wks_postings();

ddpex4rtn
InquireWksPostings(pStruct, pBuffer)
/* in */
	diStructHandle  pStruct;/* structure handle */
/* out */
	ddBufferPtr     pBuffer;/* list of workstation ids */
{

#ifdef DDTEST
	ErrorF("\nInquireWksPostings\n");
#endif

	return (get_wks_postings(pStruct, pBuffer));
}				/* InquireWksPostings */

/*++
 |
 |  Function Name:	UpdateWksRefs
 |
 |  Function Description:
 |	only for pick measure reference count
 |
 |  Note(s):
 |
 --*/

void
UpdateWksRefs(pWKS, pResource, which, action)
	diWKSHandle     pWKS;
	diResourceHandle pResource;	/* pick measure resource */
	ddResourceType  which;
	ddAction        action;
{
	register miWksPtr pwks = (miWksPtr) pWKS->deviceData;

	/* only pick measure counts are used here */
	if (action == ADD)
		pwks->refCount++;
	else if (pwks->refCount > 0)
		pwks->refCount--;

	CHECK_DELETE(pwks, pWKS);

	return;
}				/* UpdateWksRefs */

ddpex4rtn
mi_add_ord_view(plist, view)
	listofOrdView  *plist;
	ddUSHORT        view;
{
	ddOrdView      *free1, *free2, *indexer;

	indexer = plist->highest->lower;
	do {
		if ((indexer->first_view == view) &&
		    (indexer->last_view == view)) {
			/* view is already in the list */
			if (!indexer->defined) {
				plist->defined_views++;
				indexer->defined = MI_TRUE;
			}
			return (Success);
		}
		if ((indexer->first_view <= view) &&
		    (indexer->last_view >= view)) {
			MIWKS_NEW_OV_ENTRY(plist, free1);
			if (free1 == NULL) {
				/* bad news, no more free entries */
				return (BadValue);
			}
			plist->defined_views++;
			free1->defined = MI_TRUE;
			free1->first_view = view;
			free1->last_view = view;
			if (indexer->first_view == view) {	/* new entry goes in
								 * front */
				free1->higher = indexer->higher;
				free1->lower = indexer;
				indexer->higher = free1;
				free1->higher->lower = free1;
				indexer->first_view++;
				ASSURE(indexer->first_view <= indexer->last_view);
			} else if (indexer->last_view == view) {	/* new entry goes on
									 * back */
				free1->higher = indexer;
				free1->lower = indexer->lower;
				indexer->lower = free1;
				free1->lower->higher = free1;
				indexer->last_view--;
				ASSURE(indexer->first_view <= indexer->last_view);
			} else {/* new entry goes in middle - need to split
				 * the range. the end of the range goes into
				 * a new entry. ranges are always not defined
				 * views, so all views in that new entry are
				 * still undefined */
				MIWKS_NEW_OV_ENTRY(plist, free2);
				free2->defined = MI_FALSE;
				free2->first_view = view + 1;
				free2->last_view =
					indexer->last_view;
				indexer->last_view = view - 1;
				free1->higher = indexer;
				free1->lower = free2;
				free2->higher = free1;
				free2->lower = indexer->lower;
				indexer->lower = free1;
				free2->lower->higher = free2;
			}
			return (Success);
		}
		indexer = indexer->lower;
	} while (indexer != plist->lowest);
	/* reached end of list: should never get here */
	return (BadValue);

}

ddOrdView      *
mi_find_ord_view(plist, view)
	listofOrdView  *plist;
	ddUSHORT        view;
{
/* finds only defined views */
	ddOrdView      *indexer;

	indexer = plist->highest;
	do {
		if (indexer->defined && (indexer->first_view == view))
			return (indexer);

		indexer = indexer->lower;
	} while (indexer != NULL);
	/* view is not defined */
	return (NULL);
}

void
initialize_pick_devices()
{
	register int    i;

	for (i = 0; i < MIWKS_NUM_PICK_DEVICES; i++) {
		pick_devices[i].type = i + 1;
		pick_devices[i].status = PEXNoPick;
		/* don't put the path in here - create it for the wks devices */
		pick_devices[i].path = (listofObj *) NULL;
		pick_devices[i].pathOrder = PEXTopPart;
		pick_devices[i].inclusion = (diNSHandle) NULL;
		pick_devices[i].exclusion = (diNSHandle) NULL;

		if (!i)
			MIWKS_PD_DATA_REC_1(&pick_devices[i]) = 0;
		else
			MIWKS_PD_DATA_REC_2(&pick_devices[i]) = 0;

		pick_devices[i].pet = PEXEchoPrimitive;
		pick_devices[i].echoVolume = viewportInit;
		pick_devices[i].echoSwitch = PEXNoEcho;
	}
}
