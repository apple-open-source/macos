/* $Xorg: miDestroy.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */

/***********************************************************

Copyright 1990, 1991, 1998  The Open Group

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

Copyright 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/


#include "X.h"
#include "ddpex.h"
#include "ddpex3.h"
#include "PEX.h"
#include "PEXproto.h"
#include "pexExtract.h"
#include "ddpex2.h"
#include "miStruct.h"
#include "pexUtils.h"


/**  This file contains the definition for the OC Destroy Functions,
 ** each of which takes one parameter: a pointer to the element to be 
 ** destroyed (in server native internal format).
 ****Note that these functions may be replaced by PEX server porters.
 **/

#if (defined(__STDC__) && !defined(UNIXCPP)) || defined(ANSICPP)
#define CAT(a,b)    a##b
#else
#define CAT(a,b)    a/**/b
#endif	/* __STDC__ */

#define OC_DESTROY_FUNC_HEADER(suffix)   \
    void CAT(destroy,suffix)(pExecuteOC) \
    miGenericElementPtr	     pExecuteOC;	/* internal format */ 
    

/* nothing to delete */
OC_DESTROY_FUNC_HEADER(NoOp)
{
	return;
}

/* Most OC's are allocated in one hunk */
OC_DESTROY_FUNC_HEADER(OC_PEX)
{
    xfree(pExecuteOC);		/* was allocated in one hunk */
    return;
}

/* others are more chewy */
OC_DESTROY_FUNC_HEADER(NurbSurface)
{
    miNurbSurfaceStruct *ddNurb = (miNurbSurfaceStruct *)(pExecuteOC + 1);
    listofTrimCurve	*ddTrim;
    ddTrimCurve		*ddTC;
    ddULONG		i, j;

    if (!pExecuteOC) return;

    for (   i=0, ddTrim = ddNurb->trimCurves;
	    i < ddNurb->numTrimCurveLists;
	    i++, ddTrim++ ) {
	if (ddTrim->pTC) {
	    for (   j=0, ddTC = ddTrim->pTC;
		    j < ddTrim->count;
		    j++, ddTC++ ) {
		if (ddTC->pKnots)	    xfree(ddTC->pKnots);
		if (ddTC->points.pts.ptr)   xfree(ddTC->points.pts.ptr);
		else break;
	    }
	    xfree(ddTrim->pTC);
	} else break;
    }

    xfree(pExecuteOC);

    return;
}

/* and sofas have a LOT of stuffing */
OC_DESTROY_FUNC_HEADER(SOFAS)
{
    ddUSHORT i,j;
    miSOFASStruct *ddFill = (miSOFASStruct *)(pExecuteOC + 1);
    miConnListList *pCLL;
    miConnList *pCList;

    if (!pExecuteOC) return;

    for (   i=0, pCLL = ddFill->connects.data;
	    i<ddFill->connects.numListLists;
	    i++, pCLL++) {
	if (pCLL->pConnLists) {
	    for (   j=0, pCList=(miConnList *)(pCLL->pConnLists);
		    j<pCLL->numLists;
		    j++, pCList++) {
		if (pCList->pConnects) xfree(pCList->pConnects);
		else break;
	    }
	} else break;
	xfree(pCLL->pConnLists);
    }
    xfree(pExecuteOC);
}
