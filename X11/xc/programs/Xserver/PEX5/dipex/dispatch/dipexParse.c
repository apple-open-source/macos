/* $Xorg: dipexParse.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */

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


#include "X.h"
#include "PEXproto.h"
#include "Xproto.h"
#include "misc.h"
#include "dixstruct.h"
#include "dix.h"
#include "pexError.h"
#include "pexSwap.h"


extern RequestFunction set_tables[];
extern pexContext * InitPexClient();

/*++    pexErrorHandler
 --*/

ErrorCode
pexErrorHandler(client, err)
    ClientPtr client;
    ErrorCode	err;
{
    ErrorF( "PEX Error %d Detected.  Continuing...\n", err);
/*
 * The following code is snitched from Dispatch
    if (client->noClientException != Success)
	CloseDownClient(client);
    else
	Oops(client, 0, err);
*/
/*
 * used to do this	
 */
    return(err);
 
} /* end-pexErrorHandler() */


/*++    ProcPEXDispatch
 */

ProcPEXDispatch( client )
ClientPtr client;
{
    XID		pexId;
    pexContext	*cntxtPtr;
    CARD8	op;
    ErrorCode  err = Success;

    REQUEST( xReq );

    pexId = PEXID( client, PEXCONTEXTTABLE );
    cntxtPtr = (pexContext *)LookupIDByType(pexId, PEXContextType);
      
    if( !cntxtPtr ) {

	if (!(cntxtPtr = InitPexClient(client))) return (BadAlloc);

    }

    op  = ((pexReq *)stuff)->opcode;

    if ((op >= PEX_GetExtensionInfo) && (op <= PEXMaxRequest)) {

	if (!(err = set_tables[op](cntxtPtr, stuff))) {
	    cntxtPtr->current_req = (pexReq *)stuff;
	    err = cntxtPtr->pexRequest[ op ]( cntxtPtr, stuff ); }

    } else {
	err = BadRequest;
    }

    return( err );

}


/*++	PEXRequestUnused -- stub for unimplemented requests
 --*/
ErrorCode
PEXRequestUnused(context)
pexContext      	*context;
{
ErrorCode	err = PEX_ERROR_CODE(BadRequest);
return ( err );

} /* end-PEXRequestUnused */
