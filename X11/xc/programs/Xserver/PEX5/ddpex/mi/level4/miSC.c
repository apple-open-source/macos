/* $Xorg: miSC.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/miSC.c,v 1.10 2001/12/14 19:57:35 dawes Exp $ */


#include "ddpex4.h"
#include "mipex.h"
#include "miStruct.h"
#include "miStrMacro.h"
#include "pexUtils.h"
#include "pexos.h"


/*  Level 4 Workstation Support */
/*  Search Context procedures  */

/*++
 |
 |  Function Name:	SearchNetwork
 |
 |  Function Description:
 |	 Handles the PEXSearchNetwork request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SearchNetwork(pSC, pNumRefs, pBuffer)
/* in */
    ddSCStr        *pSC;	/* search context */
/* out */
    ddULONG        *pNumRefs;	/* number of references returned in list */
    ddBufferPtr     pBuffer;/* list of element references */
{
    miTraverserState	trav_state;
    ddpex4rtn		err = Success;
    diStructHandle  pstr;
    register int i;
    register pexElementRef	*pb;
    register ddElementRef 	*pr;

#ifdef DDTEST
    ErrorF(" SearchNetwork\n");
#endif

    pSC->status = PEXNotFound;
    *pNumRefs = 0;

    if (pSC->startPath->numObj) {
	trav_state.exec_str_flag = ES_FOLLOW_SEARCH;
	trav_state.p_curr_pick_el = (ddPickPath *) NULL;
	trav_state.p_curr_sc_el = (ddElementRef *) pSC->startPath->pList;
	trav_state.max_depth = 0;
        trav_state.pickId = 0;
        trav_state.ROCoffset =  0;
    } else
	return (PEXERR(PEXPathError));

    pstr = (diStructHandle) trav_state.p_curr_sc_el->structure;

    if (MISTR_NUM_EL((miStructPtr) pstr->deviceData)) {
	ddRendererStr   rend;

	/* init dummy renderer */
	rend.rendId = PEXAlreadyFreed;
	rend.pPC = (ddPCPtr)NULL;
	/* rend.drawExample = ?? */
	rend.pDrawable = (DrawablePtr)NULL;
	rend.drawableId = 0;
	rend.curPath = puCreateList(DD_ELEMENT_REF);
	if ( !rend.curPath)
		return(BadAlloc);

	rend.state = PEXIdle;
	rend.tablesMask = 0;
	rend.namesetsMask = 0;
	rend.attrsMask = 0;
	rend.tablesChanges = 0;
	rend.namesetsChanges = 0;
	rend.attrsChanges = 0;

	rend.lut[PEXMarkerBundleLUT] = 0;
	rend.lut[PEXTextBundleLUT] = 0;
	rend.lut[PEXLineBundleLUT] = 0;
	rend.lut[PEXInteriorBundleLUT] = 0;
	rend.lut[PEXEdgeBundleLUT] = 0;
	rend.lut[PEXViewLUT] = 0;
	rend.lut[PEXColourLUT] = 0;
	rend.lut[PEXDepthCueLUT] = 0;
	rend.lut[PEXLightLUT] = 0;
	rend.lut[PEXColourApproxLUT] = 0;
	rend.lut[PEXPatternLUT] = 0;
	rend.lut[PEXTextFontLUT] = 0;

        rend.ns[(unsigned)DD_HIGH_INCL_NS] = 0;
        rend.ns[(unsigned)DD_HIGH_EXCL_NS] = 0;
        rend.ns[(unsigned)DD_INVIS_INCL_NS] = 0;
        rend.ns[(unsigned)DD_INVIS_EXCL_NS] = 0;

	rend.hlhsrMode = PEXHlhsrOff;
	rend.npcSubvolume.minval.x = 0.0;
	rend.npcSubvolume.minval.y = 0.0;
	rend.npcSubvolume.minval.z = 0.0;
	rend.npcSubvolume.maxval.x = 1.0;
	rend.npcSubvolume.maxval.y = 1.0;
	rend.npcSubvolume.maxval.z = 1.0;

	/* can't use drawable, it doesn't exist. Is viewport needed??
	 * what viewport to use?   default is to use drawable
	 */
        rend.viewport.useDrawable = 0;
        rend.viewport.minval.x = 0;
        rend.viewport.minval.y = 0;
        rend.viewport.minval.z = 0.0;
        rend.viewport.maxval.x = 1;
        rend.viewport.maxval.y = 1;
        rend.viewport.maxval.z = 1.0;
	rend.clipList = puCreateList( DD_DEVICE_RECT );
	if ( !rend.clipList ) {
		puDeleteList( rend.curPath );
		return( BadAlloc );
	}

	rend.immediateMode = FALSE;
	/* InitRenderer does some stuff not needed for Searching
	 * and since some of that stuff uses a drawable (and searching
	 * doesn't have one), a work-aroundis put in around the drawable
	 * code.  * It may be good to sometime reevaluate this and find
	 * another way  to deal with it.
	 */
	err = InitRenderer( &rend );
	if ( err != Success ) {
		puDeleteList( rend.curPath );
		puDeleteList( rend.clipList );
		return( err );
	}

	BeginSearching(&rend, pSC);

	BeginStructure(&rend, pstr->id);

	/* always start at the first element in the structure */
	err = traverser(&rend, pstr, (ddULONG) 1,
			MISTR_NUM_EL((miStructPtr) pstr->deviceData),
			(diPMHandle) NULL, pSC, &trav_state);

	EndStructure(&rend);

	/* turn off searching */
	EndSearching(&rend);

	if (pSC->status == PEXFound) {
	    pBuffer->dataSize = pSC->startPath->numObj * sizeof(pexElementRef);
	    PU_CHECK_BUFFER_SIZE(pBuffer, pBuffer->dataSize);
	    pb = (pexElementRef *)pBuffer->pBuf;
	    pr = (ddElementRef *)pSC->startPath->pList;
	    for (i=pSC->startPath->numObj; i>0; i--, pb++, pr++) {
		pb->structure = pr->structure->id;
		pb->offset = pr->offset;
	    }
	    *pNumRefs = pSC->startPath->numObj;
	} else
		/* set start path to "not found" */
		pSC->startPath->numObj = 0;
    } else
	return (PEXERR(PEXPathError));

    return (Success);
}				/* SearchNetwork */
