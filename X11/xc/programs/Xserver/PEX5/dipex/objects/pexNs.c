/* $Xorg: pexNs.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexNs.c,v 3.8 2001/12/14 19:57:43 dawes Exp $ */


#include "X.h"
#include "Xproto.h"
#include "pexError.h"
#include "PEXproto.h"
#include "dipex.h"
#include "pexLookup.h"
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif



/*++	PEXCreateNameSet
 --*/
ErrorCode
PEXCreateNameSet (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCreateNameSetReq     *strmPtr;
{
    ErrorCode err = Success;
    ErrorCode FreeNameSet (), CreateNameSet ();
    diNSHandle nsptr;

    if (!LegalNewID(strmPtr->id, cntxtPtr->client))
	PEX_ERR_EXIT(BadIDChoice,strmPtr->id,cntxtPtr);


    nsptr = (diNSHandle) xalloc ((unsigned long)sizeof(ddNSResource));
    if (!nsptr) PEX_ERR_EXIT(BadAlloc,0,cntxtPtr);
    nsptr->id = strmPtr->id;
    err = CreateNameSet(nsptr);
    if (err) xfree((pointer)nsptr);

    ADDRESOURCE(strmPtr->id, PEXNameType, nsptr);

    return( err );

} /* end-PEXCreateNameSet() */

/*++	PEXCopyNameSet
 --*/
ErrorCode
PEXCopyNameSet (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexCopyNameSetReq       *strmPtr;
{
    ErrorCode err = Success;
    diNSHandle nsrc = 0, ndst = 0;

    LU_NAMESET(strmPtr->src, nsrc);
    LU_NAMESET(strmPtr->dst, ndst);

    err = CopyNameSet (nsrc, ndst);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXCopyNameSet() */

/*++	PEXFreeNameSet
 --*/
ErrorCode
PEXFreeNameSet (cntxtPtr, strmPtr)
pexContext              *cntxtPtr;
pexFreeNameSetReq       *strmPtr;
{
    ErrorCode err = Success;
    diNSHandle pns = 0;

    if ((strmPtr == NULL) || (strmPtr->id == 0)) {
	err = PEX_ERROR_CODE(PEXNameSetError);
	PEX_ERR_EXIT(err,0,cntxtPtr);
    }

    LU_NAMESET(strmPtr->id, pns);

    FreeResource(strmPtr->id, RT_NONE);

    return(err);

} /* end-PEXFreeNameSet() */

/*++	PEXGetNameSet
 --*/
ErrorCode
PEXGetNameSet( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexGetNameSetReq    	*strmPtr;
{
    ErrorCode err = Success;
    diNSHandle pns;
    extern ddBufferPtr pPEXBuffer;
    CARD32 numNames;

    LU_NAMESET(strmPtr->id, pns);

    SETUP_INQ(pexGetNameSetReply);

    err = InquireNameSet (pns, &numNames, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetNameSetReply);
	reply->numNames = numNames;
	WritePEXBufferReply(pexGetNameSetReply);
    }
    return( err );

} /* end-PEXGetNameSet() */

/*++	PEXChangeNameSet
 --*/
ErrorCode
PEXChangeNameSet( cntxtPtr, strmPtr )
pexContext      	*cntxtPtr;
pexChangeNameSetReq    	*strmPtr;
{
    ErrorCode err = Success;
    diNSHandle pns = 0;

    LU_NAMESET(strmPtr->ns, pns);

    err = ChangeNameSet(    pns, strmPtr->action, 
			    (strmPtr->length - (sizeof(pexChangeNameSetReq)
				/ sizeof(pexNameSet))),
			    (ddULONG *)(strmPtr + 1));
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
    return( err );

} /* end-PEXChangeNameSet() */
/*++
 *
 * 	End of File
 --*/
