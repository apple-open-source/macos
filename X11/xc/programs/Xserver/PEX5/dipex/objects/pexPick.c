/* $Xorg: pexPick.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems,
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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexPick.c,v 3.8 2001/12/14 19:57:44 dawes Exp $ */


/*++
 *  --- Workstation Picking ---
 *
 *	PEXCreatePickMeasure
 *	PEXFreePickMeasure
 *	PEXGetPickDevice
 *	PEXChangePickDevice
 *	PEXGetPickMeasure
 *	PEXUpdatePickMeasure
 *
 --*/

#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "dipex.h"
#include "PEXprotost.h"
#include "pex_site.h"
#include "ddpex4.h"
#include "pexLookup.h"
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif

typedef struct {
    pexElementRef   *path;
    pexNameSet	    inclusion;
    pexNameSet	    exclusion;
    long	    PET;
    pexViewport	    viewport;
    pexSwitch	    status;
    pexSwitch	    order;
    pexSwitch	    echo_switch;
    unsigned char   *record;
} dipickdev;


/*++	PEXCreatePickMeasure
 --*/
ErrorCode
PEXCreatePickMeasure (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreatePickMeasureReq *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode freePickMeasure();
    diPMHandle pmh;
    dipexPhigsWks *pw = 0;
    extern ErrorCode FreePickMeasure();

    pmh = (diPMHandle)LookupIDByType(strmPtr->pm, PEXPickType);
    if (pmh) PEX_ERR_EXIT(BadIDChoice,strmPtr->pm,cntxtPtr);

    pmh = (diPMHandle) xalloc ((unsigned long)sizeof(ddPMResource));
    if (!pmh) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);

    LU_PHIGSWKS (strmPtr->wks, pw);

    pmh->id = strmPtr->pm;
    err = CreatePickMeasure ((diWKSHandle)pw, strmPtr->devType, pmh);
    if (err){
	xfree((pointer)pmh);
	PEX_ERR_EXIT(err,0,cntxtPtr);	
    }

    ADDRESOURCE(strmPtr->pm, PEXPickType, pmh);

    return( err );

} /* end-PEXCreatePickMeasure() */

/*++	PEXFreePickMeasure
 --*/
ErrorCode
PEXFreePickMeasure (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexFreePickMeasureReq   *strmPtr;
{
    diPMHandle pmh = 0;
    ErrorCode err = Success;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXPickMeasureError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_PICKMEASURE (strmPtr->id, pmh);

    FreeResource(strmPtr->id, RT_NONE);

    return( err );

} /* end-PEXFreePickMeasure() */

/*++	PEXGetPickDevice
 --*/

ErrorCode
PEXGetPickDevice( cntxtPtr, strmPtr )
pexContext     		*cntxtPtr;
pexGetPickDeviceReq    	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    extern ddBufferPtr pPEXBuffer;
    CARD32 numItems;

    LU_PHIGSWKS(strmPtr->wks, pw);
    CHECK_FP_FORMAT(strmPtr->fpFormat);

    SETUP_INQ(pexGetPickDeviceReply);

    err = InquirePickDevice((diWKSHandle)pw, strmPtr->devType, strmPtr->itemMask,
			    &numItems, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetPickDeviceReply);
	WritePEXBufferReply(pexGetPickDeviceReply);
    }
    return( err );

} /* end-PEXGetPickDevice() */

/*++	PEXChangePickDevice
 --*/

ErrorCode
PEXChangePickDevice( cntxtPtr, strmPtr )
pexContext     		*cntxtPtr;
pexChangePickDeviceReq	*strmPtr;
{
    ErrorCode err = Success;
    dipexPhigsWks *pw = 0;
    CARD32 *ptr = (CARD32 *)(strmPtr+1);

    LU_PHIGSWKS(strmPtr->wks, pw);
    CHECK_FP_FORMAT(strmPtr->fpFormat);

    if (strmPtr->itemMask & PEXPDPickStatus) ptr++;

    if (strmPtr->itemMask & PEXPDPickPath) {
	pexPickElementRef *per;
	diStructHandle sh, *psh;
	CARD32 i, numRefs = *((CARD32 *)(ptr));
	ptr++;
	for (i=0, per = (pexPickElementRef *)ptr; i<numRefs; i++, per++) {
		LU_STRUCTURE(per->sid,sh);
		psh = (diStructHandle *)&(per->sid);
		*psh = sh;
	}
	ptr = (CARD32 *)per;
    }

    if (strmPtr->itemMask & PEXPDPickPathOrder) ptr++;

    if (strmPtr->itemMask & PEXPDPickIncl) {
	diNSHandle temp;
    	LU_NAMESET(*ptr, temp);
	*ptr = (CARD32)temp;
	ptr++;
    }

    if (strmPtr->itemMask & PEXPDPickExcl) {
	diNSHandle temp;
    	LU_NAMESET(*ptr, temp);
	*ptr = (CARD32)temp;
    }

    err = ChangePickDevice( (diWKSHandle)pw, strmPtr->devType, strmPtr->itemMask,
			    (ddPointer)(strmPtr + 1));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangePickDevice() */

/*++	PEXGetPickMeasure
 --*/

ErrorCode
PEXGetPickMeasure( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetPickMeasureReq    *strmPtr;
{
    ErrorCode err = Success;
    diPMHandle pmh;
    extern ddBufferPtr pPEXBuffer;
    CARD32 numItems;

    LU_PICKMEASURE (strmPtr->pm, pmh);

    SETUP_INQ(pexGetPickMeasureReply);

    err = InquirePickMeasure(	pmh, strmPtr->itemMask,
				&numItems, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetPickMeasureReply);
	WritePEXBufferReply(pexGetPickMeasureReply);
    }
    return( err );

} /* end-PEXGetPickMeasure() */

/*++	PEXUpdatePickMeasure
 --*/

ErrorCode
PEXUpdatePickMeasure( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexUpdatePickMeasureReq *strmPtr;
{
    ErrorCode err = Success;
    diPMHandle pmh;

    LU_PICKMEASURE (strmPtr->pm, pmh);

    err = UpdatePickMeasure (pmh, strmPtr->numBytes, (ddPointer)(strmPtr + 1));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXUpdatePickMeasure() */


/*++
 *
 *	End of File
 *
 --*/
