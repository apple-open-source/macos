/* $Xorg: miTraverse.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/miTraverse.c,v 1.10 2001/12/14 19:57:36 dawes Exp $ */

#include "miWks.h"
#include "PEXproto.h"
#include "PEXprotost.h"
#include "miRender.h"
#include "miPick.h"
#include "miStruct.h"
#include "miStrMacro.h"
#include "pexos.h"


extern void     InquirePickStatus();
extern void     InquireSearchStatus();

extern ddpex3rtn BeginStructure();
extern ddpex3rtn EndStructure();
extern ocTableType ExecuteOCTable[];

ddpex4rtn traverser();

#ifdef DDTEST
#define ASSURE(test) \
    if (!(test)) {  \
	ErrorF( "test \n"); \
	ErrorF( "Failed: Line %d, File %s\n\n", __LINE__, __FILE__); \
    }
#else
#define ASSURE(test)
#endif				/* DDTEST */

static          ddBOOL
pickES (pRend, p_trav_state, p_str, depth, curr_offset)
    ddRendererPtr	    pRend;
    miTraverserState    *p_trav_state;
    diStructHandle	p_str;	        /* current structure */
    ddSHORT		depth;		/* how far down in structures */
    ddULONG		curr_offset;
{

    if ((p_str->id == p_trav_state->p_curr_pick_el->structure->id) &&
	    (curr_offset == p_trav_state->p_curr_pick_el->offset)) {

	  if (depth < pRend->pickStartPath->numObj) 
	      /* continue following start path */
	      p_trav_state->p_curr_pick_el++;
	  else 
	      /* at end of start path; time to start traversal */
	      p_trav_state->exec_str_flag = ES_YES;

	  return(MI_TRUE);
    }
    return (MI_FALSE);
}

static          ddBOOL
searchES(pSC, p_trav_state, p_str, depth, curr_offset)
    ddSCStr		    *pSC;
    miTraverserState    *p_trav_state;
    diStructHandle	p_str;	        /* current structure */
    ddSHORT		depth;		/* how far down in structures */
    ddULONG		curr_offset;
{
    if ((p_str->id == p_trav_state->p_curr_sc_el->structure->id) &&
	    (curr_offset == p_trav_state->p_curr_sc_el->offset)) {
	/* OK, this element is in the start path. Now if we're
	 * at the ceiling, don't follow the exec-str
	 */
	if ((pSC->ceiling == 1) || (depth < pSC->ceiling)) {
	    if (depth < pSC->startPath->numObj) 
	        /* continue following start path */
                p_trav_state->p_curr_sc_el++;
	    else 
	        /* at end of start path; time to start traversal */
	        p_trav_state->exec_str_flag = ES_YES;
	    return(MI_TRUE);
	}
    }
    return (MI_FALSE);
}

int
miTraverse(pWks )
    diWKSHandle     pWks;
{
    register ddOrdStruct    *pos;
    miWksPtr		    pwks = (miWksPtr) pWks->deviceData;
    ddpex4rtn		    err = Success;
    miTraverserState	    trav_state;
    DrawablePtr             pRealDrawable;


    if (    (pwks->pRend->pDrawable == NULL)
	 || (pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

    if (!pwks->postedStructs.numStructs || !pwks->pCurDrawable) return (Success);

    /* set exec_str_flag */
    trav_state.exec_str_flag = ES_YES;
    trav_state.p_curr_pick_el = (ddPickPath *) NULL;
    trav_state.p_curr_sc_el = (ddElementRef *) NULL;

    /* save drawable to be restored later */
    pRealDrawable = pwks->pRend->pDrawable;
    pwks->pRend->pDrawable = pwks->pCurDrawable;

    /**  call into ddPEX level III Begin Rendering   **/
    BeginRendering(pwks->pRend, pwks->pCurDrawable);

    /* traverse all posted structs */
    pos = pwks->postedStructs.postruct;
    while ((pos->next) && (err == Success)) {
	pos = pos->next;
	/* reset for each structure */
	trav_state.max_depth = 0;	
	trav_state.pickId = 0;	
        trav_state.ROCoffset =  0;


	if (MISTR_NUM_EL((miStructPtr) pos->pstruct->deviceData)) {
	    BeginStructure(pwks->pRend, pos->pstruct->id);

	    /*
	     * always start at the first element in the
	     * structure
	     */
	    err = traverser(	pwks->pRend, pos->pstruct, (ddULONG) 1,
				MISTR_NUM_EL((miStructPtr) pos->pstruct->deviceData),
				(diPMHandle)NULL, (ddSCStr *) NULL, &trav_state);

	    EndStructure(pwks->pRend);
	    pwks->displaySurface = PEXNotEmpty;
	}
    }

    EndRendering(pwks->pRend);

    pwks->pRend->pDrawable = pRealDrawable;

    if (err != Success) {
	/* do stuff here to return error */
	return(err);
    }

    return (err);
}				/* miTraverse */

/* this traverses server side structures */
/* startel  must be > 0
 * stopel must be <= num els in structure
 */
/*
 * begin/end structure keep track of the current path, 
 * but calling the level II OCs directly does not
 * so do this BEFORE calling the OCs
 */
#define	INCREMENT_OFFSET(pRend)			\
    if (pRend->curPath->numObj) 		\
       ((ddElementRef *)pRend->curPath->pList)[pRend->curPath->numObj-1].offset++

ddpex4rtn
traverser(pRend, pStruct, startel, stopel, pPM, pSC, p_trav_state)
ddRendererPtr	    pRend;
diStructHandle	    pStruct;
ddULONG		    startel;
ddULONG		    stopel;
diPMHandle	    pPM;
ddSCStr		    *pSC;
miTraverserState    *p_trav_state;
{
    register ddULONG	currOffset;
    miPickMeasureStr	*ppm;
    miGenericElementPtr	p_element;
    ddPointer		ddElement;	/* imp. dep. parsed element */
    diStructHandle	p_next_str;	/* execute structure structure */
    ddSHORT		depth;		/* how far down in structures */
    ddULONG		pickId, ROCoffset;
    miStructPtr		pstruct = (miStructPtr)(pStruct->deviceData);
    ddUSHORT		pickStatus, searchStatus;
    ddpex2rtn		err;
    ddPickPath		*pl;
    ddElementRef	*sl;
    int			zap;
    miPPLevel		myPickLevel, *pp;
    int			i;


    if (pPM)
	ppm = (miPickMeasureStr *) pPM->deviceData;
    else
	ppm = (miPickMeasureStr *) NULL;

    /*
     * set depth=MaxDepth here when called on way in, 
     * so depth is current depth on way out
     */
    p_trav_state->max_depth++;
    depth = p_trav_state->max_depth;
    /* same for pick id */
    pickId = p_trav_state->pickId;
    MISTR_FIND_EL(pstruct, startel, p_element);
    currOffset = startel;

    /* when calling traverser from ROC ROCoffset may be non-zero
       to account for prior ROCs, all other times this should
       be 0 and we set it to 0 here so recursion doesn't mess
       up the offsets when processing execute structure
    */
    ROCoffset = p_trav_state->ROCoffset;
    p_trav_state->ROCoffset = 0;

    /* do stuff for following search start path */
    if (pSC) {
        /* if following start path, and its at the last element_ref
         * in the start path, and its after the last element in
         * the element_ref, then its at the end of the start
         * path and searching should begin
         */
        if ( (p_trav_state->exec_str_flag == ES_FOLLOW_SEARCH) &&
             (depth == pSC->startPath->numObj) &&
    	 (currOffset > p_trav_state->p_curr_sc_el->offset) )
    	p_trav_state->exec_str_flag = ES_YES;
    }

    /* do stuff for following pick start path */
    if (pPM) {
        if ( (p_trav_state->exec_str_flag == ES_FOLLOW_PICK) &&
             (depth == pRend->pickStartPath->numObj) &&
	     (currOffset > p_trav_state->p_curr_pick_el->offset) )
	  p_trav_state->exec_str_flag = ES_YES;
    }

    while (currOffset <= stopel) {
	ddElement = (ddPointer) (&(p_element->element));
	
	switch (MISTR_EL_TYPE(p_element)) {
	    case PEXOCExecuteStructure: {

		/*
		 * While inside this traverser, don't call level II execute 
		 * structure OC.  It is used in mixed mode traversals to get
		 * from client-side traversing to server-side traverser
		 */
		ddBOOL          go;

		p_next_str = ((diStructHandle) MISTR_GET_EXSTR_STR(p_element));
		if (p_trav_state->exec_str_flag == ES_FOLLOW_PICK)
		    go = pickES (pRend, p_trav_state, pStruct, depth, currOffset);
		else if (p_trav_state->exec_str_flag == ES_FOLLOW_SEARCH)
		    go = searchES(pSC, p_trav_state, pStruct, depth, currOffset);
		else if (p_trav_state->exec_str_flag == ES_YES)
		    go = MI_TRUE;
		else
		    go = MI_FALSE;

		if (go) {

		    BeginStructure(pRend, p_next_str->id);

		    /* build the pick path as we descend the hierarchy */
		    if (pPM) {
			myPickLevel.up = p_trav_state->p_pick_path;
			myPickLevel.pp.structure = pStruct;
			myPickLevel.pp.offset = currOffset + ROCoffset;
			myPickLevel.pp.pickid = pickId;
			p_trav_state->p_pick_path = &myPickLevel;
		    }

		    err = traverser(	pRend, p_next_str, (ddULONG) 1,
					MISTR_NUM_EL((miStructPtr) p_next_str->deviceData),
					pPM, pSC, p_trav_state);
		    if (err != Success)	return (err);
		    EndStructure(pRend);
		}

		/*
		 * We built the candidate pick path when it was found. We
		 * do nothing on the way out - except restore pointer.
	       */
	       if (pPM) {
		   p_trav_state->p_pick_path = myPickLevel.up; 
	       }


		/* do the same for searching, replacing the start
		 * path with the found path 
		 */
		if (pSC && (pSC->status == PEXFound)) {

		    sl = (ddElementRef *) pSC->startPath->pList;

		    zap = depth -1;

		    sl[zap].structure = pStruct;
		    sl[zap].offset = currOffset;

		    return (Success);
		} else 
		    /* popping out of Not Found */
		    if (pSC && (p_trav_state->exec_str_flag == ES_POP))
			return (Success);

		/*
		 * still picking or searching, so keep
		 * adjusting max_depth
		 */
		if (go)
		    p_trav_state->max_depth--;
		break;
	    }

	    case PEXOCPickId:

		/*
		 * For now, set own pick id and call into level II pick OC.
		 * Could do this in a level 4 pick procedure like execute struct
		 */
		pickId = p_trav_state->pickId = MISTR_GET_LABEL(p_element);

		INCREMENT_OFFSET(pRend);

		pRend->executeOCs[(int) (p_element->element.elementType)]
				(pRend, ddElement);

		break;

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
		INCREMENT_OFFSET(pRend);

		/* if following pick or search path, don't call prims */
		if ( (p_trav_state->exec_str_flag == ES_FOLLOW_PICK) ||
		     (p_trav_state->exec_str_flag == ES_FOLLOW_SEARCH) )
		    break;

		if (MI_DDC_DO_PRIMS(pRend)) {

		    pRend->executeOCs[(int) (p_element->element.elementType)]
					(pRend, ddElement);
		    if (pSC) {
			InquireSearchStatus(pRend, &searchStatus);
			/* searchStatus is PEXFound or PEXNotFound */
			pSC->status = searchStatus;
			if (searchStatus == PEXFound) {
		    	    sl = (ddElementRef *) pSC->startPath->pList;

			    pSC->startPath->numObj = p_trav_state->max_depth;

			    if (pSC->startPath->maxObj < p_trav_state->max_depth) {
				pSC->startPath->pList =
				    (ddPointer) xrealloc(
					     pSC->startPath->pList,
					     p_trav_state->max_depth
					     * sizeof(ddElementRef));
				pSC->startPath->maxObj = p_trav_state->max_depth;
			    }
		            zap = depth -1;

		            sl[zap].structure = pStruct;
		            sl[zap].offset = currOffset;

			    p_trav_state->exec_str_flag = ES_POP;
			    return (Success);
			}
		    }
		    if (pPM) {
			InquirePickStatus(pRend, &pickStatus, p_trav_state);
			if (pickStatus == PEXOk) {
                          if (pRend->pickstr.state == DD_PICK_ALL) {
			      myPickLevel.up = p_trav_state->p_pick_path;
			      myPickLevel.pp.structure = pStruct;
			      myPickLevel.pp.offset = currOffset + ROCoffset;
			      myPickLevel.pp.pickid = pickId;

			      AddPickPathToList( pRend, depth, &myPickLevel);
			      ppm->status = pickStatus;

                          } else {


			    pl = (ddPickPath *) ppm->path->pList;

			    /*
			     * then do stuff to update
			     * pick measure path and stop
			     * traversing
			     */
			    ppm->path->numObj = p_trav_state->max_depth;

			    if (ppm->path->maxObj < p_trav_state->max_depth) {
				ppm->path->pList =
				    (ddPointer) xrealloc(ppm->path->pList,
						     p_trav_state->max_depth
						     * sizeof(ddPickPath));
				ppm->path->maxObj = p_trav_state->max_depth;
			    }

			    /*
			     * oh boy, this is where recursion is fun.
			     * In this current iteration of traverser we know 
			     * the last element in the path, so stuff this 
			     * element into the bottom/top of the pick path list
			     * depending on whether the top/bottom part was
			     * requested
			     */
			    if (ppm->pathOrder == PEXTopFirst)
				zap = depth - 1;
			    else	
				zap = p_trav_state->max_depth - depth;

			    pl[zap].structure = pStruct;
			    pl[zap].offset = currOffset + ROCoffset;
			    pl[zap].pickid = pickId;

			    /*
			     * we want to continue on, so we do not want to
			     * pop off to use the recursion.
			     * follow the linked list down
			     */
			    pp = p_trav_state->p_pick_path;
			    for (i = depth-1; i > 0; i-- ) {		
			       if (ppm->pathOrder == PEXTopFirst)
				   zap = i - 1;
			       else	
				   zap = p_trav_state->max_depth - i;
			      
			       pl[zap] = pp->pp; /* structure assignment */
			       pp = pp->up;
			    }
			    ppm->status = pickStatus;

			  }
			}
		    }
		}
		break;

	    default:	/* all others */

		INCREMENT_OFFSET(pRend);

		/* could maybe skip some if doing pick or search */
		if (MI_IS_PEX_OC(MISTR_EL_TYPE(p_element)))
		    pRend->executeOCs[(int) (p_element->element.elementType)]
					(pRend, ddElement);

		    break;	/* default */

	    }		/* end switch */


	/* do stuff for following search start path */
	if ( pSC ) {
	    /* now, if its at the ceiling, forget it */
	    if ( (depth == pSC->ceiling) && (pSC->ceiling != 1) &&
		 (currOffset >=  stopel) ) {
		pSC->status = PEXNotFound;
		p_trav_state->exec_str_flag = ES_POP;
		return (Success);
	    }

            /* do stuff for following search start path */
            if (pSC) {
                /* if following start path, and its at the last element_ref
                 * in the start path, and its after the last element in
                 * the element_ref, then its at the end of the start
                 * path and searching should begin
                 */
                if ( (p_trav_state->exec_str_flag == ES_FOLLOW_SEARCH) &&
                     (depth == pSC->startPath->numObj) &&
            	 (currOffset >= p_trav_state->p_curr_sc_el->offset) )
            	p_trav_state->exec_str_flag = ES_YES;
            }
        
	}

	/* do stuff for following search start path */
	if (pPM) {
	    /* if following start path, and its at the last pick_path 
	     * in the start path, and its after the last element in
	     * the pick_path, then its at the end of the start
	     * path and searching should begin
	     */
	    if ( (p_trav_state->exec_str_flag == ES_FOLLOW_PICK) &&
		 (depth == pRend->pickStartPath->numObj) &&
		 (currOffset >= p_trav_state->p_curr_pick_el->offset) )
	      p_trav_state->exec_str_flag = ES_YES;
	}

	/* go on to the next element */
	currOffset++;
	p_element = MISTR_NEXT_EL(p_element);
    }			/* while loop (while there are still elements
					in the structure) */

    return (Success);

}				/* traverser */


/* for mixed mode traversals, this supports the execute structure OC */
void
execute_structure_OC(pRend, pOC)
ddRendererPtr		pRend;
pexExecuteStructure	*pOC;
{
    diStructHandle	pstruct = *((diStructHandle *)&(pOC->id));
    miTraverserState	trav_state;
    ddpex4rtn		err = Success;
    if (MISTR_NUM_EL((miStructPtr) pstruct->deviceData)) {

	BeginStructure(pRend, pstruct->id);
	trav_state.exec_str_flag = ES_YES;
	trav_state.p_curr_pick_el = NULL;
	trav_state.p_curr_sc_el = NULL;
	trav_state.max_depth = 0;
        trav_state.pickId = 0;
        trav_state.ROCoffset =  0;

	err = traverser(    pRend, pstruct, (ddULONG)0, 
			    MISTR_NUM_EL((miStructPtr)pstruct->deviceData), 
			    (diPMHandle)NULL, (ddSCStr *)NULL, &trav_state);
	EndStructure(pRend);

    }
    return;
}
