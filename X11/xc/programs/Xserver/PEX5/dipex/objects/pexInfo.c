/* $Xorg: pexInfo.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/objects/pexInfo.c,v 1.9 2001/12/14 19:57:43 dawes Exp $ */


/*++    pexInfo.c
 *
 *	Contents:   PEXGetExtensionInfo
 *		    PEXGetEnumeratedTypeInfo
 *		    PEXGetImpDepConstants
 *		    PEXMatchRendererTargets
 *		    PEXEscape
 *		    PEXEscapeWithReply
 *
 --*/

#include "X.h"
#include "Xproto.h"
#include "pex_site.h"
#include "PEX.h"
#include "dipex.h"
#include "pexError.h"
#include "pexLookup.h"
#include "pexExtract.h"
#include "pexos.h"

#ifdef min
#undef min
#endif
 
#ifdef max
#undef max
#endif


/*++	PEXGetExtensionInfo
 *
 * The client_protocol_major_version and the 
 * client_protocol_minor_version indicate what 
 * version of the protocol the client expects the 
 * server to implement. 
 --*/

ErrorCode
PEXGetExtensionInfo( cntxtPtr, strmPtr )
pexContext		*cntxtPtr;
pexGetExtensionInfoReq	*strmPtr;
{
    ErrorCode err = Success;
    extern ddBuffer *pPEXBuffer;
    pexGetExtensionInfoReply *reply =
				   (pexGetExtensionInfoReply *)pPEXBuffer->pHead;

    reply->majorVersion = PEX_PROTO_MAJOR;
    reply->minorVersion = PEX_PROTO_MINOR;
    reply->release = PEX_RELEASE_NUMBER;
    reply->lengthName = strlen(PEX_VENDOR);
    reply->length = LWORDS(reply->lengthName);
    reply->subsetInfo = PEX_SUBSET;
    memcpy( (char *)(reply+1), PEX_VENDOR, (int)(reply->lengthName));

    WritePEXReplyToClient(  cntxtPtr, strmPtr,
			    sizeof(pexGetExtensionInfoReply) + reply->lengthName,
			    reply);

    return( err );

} /* end-PEXGetExtensionInfo() */

/*++	PEXGetEnumeratedTypeInfo
 --*/

ErrorCode
PEXGetEnumeratedTypeInfo( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexGetEnumeratedTypeInfoReq	*strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr d;
    extern ddBuffer *pPEXBuffer;
    ddULONG numLists;

    LU_DRAWABLE (strmPtr->drawable, d);

    SETUP_INQ(pexGetEnumeratedTypeInfoReply);

    err = InquireEnumTypeInfo(	d, strmPtr->itemMask, strmPtr->numEnums,
				(ddUSHORT *)(strmPtr+1), &numLists, pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);
			
    {
	SETUP_VAR_REPLY(pexGetEnumeratedTypeInfoReply);
	reply->numLists = numLists;
	WritePEXBufferReply(pexGetEnumeratedTypeInfoReply);
    }
    return( err );

} /* end-PEXGetEnumeratedTypeInfo() */

/*++	PEXGetImpDepConstants
 --*/

ErrorCode
PEXGetImpDepConstants( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexGetImpDepConstantsReq	*strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr d;
    extern ddBuffer *pPEXBuffer;

    LU_DRAWABLE (strmPtr->drawable, d);

    SETUP_INQ(pexGetImpDepConstantsReply);

    err = InquireImpDepConstants(   d, strmPtr->numNames, 
				    (ddUSHORT *)(strmPtr+1), pPEXBuffer);
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexGetImpDepConstantsReply);
	WritePEXBufferReply(pexGetImpDepConstantsReply);
    }
    return( err );

} /* end-PEXGetImpDepConstants() */

ErrorCode
PEXMatchRendererTargets( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexMatchRendererTargetsReq      *strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr d;
    extern ddBuffer *pPEXBuffer;

    LU_DRAWABLE (strmPtr->drawable, d);

    /* no way to check visualID besides doing the work of Match */

    SETUP_INQ(pexMatchRendererTargetsReply);

    err = MatchRendererTargets(d, (int)strmPtr->depth, (int)strmPtr->type, 
			   (VisualID)strmPtr->visualID,
			   (int)strmPtr->maxTriplets, pPEXBuffer );
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexMatchRendererTargetsReply);
	WritePEXBufferReply(pexMatchRendererTargetsReply);
    }
    return( err );

} /* end-PEXMatchRendererTargets() */

ErrorCode
PEXQueryColorApprox( cntxtPtr, strmPtr )
pexContext			*cntxtPtr;
pexQueryColorApproxReq      	*strmPtr;
{
    ErrorCode err = Success;
    DrawablePtr d;
    extern ddBuffer *pPEXBuffer;

    LU_DRAWABLE (strmPtr->drawable, d);

    SETUP_INQ(pexQueryColorApproxReply);

  /*
	Call to query color approximation routine goes here
  */
    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    {
	SETUP_VAR_REPLY(pexQueryColorApproxReply);
	WritePEXBufferReply(pexQueryColorApproxReply);
    }
    return( err );

} /* end-PEXQueryColorApprox() */


ErrorCode
PEXEscape( cntxtPtr, strmPtr )
pexContext		  	*cntxtPtr;
pexEscapeReq	   	 	*strmPtr;
{
    ErrorCode err = Success;
    ddRendererStr *prend = 0;
    pexEscapeSetEchoColourData *ptr;
    CARD8 *pcs;


    /* Support the one Registered Escape, Set Echo Color */
    switch (strmPtr->escapeID) {
	case  PEXEscapeSetEchoColour: {
	  ptr = (pexEscapeSetEchoColourData *)(strmPtr + 1);
	  pcs = (CARD8 *)(ptr+1); 

	  LU_RENDERER(ptr->rdr, prend);
	  EXTRACT_COLOUR_SPECIFIER(prend->echoColour,pcs);
	  break;
	}

	default: 
	  err = BadValue;
	  break;
    }


    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    return( err );

} /* end-PEXEscape() */


ErrorCode
PEXEscapeWithReply( cntxtPtr, strmPtr )
pexContext		  	*cntxtPtr;
pexEscapeWithReplyReq	   	*strmPtr;
{
    ErrorCode err = Success;

    /* Do nothing here, Escape with Reply is not implemented in SI 
    */

    if (err) PEX_ERR_EXIT(err,0,cntxtPtr);

    return( err );

} /* end-PEXEscapeWithReply() */
/*++
 *
 *	End of File
 *
 --*/
