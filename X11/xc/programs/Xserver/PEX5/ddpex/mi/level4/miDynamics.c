/* $Xorg: miDynamics.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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

#include "ddpex.h"
#include "ddpex4.h"
#include "pexUtils.h"
#include "miWks.h"
#include "miStruct.h"
#include "miNS.h"


/*++
 |
 |  Function Name:	miDealWithDynamics
 |
 |  Function Description:
 |	Goes through each workstation in the given list and causes a Redraw 
 |  of the workstation depending on the supplied dynamic and other values in 
 |  the workstation state.
 |
 --*/


ddpex4rtn
miDealWithDynamics(dynamic, pwksToLookAt)
/* in */
    ddDynamic       dynamic;
    listofObj      *pwksToLookAt;
/* out */
{
    register int    i;
    diWKSHandle    *pWKS;
    miWksPtr        wksPtr;
    ddpex4rtn       err4 = Success;

    pWKS = (diWKSHandle *) pwksToLookAt->pList;
    for (i = 0; i < pwksToLookAt->numObj; i++, pWKS++) {

	wksPtr = (miWksPtr) ((*pWKS)->deviceData);

	if (!wksPtr) continue;		/* workstation has been freed */

	switch (wksPtr->displayUpdate) {
	    case PEXVisualizeEach:
		/* always make sure picture is correct */
		if (	(wksPtr->dynamics[(int) dynamic] != PEXIMM)
		     || (wksPtr->visualState != PEXCorrect)) {
		    if (err4 = RedrawStructures(*pWKS))	return (err4);
		    wksPtr->visualState = PEXCorrect;
		}
		break;

	    case PEXVisualizeEasy:
	    case PEXVisualizeNone:
	    case PEXVisualizeWhenever:
		/*
		 * for PEXIMM don't do anything (visual state doesn't change)
		 */
		if (wksPtr->dynamics[(int) dynamic] != PEXIMM)
		    /* IRG and CBS, the action is deferred */
		    wksPtr->visualState = PEXDeferred;
		break;

	    case PEXSimulateSome:

		/*
		 * don't do anything if dynamic = PEXIMM (visual
		 * state doesn't change)
		 */
		if (wksPtr->dynamics[(int) dynamic] == PEXIRG)
		    /* the action is deferred */
		    wksPtr->visualState = PEXDeferred;
		else if (wksPtr->dynamics[(int) dynamic] == PEXCBS)

		    /*
		     * the action should not be deferred and the
		     * simulation should be done when the action
		     * is done
		     */
		    if (wksPtr->visualState != PEXDeferred)
			wksPtr->visualState = PEXSimulated;
		break;
	}
    }
    return (Success);
}				/* miDealWithDynamics */

/*++
 |
 |  Function Name:	miDealWithStructDynamics
 |
 |  Function Description:
 |	Goes through each workstation that the structure appears on,
 |    and causes a Redraw of the workstation depending on the supplied
 |    dynamic and other values in the workstation state.
 |
 --*/

ddpex4rtn
miDealWithStructDynamics(dynamic, pStruct)
/* in */
    ddDynamic       dynamic;
    diStructHandle  pStruct;
/* out */
{
    miStructPtr     pstruct = (miStructPtr) pStruct->deviceData;
    listofObj      *pwksToLookAt;
    ddpex4rtn       err = Success;

    /** Build up a list of workstations from the PostedTo and AppearOn
     ** lists in the structure structure.  They are inserted in such a
     ** manner that duplicates between the lists are eliminated.
     **/

    if (pstruct->wksPostedTo->numObj || pstruct->wksAppearOn->numObj) {
	pwksToLookAt = puCreateList(DD_WKS);
	if (!pwksToLookAt) return (BadAlloc);
	err = puMergeLists( pstruct->wksPostedTo, pstruct->wksAppearOn,
			    pwksToLookAt);

	err = miDealWithDynamics(dynamic, pwksToLookAt);
	puDeleteList(pwksToLookAt);
    }
    return (err);
}				/* miDealWithStructDynamics */


/*++
 |
 |  Function Name:	miDealWithNSDynamics
 |
 |  Function Description:
 |	Goes through each workstation that uses the name set
 |    and causes a Redraw of the workstation depending on the supplied
 |    dynamic and other values in the workstation state.
 |
 --*/

ddpex43rtn
miDealWithNSDynamics(pNS)
/* in */
    diNSHandle      pNS;
/* out */
{
    miNSHeader     *pns = (miNSHeader *) pNS->deviceData;
    listofObj      *pwksToLookAt;
    diWKSHandle    *pWks;
    miWksPtr        pwks;
    register int    i;
    ddpex43rtn      err = Success;

    if (!pns->wksRefList->numObj) return (Success);

    pwksToLookAt = puCreateList(DD_WKS);
    if (!pwksToLookAt) return (BadAlloc);

    pWks = (diWKSHandle *) pns->wksRefList->pList;
    for (i = 0; i < pns->wksRefList->numObj; i++, pWks++) {
	pwks = (miWksPtr) (*pWks)->deviceData;
	if (	(pwks->pRend->ns[(int) DD_HIGH_INCL_NS] == pNS)
	     || (pwks->pRend->ns[(int) DD_HIGH_EXCL_NS] == pNS))
	    err = puAddToList((ddPointer) pWks, (ddULONG) 1, pwksToLookAt);
    }

    err = miDealWithDynamics(HIGH_FILTER_DYNAMIC, pwksToLookAt);
    if (err == Success) {
	pWks = (diWKSHandle *) pns->wksRefList->pList;
	for (i = 0; i < pns->wksRefList->numObj; i++, pWks++) {
	    pwks = (miWksPtr) (*pWks)->deviceData;
	    if (    (pwks->pRend->ns[(int) DD_INVIS_INCL_NS] == pNS)
		 || (pwks->pRend->ns[(int) DD_INVIS_EXCL_NS] == pNS))
		err = puAddToList((ddPointer) pWks, (ddULONG) 1, pwksToLookAt);
	    }

	err = miDealWithDynamics(INVIS_FILTER_DYNAMIC, pwksToLookAt);
    }
    puDeleteList(pwksToLookAt);

    return (err);
}
