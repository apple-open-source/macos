/* $Xorg: pl_nameset.c,v 1.4 2001/02/09 02:03:28 xorgcvs Exp $ */

/******************************************************************************

Copyright 1992, 1998  The Open Group

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


Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital not be used in advertising or publicity
pertaining to distribution of the software without specific, written prior
permission.  Digital make no representations about the suitability
of this software for any purpose.  It is provided "as is" without express or
implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"


PEXNameSet
PEXCreateNameSet (display)

INPUT Display		*display;

{
    register pexCreateNameSetReq	*req;
    char				*pBuf;
    PEXNameSet				ns;


    /*
     * Get a nameset resource id from X.
     */

    ns = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreateNameSet, pBuf);

    BEGIN_REQUEST_HEADER (CreateNameSet, pBuf, req);

    PEXStoreReqHead (CreateNameSet, req);
    req->id = ns;

    END_REQUEST_HEADER (CreateNameSet, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (ns);
}


void
PEXFreeNameSet (display, ns)

INPUT Display		*display;
INPUT PEXNameSet	ns;

{
    register pexFreeNameSetReq	*req;
    char			*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreeNameSet, pBuf);

    BEGIN_REQUEST_HEADER (FreeNameSet, pBuf, req);

    PEXStoreReqHead (FreeNameSet, req);
    req->id = ns;

    END_REQUEST_HEADER (FreeNameSet, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopyNameSet (display, srcNs, destNs)

INPUT Display		*display;
INPUT PEXNameSet	srcNs;
INPUT PEXNameSet	destNs;

{
    register pexCopyNameSetReq	*req;
    char			*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopyNameSet, pBuf);

    BEGIN_REQUEST_HEADER (CopyNameSet, pBuf, req);

    PEXStoreReqHead (CopyNameSet, req);
    req->src = srcNs;
    req->dst = destNs;

    END_REQUEST_HEADER (CopyNameSet, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXGetNameSet (display, ns, numNamesReturn, namesReturn)

INPUT Display		*display;
INPUT PEXNameSet	ns;
OUTPUT unsigned long	*numNamesReturn;
OUTPUT PEXName		**namesReturn;

{
    register pexGetNameSetReq	*req;
    char			*pBuf;
    pexGetNameSetReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetNameSet, pBuf);

    BEGIN_REQUEST_HEADER (GetNameSet, pBuf, req);

    PEXStoreReqHead (GetNameSet, req);
    req->id = ns;

    END_REQUEST_HEADER (GetNameSet, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*numNamesReturn = 0;
	*namesReturn = NULL;
	return (0); 		/* return an error */
    }

    *numNamesReturn = rep.numNames;


    /*
     * Allocate a buffer for the replies to pass back to the user.
     */

    *namesReturn = (PEXName *) Xmalloc (
	(unsigned) (sizeof (PEXName) * rep.numNames));

    XREAD_LISTOF_CARD32 (display, rep.numNames, *namesReturn);


   /*
    * Done, so unlock and check for synchronous-ness.
    */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


void
PEXChangeNameSet (display, ns, action, numValues, values)

INPUT Display		*display;
INPUT PEXNameSet	ns;
INPUT int		action;
INPUT unsigned long	numValues;
INPUT PEXName		*values;

{
    register pexChangeNameSetReq	*req;
    char				*pBuf;
    int					size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    size = numValues * SIZEOF (pexName);
    PEXGetReqExtra (ChangeNameSet, size, pBuf);

    BEGIN_REQUEST_HEADER (ChangeNameSet, pBuf, req);

    PEXStoreReqExtraHead (ChangeNameSet, size, req);
    req->ns = ns;
    req->action = action;

    END_REQUEST_HEADER (ChangeNameSet, pBuf, req);

    STORE_LISTOF_CARD32 (numValues, values, pBuf);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}
