/* $Xorg: miRender.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level3/miRender.c,v 3.7 2001/12/14 19:57:33 dawes Exp $ */

#include "miLUT.h"
#include "dipex.h"
#include "ddpex3.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "PEXErr.h"
#include "pexUtils.h"
#include "pixmap.h"
#include "windowstr.h"
#include "regionstr.h"
#include "miscstruct.h"
#include "dixstruct.h"
#include "miRender.h"
#include "miStruct.h"
#include "miStrMacro.h"
#include "miWks.h"
#include "ddpex4.h"
#include "gcstruct.h"
#include "pexos.h"


/* External variables used */

extern  void		mi_set_filters();
extern	void		miMatMult();
extern	ddpex3rtn	miBldViewport_xform();
extern	ddpex3rtn	miBldCC_xform();
extern	ocTableType	ParseOCTable[];
extern  void		(*DestroyOCTable[])();
extern  ocTableType	InitExecuteOCTable[];
extern  ocTableType	PickExecuteOCTable[];
extern  ocTableType	SearchExecuteOCTable[];
extern  RendTableType	RenderPrimitiveTable[];
extern  RendTableType	PickPrimitiveTable[];

/* pcflag is initialized in  ddpexInit() */
ddBOOL       pcflag;
ddPCAttr     defaultPCAttr;
#define	MI_GET_DEFAULT_PC(pPC)	\
	if (!pcflag) {		\
		DefaultPC(pPC);	\
		pcflag = MI_TRUE;  }

ddFLOAT		ident4x4[4][4] = {
    {1.0, 0.0, 0.0, 0.0},
    {0.0, 1.0, 0.0, 0.0},
    {0.0, 0.0, 1.0, 0.0},
    {0.0, 0.0, 0.0, 1.0}
};


/* Level III Rendering Procedures */

/*++
 |
 |  Function Name:	InitRenderer
 |
 |  Function Description:
 |	 Initializes the dd stuff in the renderer for the
 |	 PEXCreateRenderer request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
InitRenderer(pRend)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
{

    extern GCPtr	CreateScratchGC();
    extern ddpex3rtn	CreateDDContext();

    ddpex3rtn		err = Success;


#ifdef DDTEST
    ErrorF( " InitRenderer\n");
#endif

    /* set renderer dynamics */
    /* for now, set them all to be dynamic regardless of drawable type 
     * bit value of 0 means dynamic modifications
     * bit value of 1 means no dynamic modifications
     * todo: change this to be easy to specify individual dynamics
     * OR with PEXDynxxx for static; OR with ~PEXDynxxx for dynamics
     */
    /* since we don't actually make copies of namesets
     * or luts so that their values are bound at BeginRendering,
     * any change made in those tables/ns will be noticed anytime
     * info is read from an lut/ns, therefore, the tables and namesets
     * are dynamics and anytime a change is made, anything affected
     * by that change must be updated. The update is done in
     * ValidateRenderer, which is called at appropriate times to
     * make sure everything is updated.
     */
    pRend->tablesMask = 0;
    pRend->namesetsMask = 0;
    pRend->attrsMask = 0;

    /*
     * Create a DDContext and associate it with the Renderer
     */
    if (err = CreateDDContext(pRend)) return(err);

    /* copy the initial oc functions to the OC table */
    pRend->render_mode = MI_REND_DRAWING;
    memcpy(  (char *)pRend->executeOCs,
	    (char *)InitExecuteOCTable, 
	    sizeof(ocTableType)*OCTABLE_LENGTH); 

    MI_SET_ALL_CHANGES(pRend);
    ValidateRenderer(pRend);

    return (Success);
}

/*++
 |
 |  Function Name:	DefaultPC
 |
 |  Function Description:
 |	 Initializes a global static copy of the PC to the default values.
 |	 This copy is, in turn, used to initialize PC's the the initial values.
 |
 |  Note(s):
 |
 --*/

void
DefaultPC(pPC)
    ddPCAttr	   *pPC;
{
    pPC->markerType = PEXMarkerAsterisk;
    pPC->markerScale = 1.0;
    pPC->markerColour.colourType = PEXIndexedColour;
    pPC->markerColour.colour.indexed.index = 1;
    pPC->markerIndex = 1;
    pPC->textFont = 1;
    pPC->textPrecision = PEXStringPrecision;
    pPC->charExpansion = 1.0;
    pPC->charSpacing = 0.0;
    pPC->textColour.colourType = PEXIndexedColour;
    pPC->textColour.colour.indexed.index = 1;
    pPC->charHeight = 0.01;
    pPC->charUp.x = 0.0;
    pPC->charUp.y = 1.0;
    pPC->textPath = PEXPathRight;
    pPC->textAlignment.vertical = PEXValignNormal;
    pPC->textAlignment.horizontal = PEXHalignNormal;
    pPC->atextHeight = 0.01;
    pPC->atextUp.x = 0.0;
    pPC->atextUp.y = 1.0;
    pPC->atextPath = PEXPathRight;
    pPC->atextAlignment.vertical = PEXValignNormal;
    pPC->atextAlignment.horizontal = PEXHalignNormal;
    pPC->atextStyle = PEXATextNotConnected;
    pPC->textIndex = 1;
    pPC->lineType = PEXLineTypeSolid;
    pPC->lineWidth = 1.0;
    pPC->lineColour.colourType = PEXIndexedColour;
    pPC->lineColour.colour.indexed.index = 1;
    pPC->curveApprox.approxMethod = PEXApproxConstantBetweenKnots;
    pPC->curveApprox.tolerance = 1.0;
    pPC->lineInterp = PEXPolylineInterpNone;
    pPC->lineIndex = 1;
    pPC->intStyle = PEXInteriorStyleHollow;
    pPC->intStyleIndex = 1;
    pPC->surfaceColour.colourType = PEXIndexedColour;
    pPC->surfaceColour.colour.indexed.index = 1;
    pPC->reflAttr.ambient = 1.0;
    pPC->reflAttr.diffuse = 1.0;
    pPC->reflAttr.specular = 1.0;
    pPC->reflAttr.specularConc = 0.0;
    pPC->reflAttr.transmission = 0.0;
    pPC->reflAttr.specularColour.colourType = PEXIndexedColour;
    pPC->reflAttr.specularColour.colour.indexed.index = 1;
    pPC->reflModel = PEXReflectionNoShading;
    pPC->surfInterp = PEXSurfaceInterpNone;
    pPC->bfIntStyle = PEXInteriorStyleHollow;
    pPC->bfIntStyleIndex = 1;
    pPC->bfSurfColour.colourType = PEXIndexedColour;
    pPC->bfSurfColour.colour.indexed.index = 1;
    pPC->bfReflAttr.ambient = 1.0;
    pPC->bfReflAttr.diffuse = 1.0;
    pPC->bfReflAttr.specular = 1.0;
    pPC->bfReflAttr.specularConc = 0.0;
    pPC->bfReflAttr.transmission = 0.0;
    pPC->bfReflAttr.specularColour.colourType = PEXIndexedColour;
    pPC->bfReflAttr.specularColour.colour.indexed.index = 1;
    pPC->bfReflModel = PEXReflectionNoShading;
    pPC->bfSurfInterp = PEXSurfaceInterpNone;

    pPC->surfApprox.approxMethod = PEXApproxConstantBetweenKnots;
    pPC->surfApprox.uTolerance = 1.0;
    pPC->surfApprox.vTolerance = 1.0;

    pPC->cullMode = 0;
    pPC->distFlag = FALSE;
    pPC->patternSize.x = 1.0;
    pPC->patternSize.y = 1.0;
    pPC->patternRefPt.x = 0.0;
    pPC->patternRefPt.y = 0.0;
    pPC->patternRefPt.z = 0.0;
    pPC->patternRefV1.x = 1.0;
    pPC->patternRefV1.y = 0.0;
    pPC->patternRefV1.z = 0.0;
    pPC->patternRefV2.x = 0.0;
    pPC->patternRefV2.y = 1.0;
    pPC->patternRefV2.z = 0.0;
    pPC->intIndex = 1;
    pPC->edges = PEXOff;
    pPC->edgeType = PEXSurfaceEdgeSolid;
    pPC->edgeWidth = 1.0;
    pPC->edgeColour.colourType = PEXIndexedColour;
    pPC->edgeColour.colour.indexed.index = 1;
    pPC->edgeIndex = 1;
    memcpy( (char *) pPC->localMat, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    memcpy( (char *) pPC->globalMat, (char *) ident4x4, 16 * sizeof(ddFLOAT));
    pPC->modelClip = PEXNoClip;
    pPC->modelClipVolume = puCreateList(DD_HALF_SPACE);
    pPC->viewIndex = 0;
    pPC->lightState = puCreateList(DD_INDEX);
    pPC->depthCueIndex = 0;
    pPC->colourApproxIndex = 0;
    pPC->rdrColourModel = PEXRdrColourModelRGB;
    pPC->psc.type = PEXPSCNone;
    pPC->psc.data.none = '0';
    pPC->asfs = ~0L;
    pPC->pickId = 0;
    pPC->hlhsrType = 0;
    pPC->pCurrentNS = NULL;

    return;
}

/*++
 |
 |  Function Name:	InquireRendererDynamics
 |
 |  Function Description:
 |	 Supports the PEXGetRendererDynamics request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
InquireRendererDynamics(pRend, pTablesMask, pNSMask, pAttrMask)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
    ddBitmask	  *pTablesMask;	/* dynamics mask for luts */
    ddBitmask	  *pNSMask;  /* dynamics mask for name sets */
    ddBitmask	  *pAttrMask;/* dynamics mask for renderer attributes */
{

#ifdef DDTEST
    ErrorF( " InquireRendererDynamics\n");
#endif

    *pTablesMask = pRend->tablesMask;
    *pNSMask = pRend->namesetsMask;
    *pAttrMask = pRend->attrsMask;

    return (Success);
}

/*++
 |
 |  Function Name:	RenderOCs
 |
 |  Function Description:
 |	 Supports the PEXRenderOutputCommands request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
RenderOCs(pRend, numOCs, pOCs)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    ddULONG		numOCs;
    ddElementInfo	*pOCs;
/* out */
{
	register ddElementInfo	*poc;
	miGenericElementPtr	pexoc;
	ddpex2rtn		err = Success;
	XID			fakestr;
	diStructHandle 		sh = 0, ph;
	pexStructure 		*ps;
	pexOutputCommandError 	*pErr;
	ddULONG            	offset1, offset2, numberOCs;
	miTraverserState      	trav_state;
	diPMHandle            	pPM = (diPMHandle) NULL;
	unsigned long 		PEXStructType;
	miStructPtr 		pheader;
	ddPickPath    		*strpp;
	miPPLevel		*travpp;
	int			i, ROCdepth;
        ddUSHORT          	serverstate;    


#ifdef DDTEST
    ErrorF( " RenderOCs\n");
#endif

    /* if renderer idle ignore.... */
    if (pRend->state == PEXIdle)
      return Success;

    ValidateRenderer(pRend);

    /* state == PEXPicking, call through traverser */
    if (pRend->state == PEXPicking) {

	/* get the structure handle from the last pickpath in the list of 
	   fake structures (these are added by BeginStructure)
	*/
	ROCdepth = (pRend->pickstr.fakeStrlist)->numObj-1;
	strpp = (ddPickPath *)(pRend->pickstr.fakeStrlist)->pList;
	sh = strpp[ROCdepth].structure;

	/* set up incoming state properly for traverser */
	if (ROCdepth > 0) {
	    travpp = (miPPLevel *)  xalloc(sizeof(miPPLevel));
	    trav_state.p_pick_path = travpp;
	    travpp->pp = strpp[ROCdepth-1];
	    for (i = ROCdepth-2; i >= 0; i--) {
		travpp->up = (miPPLevel *)  xalloc(sizeof(miPPLevel));
		travpp = travpp->up;
		travpp->pp = strpp[i];
	    }
	}


	/* now do the work of storing stuff into the structure */
	numberOCs = numOCs;
	for ( poc=pOCs; numberOCs>0; numberOCs-- )
	{

	    err = StoreElements( sh, 1, poc, &pErr);
	    if (err != Success) return(err);

	    poc += poc->length;	
	}

	/* now call the traverser to traverse this structure */
        /* set exec_str_flag */
	trav_state.exec_str_flag = ES_YES;
	trav_state.p_curr_pick_el = (ddPickPath *) NULL;
	trav_state.p_curr_sc_el = (ddElementRef *) NULL;
	trav_state.max_depth = ROCdepth;
	trav_state.pickId =  strpp[ROCdepth].pickid;
	trav_state.ROCoffset =  strpp[ROCdepth].offset;
	pPM = pRend->pickstr.pseudoPM;

	/* turn off this flag so BeginStructure calls made inside traverser
	   do not cause fake structures to be allocated
	*/
	serverstate = pRend->pickstr.server;
	pRend->pickstr.server = DD_NEITHER;
	offset1 = 1;
	offset2 = numOCs;

	err = traverser(pRend, sh, offset1, offset2, pPM, NULL, &trav_state);

	/* restore the state flag */
	pRend->pickstr.server = serverstate;

	/* save pickid returned by traverser */
	strpp[ROCdepth].pickid = trav_state.pickId;
	strpp[ROCdepth].offset += numOCs;

        /* clean up structure */
        {
          miStructPtr pheader = (miStructPtr) sh->deviceData;
          extern cssTableType DestroyCSSElementTable[];
 
          MISTR_DEL_ELS(sh, pheader, 1, numOCs);
          MISTR_CURR_EL_PTR(pheader) = MISTR_ZERO_EL(pheader);
          MISTR_CURR_EL_OFFSET(pheader) = 0;
 
        }

    }
    else { 
    /* state == PEXRendering, call directly to level 2 for efficiency */
	for ( poc=pOCs; numOCs>0; numOCs-- )
	{
	    switch( poc->elementType ) {
	     /* drawing primitives */
	      case PEXOCMarker:
	      case PEXOCMarker2D:
	      case PEXOCText:
	      case PEXOCText2D:
	      case PEXOCAnnotationText:
	      case PEXOCAnnotationText2D:
	      case PEXOCPolyline:
	      case PEXOCPolyline2D:
	      case PEXOCPolylineSet:
	      case PEXOCNurbCurve:
	      case PEXOCFillArea:
	      case PEXOCFillArea2D:
	      case PEXOCExtFillArea:
	      case PEXOCFillAreaSet:
	      case PEXOCFillAreaSet2D:
	      case PEXOCExtFillAreaSet:
	      case PEXOCTriangleStrip:
	      case PEXOCQuadrilateralMesh:
	      case PEXOCSOFAS:
	      case PEXOCNurbSurface:
	      case PEXOCCellArray:
	      case PEXOCCellArray2D:
	      case PEXOCExtCellArray:
	      case PEXOCGdp:
	      case PEXOCGdp2D:

		    /* drop out if not doing primitives
		     * otherwise fall through */
		    if (!MI_DDC_DO_PRIMS(pRend)) 
		       break; 

	      default:
		/* if a Proprietary OC bump the counter and continue */
		if (MI_HIGHBIT_ON((int)poc->elementType)) {
		    ((ddElementRef *)pRend->curPath->pList)
			[pRend->curPath->numObj - 1].offset++;
		    break;
		}
		else {
		    /* not Proprietary see if valid PEX OC */
		    if (MI_IS_PEX_OC((int)poc->elementType)){

			pexoc = 0;
			err = ParseOCTable[ (int)poc->elementType ]
					  ( (ddPointer)poc, &pexoc );
		    }
		    else
			err = !Success;
		  }
		 
		if (err != Success)
		    return( PEXERR(PEXOutputCommandError) );
	
		/* If we make it here it is a valid OC no more checking to do */

		/* add one to the current_path's element offset if a 
		 * begin structure has been done
		 */
		if (pRend->curPath->numObj)
		    ((ddElementRef *)pRend->curPath->pList)[pRend->curPath->numObj - 1].offset++;
		pRend->executeOCs[ (int)poc->elementType ]( pRend, &pexoc->element );

		DestroyOCTable[ (int)poc->elementType ](  pexoc );
	    }

	    poc += poc->length;	/* length is in four byte units & sizeof(poc) is 4 */
	}
    }

    return (err);
}

ddpex3rtn
convertoffset(pstruct, ppos, poffset)
/* in */
    miStructStr    *pstruct;  /* pointer to the structure involved */
    ddElementPos   *ppos;     /* the position information */
/* out */
    ddULONG        *poffset;  /* valid offset calculated from the postition */

{
	/* shamelessly lifted from the pos2offset routine in miStruct.c */

        ddUSHORT        whence = ppos->whence;
	ddLONG          offset = ppos->offset, temp;

	switch (whence) {
	    case PEXBeginning:
                temp = offset;
		break;

	    case PEXCurrent:
                temp = MISTR_CURR_EL_OFFSET(pstruct) + offset;
		break;

	    case PEXEnd:
                /* numElements is the same as the last elements offset */
		temp = MISTR_NUM_EL(pstruct) + offset;
		break;

	    default:
                /* value error */
		return (BadValue);
		break;
        }

        /* now check that the new offset is in range of the structure */
		if (temp < 0)
		    *poffset = 0;
		else if (temp > MISTR_NUM_EL(pstruct))
		    *poffset = MISTR_NUM_EL(pstruct);
		else
		    *poffset = temp;

        return (Success);
			
}



/*++
 |
 |  Function Name:	RenderElements
 |
 |  Function Description:
 |	 Supports the PEXRenderElements request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
RenderElements(pRend, pStr, range)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    diStructHandle      pStr;
    ddElementRange      *range;
/* out */
{
    ddpex3rtn		err = Success;
    miStructPtr         pstruct;
    miGenericElementPtr pel;
    ddULONG             offset1, offset2, i;
    miTraverserState      trav_state;
    diPMHandle            pPM = (diPMHandle) NULL;
    miDDContext           *pddc = (miDDContext *) pRend->pDDContext;
    int                 eltype, ROCdepth, j;
    ddPickPath          *strpp, sIDpp, *sIDlist;
    miPPLevel           *travpp;
    XID                 fakeStrID;
    diStructHandle      sh = 0, REfakeStr;
    ddUSHORT		serverstate;
    ddElementPos	REfakePos;




    /* if renderer idle ignore.... */
    if (pRend->state == PEXIdle)
      return Success;

    pstruct = (miStructPtr) pStr->deviceData;

    /* convert the offset based on whence value */
    if (convertoffset(pstruct, &(range->position1), &offset1))
	    return (BadValue);      /* bad whence value */

    if (convertoffset(pstruct, &(range->position2), &offset2))
	    return (BadValue);      /* bad whence value */

    /* flip the range if necessary */
    if (offset1 > offset2) {
	    i = offset1;
	    offset1 = offset2;
	    offset2 = i;
    }

    /* return early if offsets out of range */
    if (offset1 == 0)
	if (offset2 == 0)
	      return(Success);
	else
	      offset1 = 1;

    ValidateRenderer(pRend);

    if (pRend->state == PEXPicking) {

	/* in client side picking RenderElements could be called
	   at different levels of nested Begin Structures. In
	   order to uniquely store a correspondence between 
	   structure handles and the correct ID a fake structure 
	   must be allocated
	*/

        REfakeStr = (diStructHandle)xalloc((unsigned long)
                                              sizeof(ddStructResource));
        if (!REfakeStr) return (BadAlloc);
        REfakeStr->id = -666;
        err = CreateStructure(REfakeStr);
        if (err != Success) {
            xfree((pointer)(REfakeStr));
            return (err);
        }

	/* now copy the desired elements out of the structure passed in
	   and into the fake structure
	*/
	REfakePos.whence = PEXBeginning;
	REfakePos.offset = 0;
	err = CopyElements(pStr, range, REfakeStr, &REfakePos);
        if (err != Success) {
            xfree((pointer)(REfakeStr));
            return (err);
        }


	/* need to handle case where RenderElements is called
	   after a ROC that may or may not have Begin/End structures
	   nested in it
	*/
        ROCdepth = (pRend->pickstr.fakeStrlist)->numObj-1;
        strpp = (ddPickPath *)(pRend->pickstr.fakeStrlist)->pList;
        sh = strpp[ROCdepth].structure;


        /* set up incoming state properly for traverser */
        if (ROCdepth > 0) {
            travpp = (miPPLevel *)  xalloc(sizeof(miPPLevel));
            trav_state.p_pick_path = travpp;
            travpp->pp = strpp[ROCdepth-1];
            for (j = ROCdepth-2; j >= 0; j--) {
                travpp->up = (miPPLevel *)  xalloc(sizeof(miPPLevel));
                travpp = travpp->up;
                travpp->pp = strpp[j];
            }
        }


	/* set exec_str_flag */
	trav_state.exec_str_flag = ES_YES;
	trav_state.p_curr_pick_el = (ddPickPath *) NULL;
	trav_state.p_curr_sc_el = (ddElementRef *) NULL;
	trav_state.max_depth = ROCdepth;
	trav_state.pickId = strpp[ROCdepth].pickid;
	trav_state.ROCoffset =  strpp[ROCdepth].offset;

	pPM = pRend->pickstr.pseudoPM;

        /* turn off this flag so BeginStructure calls made inside traverser
          do not cause fake structures to be allocated
        */
        serverstate = pRend->pickstr.server;
        pRend->pickstr.server = DD_NEITHER;
	/* redefine the offsets into the fake structure from the originals */
	offset2 = (offset2 - offset1 + 1);
        offset1 = 1;
	
	err = traverser(pRend, REfakeStr, offset1, offset2, pPM, NULL, &trav_state);

        /* restore the state flag */
        pRend->pickstr.server = serverstate;

        /* save pickid returned by traverser */
        strpp[ROCdepth].pickid = trav_state.pickId;
        strpp[ROCdepth].offset += offset2;

	/* now find the ID that corresponds to the handle sh and
	   save that as the corresponding ID for pStr in the sIDlist
	   note that the IDs ARE stored in the pickid field
	*/
        sIDpp.structure = REfakeStr;
        sIDpp.offset = 0;
	sIDlist = (ddPickPath *) (pRend->pickstr.sIDlist)->pList;
	for (j = 0; j < (pRend->pickstr.sIDlist)->numObj; j++, sIDlist++) 
	    if (sh == sIDlist->structure) {
		sIDpp.pickid = sIDlist->pickid;
		break;
	    }

        err = puAddToList((ddPointer) &sIDpp, (ddULONG) 1, pRend->pickstr.sIDlist);
        if (err != Success)   return (err);
 
        /* clean up structure */
        {
          miStructPtr pheader = (miStructPtr) REfakeStr->deviceData;
          extern cssTableType DestroyCSSElementTable[];
 
          MISTR_DEL_ELS(REfakeStr, pheader, offset1, offset2);
          MISTR_CURR_EL_PTR(pheader) = MISTR_ZERO_EL(pheader);
          MISTR_CURR_EL_OFFSET(pheader) = 0;
 
        }

    }
    else {
    /* state == PEXRendering call directly into level 2 for efficiency */
	for (i = offset1; i <= offset2; i++){

		/* set the element pointer */
		if ( i == offset1) {
		    MISTR_FIND_EL(pstruct, offset1, pel);
		}
		else
		    pel = MISTR_NEXT_EL(pel);

		eltype = MISTR_EL_TYPE (pel);

		switch (eltype) {
		   /* drawing primitives */
		    case PEXOCMarker:
		    case PEXOCMarker2D:
		    case PEXOCText:
		    case PEXOCText2D:
		    case PEXOCAnnotationText:
		    case PEXOCAnnotationText2D:
		    case PEXOCPolyline:
		    case PEXOCPolyline2D:
		    case PEXOCPolylineSet:
		    case PEXOCNurbCurve:
		    case PEXOCFillArea:
		    case PEXOCFillArea2D:
		    case PEXOCExtFillArea:
		    case PEXOCFillAreaSet:
		    case PEXOCFillAreaSet2D:
		    case PEXOCExtFillAreaSet:
		    case PEXOCTriangleStrip:
		    case PEXOCQuadrilateralMesh:
		    case PEXOCSOFAS:
		    case PEXOCNurbSurface:
		    case PEXOCCellArray:
		    case PEXOCCellArray2D:
		    case PEXOCExtCellArray:
		    case PEXOCGdp:

		    /* drop out if not doing primitives
		     * otherwise fall through */
		     if (!MI_DDC_DO_PRIMS(pRend))
			break;
		default:
		    /* if a Proprietary OC call the correct routine */
		    if (MI_HIGHBIT_ON(eltype)) {
			pRend->executeOCs[MI_OC_PROP]( pRend,
					    (ddPointer)&(MISTR_EL_DATA (pel)));
		    }
		    else {
			/* not Proprietary see if valid PEX OC */
			if (MI_IS_PEX_OC(eltype))
			    pRend->executeOCs[ eltype]( pRend,
					    (ddPointer)&(MISTR_EL_DATA (pel)));
			else
			    err = !Success;
		    }

		    if (err != Success)
			return( PEXERR(PEXOutputCommandError) );
		 
	    }
	}
    }	

    return(err);
}

/*++
 |
 |  Function Name:      AccumulateState	
 |
 |  Function Description:
 |	 Supports the PEXAccumulateState request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
AccumulateState(pRend,  pAccSt )
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
ddAccStPtr          pAccSt;	  /* accumulate state handle */
/* out */
{
    register int	depth, offset;
    ddpex3rtn		err = Success;
    ddElementRef	*elemRef;
    miStructPtr		structPtr;
    miGenericElementPtr	elemPtr;

    /* if renderer idle ignore.... */
    if (pRend->state == PEXIdle)
      return Success;

    ValidateRenderer(pRend);

    /* The path has already been validated */
    
    elemRef = (ddElementRef *) pAccSt->Path->pList;
    for (depth = 1; depth <= pAccSt->numElRefs; depth++) {
	structPtr = (miStructPtr) elemRef->structure->deviceData;
	elemPtr = MISTR_NEXT_EL (MISTR_ZERO_EL (structPtr));
	for (offset = 0; offset < elemRef->offset; offset++) {
	    switch (MISTR_EL_TYPE (elemPtr)) {
	    case PEXOCMarkerType:
	    case PEXOCMarkerScale:
	    case PEXOCMarkerColourIndex:
	    case PEXOCMarkerColour:
	    case PEXOCMarkerBundleIndex:
	    case PEXOCTextFontIndex:
	    case PEXOCTextPrecision:
	    case PEXOCCharExpansion:
	    case PEXOCCharSpacing:
	    case PEXOCTextColourIndex:
	    case PEXOCTextColour:
	    case PEXOCCharHeight:
	    case PEXOCCharUpVector:
	    case PEXOCTextPath:
	    case PEXOCTextAlignment:
	    case PEXOCAtextHeight:
	    case PEXOCAtextUpVector:
	    case PEXOCAtextPath:
	    case PEXOCAtextAlignment:
	    case PEXOCAtextStyle:
	    case PEXOCTextBundleIndex:
	    case PEXOCLineType:
	    case PEXOCLineWidth:
	    case PEXOCLineColourIndex:
	    case PEXOCLineColour:
	    case PEXOCCurveApproximation:
	    case PEXOCPolylineInterp:
	    case PEXOCLineBundleIndex:
	    case PEXOCInteriorStyle:
	    case PEXOCInteriorStyleIndex:
	    case PEXOCSurfaceColourIndex:
	    case PEXOCSurfaceColour:
	    case PEXOCSurfaceReflAttr:
	    case PEXOCSurfaceReflModel:
	    case PEXOCSurfaceInterp:
	    case PEXOCBfInteriorStyle:
	    case PEXOCBfInteriorStyleIndex:
	    case PEXOCBfSurfaceColourIndex:
	    case PEXOCBfSurfaceColour:
	    case PEXOCBfSurfaceReflAttr:
	    case PEXOCBfSurfaceReflModel:
	    case PEXOCBfSurfaceInterp:
	    case PEXOCSurfaceApproximation:
	    case PEXOCCullingMode:
	    case PEXOCDistinguishFlag:
	    case PEXOCPatternSize:
	    case PEXOCPatternRefPt:
	    case PEXOCPatternAttr:
	    case PEXOCInteriorBundleIndex:
	    case PEXOCSurfaceEdgeFlag:
	    case PEXOCSurfaceEdgeType:
	    case PEXOCSurfaceEdgeWidth:
	    case PEXOCSurfaceEdgeColourIndex:
	    case PEXOCSurfaceEdgeColour:
	    case PEXOCEdgeBundleIndex:
	    case PEXOCSetAsfValues:
	    case PEXOCLocalTransform:
	    case PEXOCLocalTransform2D:
	    case PEXOCGlobalTransform:
	    case PEXOCGlobalTransform2D:
	    case PEXOCModelClip:
	    case PEXOCModelClipVolume:
	    case PEXOCModelClipVolume2D:
	    case PEXOCRestoreModelClip:
	    case PEXOCViewIndex:
	    case PEXOCLightState:
	    case PEXOCDepthCueIndex:
	    case PEXOCPickId:
	    case PEXOCHlhsrIdentifier:
	    case PEXOCColourApproxIndex:
	    case PEXOCRenderingColourModel:
	    case PEXOCParaSurfCharacteristics:
	    case PEXOCAddToNameSet:
	    case PEXOCRemoveFromNameSet:
		/* if a Proprietary OC call the correct routine */
		if (MI_HIGHBIT_ON(MISTR_EL_TYPE (elemPtr))) {
		    pRend->executeOCs[MI_OC_PROP]( pRend,
				    (ddPointer)&(MISTR_EL_DATA (elemPtr)));
		}
		else {
		    /* not Proprietary see if valid PEX OC */
		    if (MI_IS_PEX_OC(MISTR_EL_TYPE (elemPtr)))
			pRend->executeOCs[(int) MISTR_EL_TYPE (elemPtr)]( pRend,
				     (ddPointer)&(MISTR_EL_DATA (elemPtr)));
		    else
			err = !Success;
		}

		if (err != Success)
		    return( PEXERR(PEXOutputCommandError) );
		 
		break;
	    default:
		break;
	    }

	    elemPtr = MISTR_NEXT_EL (elemPtr);
	}

	elemRef++;
    }

  return(err);
}


/*++
 |
 |  Function Name:	init_def_matrix
 |
 |  Note(s):
 |
 --*/

void
init_def_matrix (matrix)
ddFLOAT matrix[4][4];
{
  memcpy( (char *) matrix, (char *) ident4x4, 16 * sizeof(ddFLOAT));
}


/*++
 |
 |  Function Name:	init_pipeline
 |
 |  Function Description:
 |	 does stuff common to BeginRendering, BeginPicking, BeginSearching
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
init_pipeline(pRend, pDrawable)
/* in */
    ddRendererPtr	pRend;	  /* renderer handle */
    DrawablePtr		pDrawable;/* pointer to drawable */
/* out */
{
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;
    ddPCAttr		*ppca;
    listofObj		*pMC, *pLS;

#ifdef DDTEST
    ErrorF( " init_pipeline\n");
#endif

    /*
     * check that drawable is OK for renderer this means it's ok for the
     * luts, ns, and GC since they are ok for the example drawable
     */

    /* empty current path */
    PU_EMPTY_LIST(pRend->curPath);

    /* 
     * Initialize ddcontext. 
     */
    ppca = pddc->Dynamic->pPCAttr;
    pMC = ppca->modelClipVolume;
    pLS = ppca->lightState;
    if (pRend->pPC != NULL) {
	*ppca = *pRend->pPC->pPCAttr;
	/* 
	 * don't forget the model clip half planes and list of
	 * light sources, which are only pointed to 
	 */
	if (puCopyList(pRend->pPC->pPCAttr->modelClipVolume, pMC))
		return(BadAlloc);
	if (puCopyList(pRend->pPC->pPCAttr->lightState, pLS))
		return(BadAlloc);
    } else {				  /* use default PC values */
	MI_GET_DEFAULT_PC(&defaultPCAttr);
	*ppca = defaultPCAttr;
	/* 
	 * don't forget the model clip half planes and list of
	 * light sources, which are only pointed to 
	 */
	if (puCopyList(defaultPCAttr.modelClipVolume, pMC))
		return(BadAlloc);
	if (puCopyList(defaultPCAttr.lightState, pLS))
		return(BadAlloc);
    }
    ppca->modelClipVolume = pMC;
    ppca->lightState = pLS;

    /* copy the current name set from the ns resource to the renderer */
    MINS_EMPTY_NAMESET(pddc->Dynamic->currentNames);
    if (ppca->pCurrentNS)
    {
	miNSHeader	*pns = (miNSHeader *)ppca->pCurrentNS->deviceData;

	MINS_COPY_NAMESET(pns->names, pddc->Dynamic->currentNames);
    }

    /* set the filter_flags in pddc for high, invis, pick, search */
    mi_set_filters(pRend, pddc);

    MI_DDC_SET_DO_PRIMS(pRend, pddc);

    /* this must be called before the rendering state is set 
     * and after the filters are set */

    MI_SET_ALL_CHANGES(pRend);
    ValidateRenderer(pRend);

    /* 
     * Compute composite 4x4s for use in the rendering pipeline 
     * Computed composites: 1. [GM][LM] = [CMM] or mc_to_wc_xform
     *		      2. [VO][VM] = [VOM] or wc_to_npc_xform
     *		      3. [CMM][VOM] = [VCM] or mc_to_npc_xform
     * Reminder: [GM] and  [LM] are in pipeline context, [VO]
     *	   and [VM] are in the View LUT indexed by the
     *	   view index set in the pipeline context.
     */

    /* Compute the composite [CMM] next */
    miMatMult (pddc->Dynamic->mc_to_wc_xform, 
	       pddc->Dynamic->pPCAttr->localMat,
	       pddc->Dynamic->pPCAttr->globalMat); 

    return (Success);
}

/*++
 |
 |  Function Name:	BeginRendering
 |
 |  Function Description:
 |	 Supports the PEXBeginRendering request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
BeginRendering(pRend, pDrawable)
/* in */
    ddRendererPtr	pRend;	  /* renderer handle */
    DrawablePtr		pDrawable;/* pointer to drawable */
/* out */
{
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;

#ifdef DDTEST
    ErrorF( " BeginRendering\n");
#endif

    pRend->render_mode = MI_REND_DRAWING;

    init_pipeline(pRend, pDrawable);

    /*
     * Determine the npc -> dc viewport transform
     */
     miBldViewport_xform(pRend, pDrawable, pddc->Static.misc.viewport_xform, pddc );

    /*
     * compute cc xform, concatenate to appropriate dd context matrices.
     */
     miBldCC_xform(pRend, pddc);

    /*
     * Clear the window if clearI flag is on.
     * Use the background color in the renderer attributes.
     * The default (0) entry in the Color Approx Table is used
     * to compute the pixel.
     */
    if (pRend->clearI) {

      unsigned long   colorindex, gcmask;
      GCPtr           pGC;
      extern GCPtr    CreateScratchGC();
      extern int      ChangeGC();
      extern void     ValidateGC();
      xRectangle      xrect;
      DrawablePtr     pDraw;
      xRectangle      *xrects, *p;
      ddDeviceRect    *ddrects;
      ddLONG          numrects;
      int             i;
      ddTableIndex    colourApproxIndex;

      pDraw = pRend->pDrawable;
      if ((!pRend->pPC) || (!pRend->pPC->pPCAttr)) colourApproxIndex = 0;
      else colourApproxIndex = pRend->pPC->pPCAttr->colourApproxIndex;
      miColourtoIndex(pRend, colourApproxIndex, 
			&pRend->backgroundColour, &colorindex);
      pGC = CreateScratchGC(pDraw->pScreen, pDraw->depth);
      gcmask = GCForeground;
      ChangeGC(pGC, gcmask, &colorindex);
      /* Set the Clip List if there is one */
      numrects = pRend->clipList->numObj;
      if (numrects) {
        ddrects = (ddDeviceRect *) pRend->clipList->pList;
        xrects = (xRectangle*) xalloc(numrects * sizeof(xRectangle));
        if (!xrects) return BadAlloc;
        /* Need to convert to XRectangle format and flip Y */
        for (i = 0, p = xrects; i < numrects; i++, p++, ddrects++) {
          p->x = ddrects->xmin;
          p->y = pDraw->height - ddrects->ymax;
          p->width = ddrects->xmax - ddrects->xmin + 1;
          p->height = ddrects->ymax - ddrects->ymin + 1;
        }
        SetClipRects(pGC, 0, 0, (int)numrects, xrects, Unsorted);
        xfree((char*)xrects);
      }
      ValidateGC(pDraw, pGC);
      /* Now draw a filled rectangle to clear the image buffer */
      xrect.x = 0;
      xrect.y = 0;
      xrect.width = pDraw->width;
      xrect.height = pDraw->height;
      (*pGC->ops->PolyFillRect) (pDraw, pGC, 1, &xrect);
      gcmask = GCClipMask;
      colorindex = 0;
      ChangeGC(pGC, gcmask, &colorindex);
      FreeScratchGC(pGC);
    }

    /* do double buffering stuff */
    /* do hlhsr stuff */

    pRend->state = PEXRendering;
    return (Success);
}

/*++
 |
 |  Function Name:	EndRendering
 |
 |  Function Description:
 |	 Supports the PEXEndRendering request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
EndRendering(pRend)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
{

#ifdef DDTEST
    ErrorF( " EndRendering\n");
#endif

    pRend->state = PEXIdle;
    /* switch display buffers if doing multi-buffers */

    return (Success);
}

/*++
 |
 |  Function Name:	BeginStructure
 |
 |  Function Description:
 |	 Supports the PEXBeginStructure request.
 |
 |  Note(s):
 |      This procedure creates a new ddcontext which looks like the
 |      old (current) context.  
 |
 |      Since some of these elements contain and/or are pointers to
 |      objects, the info cannot be copied directly, but new objects
 |      must be made to be pointed to and their contents copied.
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
BeginStructure(pRend, sId)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
    ddResourceId	sId;	  /* structure id */
/* out */
{
    ddpex3rtn		PushddContext();
    ddpex3rtn		err = Success;
    ddElementRef	newRef;
    ddpex3rtn		status;
    XID  		fakeStrID;
    diStructHandle    	fakeStr;
    ddPickPath          fakeStrpp, sIDpp, *curpp;


#ifdef DDTEST
    ErrorF( " BeginStructure %d\n", sId);
#endif

    /* if renderer idle ignore.... */
    if (pRend->state == PEXIdle)
      return Success;

    /* 
     * Push the current ddContext attributes onto the stack and create
     * a new instance of these attributes.
     */
    if (status = PushddContext(pRend)) return(status);

    /* update offset of existing path to count execute structure element */
    if (pRend->curPath->numObj)
     ((ddElementRef *)pRend->curPath->pList)[pRend->curPath->numObj-1].offset++;

    /** Add a new element to the cur_path for the new structure **/
    /* sid is really an id not a handle*/
    newRef.structure = (diStructHandle)sId;
    newRef.offset = 0;

    /* puAddToList returns 0 if it's successful, other there's an error */
    if (puAddToList((ddPointer)&newRef, (ddULONG)1, pRend->curPath))
    {
	return (BadAlloc);
    }

    /* when doing client side picking fake structures must be allocated 
       and the correspondence between their structure handles and IDs
       saved for later lookup by the appropriate EndPick routine
    */
    if ((pRend->state == PEXPicking) && (pRend->pickstr.server == DD_CLIENT)) {

	/* bump up the offset in the current element to simulate ExecStr */
        curpp = (ddPickPath *)(pRend->pickstr.fakeStrlist)->pList;
	curpp[(pRend->pickstr.fakeStrlist)->numObj-1].offset++;

	/* allocate a new fake structure and add to both lists */
	fakeStr = (diStructHandle)xalloc((unsigned long)
					      sizeof(ddStructResource));
	if (!fakeStr) return (BadAlloc);
	fakeStr->id = -666;
	err = CreateStructure(fakeStr);
	if (err != Success) {
	    xfree((pointer)(fakeStr));
	    return (err);
	}

	fakeStrpp.structure = fakeStr;
	fakeStrpp.offset = 0;
	fakeStrpp.pickid = 0;
	err = puAddToList((ddPointer) &fakeStrpp, (ddULONG) 1, pRend->pickstr.fakeStrlist);
	if (err != Success) {
	    xfree((pointer)(fakeStr));
	    return (err);
	}

	sIDpp.structure = fakeStr;
	sIDpp.offset = 0;
	/* Store the supplied structure ID here for retrieval at EndPick */
	sIDpp.pickid = sId;
	err = puAddToList((ddPointer) &sIDpp, (ddULONG) 1, pRend->pickstr.sIDlist);
	if (err != Success) {
	    xfree((pointer)(fakeStr));
	    return (err);
	}

    }
    return (Success);
}	/* BeginStructure */

/*++
 |
 |  Function Name:	EndStructure
 |
 |  Function Description:
 |	 Supports the PEXEndStructure request.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
EndStructure(pRend)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
{
    ddpex3rtn		PopddContext();
    ddpex3rtn		status;
    miStructPtr 	pheader; 
    diStructHandle      sh = 0;
    ddPickPath          *strpp;
    miDDContext    *pddc = (miDDContext *) pRend->pDDContext;




#ifdef DDTEST
    ErrorF( " EndStructure\n");
#endif

    /* if renderer idle ignore.... */
    if (pRend->state == PEXIdle)
      return Success;

    /* if there is no next then BeginStructure has not been
       called so simply ignore this EndStructure call.... 
    */
    if (pddc->Dynamic->next == NULL)
      return Success;

    /*
     * Pop ddContext off stack - retrieve attributes for current structure */
    if (status = PopddContext(pRend)) return (status);

    /* 
     * could put more intelligence here, 
     * but for now assume everything changes 
     */
    MI_SET_ALL_CHANGES(pRend);
    ValidateRenderer(pRend);

    /** Remove the last currentPath element from the renderer **/
    PU_REMOVE_LAST_OBJ(pRend->curPath);

    if ((pRend->state == PEXPicking) && (pRend->pickstr.server == DD_CLIENT)) {

	/* the fake structure can not be freed until End Picking
	   since otherwise that chunk of memory could be allocated
	   to another client provided sID thus destroying the 1 to 1
	   mapping that the sIDlist counts up, so just remove the info 
	   for the structure from the fakeStrlist. The handle and ID 
	   must stay on the sID list until after the reply is processed
	*/
	PU_REMOVE_LAST_OBJ(pRend->pickstr.fakeStrlist);
    }

    return (Success);

}	/* EndStructure */

static void
set_highlight_colours(pRend, pddc)
    ddRendererPtr    pRend;
	miDDContext    *pddc;
{
    pddc->Static.attrs->lineColour = pddc->Static.misc.highlight_colour;
    pddc->Static.attrs->edgeColour = pddc->Static.misc.highlight_colour;
    pddc->Static.attrs->markerColour = pddc->Static.misc.highlight_colour;
    pddc->Static.attrs->surfaceColour = pddc->Static.misc.highlight_colour;
    pddc->Static.attrs->textColour = pddc->Static.misc.highlight_colour;

    pddc->Static.misc.flags |= POLYLINEGCFLAG;
    pddc->Static.misc.flags |= EDGEGCFLAG;
    pddc->Static.misc.flags |= MARKERGCFLAG;
    pddc->Static.misc.flags |= FILLAREAGCFLAG;
    pddc->Static.misc.flags |= TEXTGCFLAG;
    return;
}

static void
unset_highlight_colours(pRend, pddc)
    ddRendererPtr    pRend;
    miDDContext    *pddc;
{
    ddBitmask    tables, namesets, attrs;

    /* not too efficient: ValidateDDContextAttrs does more than
     * just colours
     */
    tables  = PEXDynMarkerBundle | PEXDynTextBundle | PEXDynLineBundle
	    | PEXDynInteriorBundle | PEXDynEdgeBundle;
    namesets = 0;
    attrs = 0;
    ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
    return;
}

/*++
 |
 |  Function Name:	ValidateFilters
 |
 |  Function Description:
 |	 updates filters flags 
 |
 |  Note(s):
 |
 --*/

void
ValidateFilters(pRend, pddc, namesets)
ddRendererPtr     pRend;        /* renderer handle */
miDDContext      *pddc;         /* ddPEX attribute structure */
ddBitmask         namesets;
{
    ddUSHORT      save_flags;

    if ((namesets & PEXDynHighlightNameset) ||
        (namesets & PEXDynInvisibilityNameset) ||
        (namesets & PEXDynHighlightNamesetContents) ||
        (namesets & PEXDynInvisibilityNamesetContents)) {

	    save_flags = pddc->Dynamic->filter_flags;

	    mi_set_filters(pRend, pddc, namesets);

	    if ( (MI_DDC_IS_HIGHLIGHT(pddc)) &&
		 !(save_flags & MI_DDC_HIGHLIGHT_FLAG) )
		/* just turned on highlighting */
		set_highlight_colours(pRend, pddc);
	    else if ( (!(MI_DDC_IS_HIGHLIGHT(pddc))) &&
		      (save_flags & MI_DDC_HIGHLIGHT_FLAG) )
		/* just turned off highlighting */
		unset_highlight_colours(pRend, pddc);

	    MI_DDC_SET_DO_PRIMS(pRend, pddc);
    }
}

/*++
 |
 |  Function Name:	ValidateRenderer
 |
 |  Function Description:
 |	 loads executeOC table in renderer correctly and calls
 |	 to validate the ddcontext
 |
 |  Note(s):
 |
 --*/

ValidateRenderer(pRend)
    ddRendererPtr	pRend;	  /* renderer handle */
{
    ddpex3rtn		ValidateDDContextAttrs();

    miDDContext		*pddc = (miDDContext *)pRend->pDDContext;
    ddBitmask		tables, namesets, attrs;
    extern void inq_last_colour_entry();

    /* load in different executeOCs if needed
     * can do this here or in ValidateDDContextAttrs as needed 
     * eg, if there are multiple procs in a set for an oc (different
     * ones for hollow fill areas and solid ones), then load 
     * them here or in ValidateDDContextAttrs when the attribute
     * controlling it changes
     */

    /* set highlight colour if necessary  */
    if (pRend->tablesChanges & (PEXDynColourTable | PEXDynColourTableContents))
	inq_last_colour_entry(pRend->lut[PEXColourLUT], &pddc->Static.misc.highlight_colour);

    /* validate the attributes */
    if (pRend->state == PEXRendering)
    {
	/* validate only dynamic attrs */
	tables = pRend->tablesChanges & ~pRend->tablesMask;
	namesets = pRend->namesetsChanges & ~pRend->namesetsMask;
	attrs = pRend->attrsChanges & ~pRend->attrsMask;
	ValidateFilters(pRend, pddc, namesets);
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
	pRend->tablesChanges &= (~tables);
	pRend->namesetsChanges &= (~namesets);
	pRend->attrsChanges &= (~attrs);
    } else
    {
	/* validate all attrs */
	tables = pRend->tablesChanges;
	namesets = pRend->namesetsChanges;
	attrs = pRend->attrsChanges;
	ValidateDDContextAttrs(pRend, pddc, tables, namesets, attrs);
	ValidateFilters(pRend, pddc, namesets);
	/* reset change masks */
	MI_ZERO_ALL_CHANGES(pRend);
    }
}

/*++
 |
 |  Function Name:	BeginPicking
 |
 |  Function Description:
 |	 Sets up the pipeline to do Picking.
 |
 |  Note(s): This is a copy of BeginRendering with extraneous rendering
 |	   stuff removed. Wherever, the code has been removed, comments
 |	   have been placed to identify the removals.
 |
 --*/

ddpex3rtn
BeginPicking(pRend, pPM)
/* in */
    ddRendererPtr	pRend;	  /* renderer handle */
    diPMHandle		pPM;	/* pick measure */
/* out */
{
    miPickMeasureStr *ppm = (miPickMeasureStr *) pPM->deviceData;
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;
    DrawablePtr		pDrawable = pRend->pDrawable;
    ddPCAttr		*ppca;
    ddFLOAT             inv_xform[4][4];

#ifdef DDTEST
    ErrorF( " BeginPicking\n");
#endif

    /* set device info needed for picking */
    pddc->Static.pick.type = ppm->type; 
    pddc->Static.pick.status = ppm->status; 
    switch (ppm->type)
    {
	case PEXPickDeviceDC_HitBox:
		pddc->Static.pick.data_rec.dc_data_rec = 
			ppm->data_rec.dc_data_rec; 
		pddc->Static.pick.input_rec.dc_hit_box = 
			ppm->input_rec.dc_hit_box; 
		break;

	case PEXPickDeviceNPC_HitVolume:
		pddc->Static.pick.data_rec.npc_data_rec = 
			ppm->data_rec.npc_data_rec; 
		pddc->Static.pick.input_rec.npc_hit_volume = 
			ppm->input_rec.npc_hit_volume; 
		break;
    }

    MINS_EMPTY_NAMESET(pddc->Static.pick.inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.pick.exclusion);
    if (ppm->incl_handle) {
	miNSHeader	*pns = (miNSHeader *)ppm->incl_handle->deviceData;
	MINS_COPY_NAMESET(pns->names, pddc->Static.pick.inclusion);
    }
    if (ppm->excl_handle) {
	miNSHeader	*pns = (miNSHeader *)ppm->excl_handle->deviceData;
	MINS_COPY_NAMESET(pns->names, pddc->Static.pick.exclusion);
    }

/* load picking procs into executeOCs */

    memcpy( (char *)pRend->executeOCs, 
	(char *)PickExecuteOCTable, 
	sizeof(ocTableType)*OCTABLE_LENGTH); 

    pRend->render_mode = MI_REND_PICKING;

    /* make sure this gets initialized for every pick */
    pRend->pickstr.more_hits = PEXNoMoreHits;


    /*
     * Reinitialize level 1 procedure jump table for PICKING !
     */
    memcpy( (char *)pddc->Static.RenderProcs, 
	  (char *)PickPrimitiveTable, 
	  sizeof(RendTableType) * RENDER_TABLE_LENGTH); 

    init_pipeline(pRend, pDrawable);

    /*
     * Determine the npc -> dc viewport transform
     */
    miBldViewport_xform( pRend, pDrawable, pddc->Static.misc.viewport_xform, pddc );

    /* Compute the inverse of the viewport transform to be used to */
    /* convert DC_HitBoxes to NPC_HitVolumes.                      */

    memcpy( (char *)inv_xform, 
	   (char *)pddc->Static.misc.viewport_xform, 16*sizeof(ddFLOAT));
    miMatInverse (inv_xform);
    memcpy( (char *)pddc->Static.misc.inv_vpt_xform, 
	    (char *) inv_xform, 16*sizeof(ddFLOAT));

    /* Now, clear out the viewport transform computed above to identity */
    /* since we are PICKING, and we do not need this to go to final ddx */
    /* space. THIS IS IMPORTANT SINCE WE WILL BE USING THE LEVEL 2 REND-*/
    /* ERING ROUTINES TO DO TRANSFORMATIONS AND CLIPPING. ONLY LEVEL 1  */
    /* PICKING ROUTINES WILL ACTUALLY DO THE PICK HIT TEST IN CC. THUS  */
    /* BY MAKING THE viewport transform IDENTITY WE WILL STAY IN CC.    */

    memcpy( (char *) pddc->Static.misc.viewport_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    /* Clear out the cc_to_dc_xform also, since we will not be going to DC */

    memcpy( (char *) pddc->Dynamic->cc_to_dc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG |
				INVTRMCTOCCXFRMFLAG | INVTRCCTODCXFRMFLAG |
				INVVIEWXFRMFLAG);

    /* Mark as invalid any transform dependant fields in ddContext */
    pddc->Static.misc.flags |= (MCVOLUMEFLAG | CC_DCUEVERSION);

    /* 
     * Computation of the composite mc -> dc transform has been REMOVED.
     */

    /* do double buffering stuff */
    /* do hlhsr stuff */

    pRend->state = PEXPicking;

    return (Success);
}

/*++
 |
 |  Function Name:	EndPicking
 |
 |  Function Description:
 |	 Handles the stuff to be done after a Pick traversal.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
EndPicking(pRend)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
{
/* Locals */
      miDDContext      *pddc = (miDDContext *)(pRend->pDDContext);
      ddPickPath        *strpp;
      diStructHandle    sh = 0;
      int		i;


#ifdef DDTEST
    ErrorF( " EndPicking\n");
#endif

    if (pRend->immediateMode == TRUE) {
	/*  empty listoflist for Pick All
	   this assumes the individual pick paths lists that this
	   pointed to have already been deleted by the EndPickAll routine 
	*/
	PU_EMPTY_LIST(pRend->pickstr.list);

	/* free all but the first the fake structure
	   it should always be there to support ROCs  
	*/
	strpp = (ddPickPath *)(pRend->pickstr.sIDlist)->pList;
	for (i = 1; i < (pRend->pickstr.sIDlist)->numObj; i++) {
	    sh = strpp[i].structure;
	    DeleteStructure(sh, sh->id);    
	}

	(pRend->pickstr.sIDlist)->numObj = 1;
    }

    pRend->state = PEXIdle;

    pRend->render_mode = MI_REND_DRAWING;

    /* copy the initial oc functions to the OC table */

    memcpy( (char *)pRend->executeOCs, 
	    (char *)InitExecuteOCTable, 
	    sizeof(ocTableType)*OCTABLE_LENGTH); 

    /*
     * Reinitialize level 1 procedure jump table for Rendering !
     */
    memcpy( (char *)pddc->Static.RenderProcs, 
	  (char *)RenderPrimitiveTable, 
	  sizeof(RendTableType) * RENDER_TABLE_LENGTH); 

    return (Success);
}

/*++
 |
 |  Function Name:	InquirePickStatus
 |
 |  Function Description:
 |	 returns current pick status
 |
 |  Note(s):

 |
 --*/

/* 
#define PEX_SI_FAKE_PICK 
*/
#ifdef PEX_SI_FAKE_PICK
/*dummy proc to use to fake hit:
  hit occurs when traversal depth = 3, structure offset = 3,
  current pick id = 4
  need p_trav_state for this, but p_trav_state will not be a 
  parameter when real picking is done
*/
InquirePickStatus(pRend, pStatus, p_trav_state)
	ddRendererPtr	pRend;
	ddUSHORT	*pStatus;	
	miTraverserState	*p_trav_state;	
{
	if ( (p_trav_state->max_depth == 3) &&
		(((ddElementRef *)pRend->curPath-> pList)[pRend->curPath->numObj - 1].offset == 3) &&
		( ((miDDContext *)pRend->pDDContext)->Dynamic->pPCAttr->pickId == 4) )
	  *pStatus = PEXOk;
	else
	  *pStatus = PEXNoPick;
	return;
}
#else

void
InquirePickStatus(pRend, pStatus, p_trav_state)
	ddRendererPtr	pRend;
	ddUSHORT	*pStatus;	
{
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;

	*pStatus = pddc->Static.pick.status;
	pddc->Static.pick.status = PEXNoPick;
	return;
}
#endif

/*++
 |
 |  Function Name:	BeginSearching
 |
 |  Function Description:
 |	 Sets up the pipeline to do spatial search
 |
 |  Note(s): This is a copy of BeginRendering with extraneous rendering
 |	   stuff removed. Wherever, the code has been removed, comments
 |	   have been placed to identify the removals.
 |
 --*/

ddpex3rtn
BeginSearching(pRend, pSC)
/* in */
    ddRendererPtr	pRend;	  /* renderer handle */
    ddSCStr		*pSC;	/* search context */
/* out */
{
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;
    DrawablePtr		pDrawable = pRend->pDrawable;
    ddNSPair		*pPairs;
    miNSHeader		*pns; 
    register int	i;

#ifdef DDTEST
    ErrorF( " BeginSearching\n");
#endif

    /* set device info needed for searching */
    pddc->Static.search.status = PEXNotFound;	
    pddc->Static.search.position = pSC->position; 
    pddc->Static.search.distance = pSC->distance; 
    pddc->Static.search.modelClipFlag = pSC->modelClipFlag; 

    MINS_EMPTY_NAMESET(pddc->Static.search.norm_inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.norm_exclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.invert_inclusion);
    MINS_EMPTY_NAMESET(pddc->Static.search.invert_exclusion);

    if (pSC->normal.numPairs) {
	pPairs = pSC->normal.pPairs;
	for (i=0; i<pSC->normal.numPairs; i++, pPairs++ ) {

	    if (pPairs->incl) {
	        pns = (miNSHeader *)pPairs->incl->deviceData;
	        MINS_OR_NAMESETS(pns->names, pddc->Static.search.norm_inclusion);
	    }
	    if (pPairs->excl) {
	        pns = (miNSHeader *)pPairs->excl->deviceData;
	        MINS_OR_NAMESETS(pns->names, pddc->Static.search.norm_exclusion);
	    }
	}
    }
    if (pSC->inverted.numPairs) {
	pPairs = pSC->inverted.pPairs;
	for (i=0; i<pSC->inverted.numPairs; i++, pPairs++ ) {

	    if (pPairs->incl) {
	        pns = (miNSHeader *)pPairs->incl->deviceData;
	        MINS_OR_NAMESETS(pns->names, pddc->Static.search.invert_inclusion);
	    }
	    if (pPairs->excl) {
	        pns = (miNSHeader *)pPairs->excl->deviceData;
	        MINS_OR_NAMESETS(pns->names, pddc->Static.search.invert_exclusion);
	    }
	}
    }

 /* load searching procs into executeOCs  */

    memcpy( (char *)pRend->executeOCs, 
	(char *)SearchExecuteOCTable, 
	sizeof(ocTableType)*OCTABLE_LENGTH); 

    /*
     * Reinitialize level 1 procedure jump table for Searching.
     * Note that we use the same table as Picking.
     */
    memcpy( (char *)pddc->Static.RenderProcs,
	  (char *)PickPrimitiveTable,
          sizeof(RendTableType) * RENDER_TABLE_LENGTH);
 
    /* Set the model clipping flag to value in search context
       resource */
    pddc->Dynamic->pPCAttr->modelClip = pSC->modelClipFlag;
 
    pRend->render_mode = MI_REND_SEARCHING;

    init_pipeline(pRend, pDrawable);

    /*
     * Since searching is done in world coordinate space, we need NOT
     * compute any of the rest of the matrices. They must all be set
     * to identity.
     */

    memcpy( (char *) pddc->Static.misc.viewport_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->wc_to_npc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->mc_to_npc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->wc_to_cc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->cc_to_dc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->mc_to_cc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->mc_to_dc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    memcpy( (char *) pddc->Dynamic->npc_to_cc_xform, 
	  (char *) ident4x4, 16 * sizeof(ddFLOAT));

    /* Mark as invalid appropriate inverse transforms in dd context */
    pddc->Static.misc.flags |= (INVTRMCTOWCXFRMFLAG | INVTRWCTOCCXFRMFLAG |
				INVTRMCTOCCXFRMFLAG | INVTRCCTODCXFRMFLAG |
				INVVIEWXFRMFLAG);

    /* Mark as invalid any transform dependant fields in ddContext */
    pddc->Static.misc.flags |= (MCVOLUMEFLAG | CC_DCUEVERSION);

    /* 
     * Computation of the composite mc -> dc transform has been REMOVED.
     */

    /* do double buffering stuff */
    /* do hlhsr stuff */

    pRend->state = PEXRendering;

    return (Success);
}

/*++
 |
 |  Function Name:	EndSearching
 |
 |  Function Description:
 |	 Handles the stuff to be done after a search traversal.
 |
 |  Note(s):
 |
 --*/

ddpex3rtn
EndSearching(pRend)
/* in */
    ddRendererPtr       pRend;	  /* renderer handle */
/* out */
{

#ifdef DDTEST
    ErrorF( " EndSearching\n");
#endif

    pRend->state = PEXIdle;

    pRend->render_mode = MI_REND_DRAWING;
    /* copy the initial oc functions to the OC table */
    memcpy( (char *)pRend->executeOCs, 
	(char *)InitExecuteOCTable, 
	sizeof(ocTableType)*OCTABLE_LENGTH); 

    return (Success);
}

/*++
 |
 |  Function Name:	InquireSearchStatus
 |
 |  Function Description:
 |	 returns current spatial search status
 |
 |  Note(s):
 |
 --*/

void
InquireSearchStatus(pRend, pStatus)
	ddRendererPtr	pRend;
	ddUSHORT	*pStatus;	/* PEXFound or PEXNotFound */
{
    miDDContext		*pddc = (miDDContext *) pRend->pDDContext;

	*pStatus = pddc->Static.search.status;
	return;
}


